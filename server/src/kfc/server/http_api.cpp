#include "kfc/server/http_api.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

#include <ixwebsocket/IXConnectionState.h>
#include <ixwebsocket/IXHttpServer.h>
#include <ixwebsocket/IXNetSystem.h>
#include <nlohmann/json.hpp>

#include "kfc/database/user_repository.hpp"
#include "kfc/protocol/file_logger.hpp"
#include "kfc/server/auth_token_store.hpp"
#include "kfc/server/metrics.hpp"
#include "kfc/server/rate_limiter.hpp"
#include "kfc/server/room_manager.hpp"
#include "kfc/server/session_registry.hpp"
#include "kfc/server/websocket_game_server.hpp"  // kTcpBacklog/kMaxConnections

namespace kfc::server {

namespace {

using nlohmann::json;

constexpr std::string_view kHistoryPrefix = "/api/history/";

// ixwebsocket's HttpServer has no request-body-size option and always reads
// the full body before this callback runs; this only stops an oversized body
// from being parsed and handed to UserRepository.
constexpr std::size_t kMaxHttpBodyBytes = 64 * 1024;

std::string reason_phrase(int status) {
    switch (status) {
        case 200:
            return "OK";
        case 201:
            return "Created";
        case 400:
            return "Bad Request";
        case 401:
            return "Unauthorized";
        case 403:
            return "Forbidden";
        case 404:
            return "Not Found";
        case 409:
            return "Conflict";
        case 413:
            return "Payload Too Large";
        case 429:
            return "Too Many Requests";
        case 503:
            return "Service Unavailable";
        default:
            return "";
    }
}

ix::HttpResponsePtr json_response(int status, const json& body) {
    ix::WebSocketHttpHeaders headers{{"Content-Type", "application/json"}};
    return std::make_shared<ix::HttpResponse>(status, reason_phrase(status), ix::HttpErrorCode::Ok, headers,
                                              body.dump());
}

ix::HttpResponsePtr empty_response(int status) {
    ix::WebSocketHttpHeaders headers{{"Content-Type", "application/json"}};
    return std::make_shared<ix::HttpResponse>(status, reason_phrase(status), ix::HttpErrorCode::Ok, headers,
                                              std::string());
}

// Prometheus text exposition format, not JSON.
ix::HttpResponsePtr text_response(const std::string& body) {
    ix::WebSocketHttpHeaders headers{{"Content-Type", "text/plain; version=0.0.4"}};
    return std::make_shared<ix::HttpResponse>(200, "OK", ix::HttpErrorCode::Ok, headers, body);
}

json auth_body(const std::string& username, int rating, const std::string& token) {
    return json{{"username", username}, {"rating", rating}, {"token", token}};
}

ix::HttpResponsePtr reason_response(int status, const std::string& reason) {
    return json_response(status, json{{"reason", reason}});
}

ix::HttpResponsePtr handle_register(kfc::database::UserRepository& users, AuthTokenStore& tokens,
                                    const json& request) {
    std::string username = request.at("username").get<std::string>();
    std::string password = request.at("password").get<std::string>();
    // authenticate() is one atomic lookup-or-insert (not user_exists() then
    // authenticate()), avoiding a race where two concurrent registrations of
    // the same new username both insert and one throws on the PRIMARY KEY.
    kfc::database::IUserStore::AuthOutcome auth = users.authenticate(username, password);
    if (auth.newly_registered) {
        return json_response(201, auth_body(username, auth.rating, tokens.issue(username)));
    }
    if (auth.reason == "invalid_username" || auth.reason == "weak_password") {
        return reason_response(400, auth.reason);
    }
    return empty_response(409);
}

ix::HttpResponsePtr handle_login(kfc::database::UserRepository& users, AuthTokenStore& tokens, const json& request) {
    std::string username = request.at("username").get<std::string>();
    std::string password = request.at("password").get<std::string>();
    if (!users.user_exists(username)) {
        return empty_response(401);
    }
    kfc::database::IUserStore::AuthOutcome auth = users.authenticate(username, password);
    if (!auth.ok) {
        return empty_response(401);
    }
    return json_response(200, auth_body(username, auth.rating, tokens.issue(username)));
}

std::optional<std::string> bearer_token(const ix::HttpRequestPtr& request) {
    constexpr std::string_view kPrefix = "Bearer ";
    auto it = request->headers.find("Authorization");
    if (it == request->headers.end() || it->second.rfind(kPrefix, 0) != 0) {
        return std::nullopt;
    }
    std::string token = it->second.substr(kPrefix.size());
    return token.empty() ? std::nullopt : std::optional<std::string>(std::move(token));
}

ix::HttpResponsePtr handle_history(kfc::database::UserRepository& users, AuthTokenStore& tokens,
                                   const ix::HttpRequestPtr& request, const std::string& username) {
    std::optional<std::string> token = bearer_token(request);
    if (!token.has_value()) {
        return empty_response(401);  // no credentials at all
    }
    std::optional<std::string> owner = tokens.username_for(*token);
    if (!owner.has_value()) {
        return empty_response(401);  // credentials, but not ones anyone issued
    }
    if (*owner != username) {
        return empty_response(403);
    }

    json games = json::array();
    for (const kfc::database::GameRecord& game : users.history_for(username)) {
        games.push_back(json{
            {"id", game.id},
            {"whiteUsername", game.white_username},
            {"blackUsername", game.black_username},
            {"winnerUsername", game.winner_username.has_value() ? json(*game.winner_username) : json(nullptr)},
            {"endReason", game.end_reason},
            {"startedAt", game.started_at},
            {"endedAt", game.ended_at},
        });
    }
    return json_response(200, games);  // 200 [] for an unknown username too, not a 404.
}

// Liveness check: confirms the account store is actually answering queries,
// not just that the process exists. The placeholder username never exists.
ix::HttpResponsePtr handle_health(kfc::database::UserRepository& users) {
    try {
        (void)users.rating_of("__kfc_health_check__");
    } catch (const std::exception&) {
        return empty_response(503);
    }
    return json_response(200, json{{"status", "ok"}});
}

ix::HttpResponsePtr dispatch(kfc::database::UserRepository& users, RoomManager& rooms, SessionRegistry& sessions,
                             Metrics& metrics, AuthTokenStore& tokens, RateLimiter& auth_limiter,
                             const std::string& remote_ip, const ix::HttpRequestPtr& request) {
    try {
        if (request->method == "GET" && request->uri == "/health") {
            return handle_health(users);
        }
        if (request->method == "GET" && request->uri == "/metrics") {
            return text_response(metrics.render(sessions.live_count(), rooms.room_count(), rooms.worker_count()));
        }
        bool is_register = request->method == "POST" && request->uri == "/api/auth/register";
        bool is_login = request->method == "POST" && request->uri == "/api/auth/login";
        if (is_register || is_login) {
            // Checked before the body is parsed: malformed JSON floods count too.
            if (!auth_limiter.allow(remote_ip, std::chrono::steady_clock::now())) {
                return empty_response(429);
            }
            if (request->body.size() > kMaxHttpBodyBytes) {
                return empty_response(413);
            }
            return is_register ? handle_register(users, tokens, json::parse(request->body))
                               : handle_login(users, tokens, json::parse(request->body));
        }
        if (request->method == "GET" && request->uri.rfind(kHistoryPrefix, 0) == 0) {
            return handle_history(users, tokens, request, request->uri.substr(kHistoryPrefix.size()));
        }
    } catch (const json::exception&) {
        return empty_response(400);  // malformed body, or a required field missing
    }
    return empty_response(404);
}

}  // namespace

HttpApiServer::HttpApiServer(int port, kfc::database::UserRepository& users, RoomManager& rooms,
                             SessionRegistry& sessions, Metrics& metrics, RateLimiter& auth_limiter,
                             kfc::protocol::FileLogger& logger)
    : port_(port),
      users_(users),
      rooms_(rooms),
      sessions_(sessions),
      metrics_(metrics),
      logger_(logger),
      auth_limiter_(auth_limiter) {
    // ix's init/uninit are reference-counted; paired with uninitNetSystem() below.
    ix::initNetSystem();
    server_ = std::make_unique<ix::HttpServer>(port_, "0.0.0.0", kTcpBacklog, kMaxConnections);

    server_->setOnConnectionCallback([this](ix::HttpRequestPtr request,
                                            const std::shared_ptr<ix::ConnectionState>& connection_state)
                                         -> ix::HttpResponsePtr {
        return dispatch(users_, rooms_, sessions_, metrics_, tokens_, auth_limiter_, connection_state->getRemoteIp(),
                        request);
    });
}

HttpApiServer::~HttpApiServer() {
    if (server_) {
        server_->stop();
    }
    ix::uninitNetSystem();
}

bool HttpApiServer::listen() {
    std::pair<bool, std::string> result = server_->listen();
    if (!result.first) {
        logger_.log(kfc::protocol::LogLevel::Error,
                    "Failed to listen on HTTP port " + std::to_string(port_) + ": " + result.second);
    }
    return result.first;
}

void HttpApiServer::start() {
    server_->start();
}

void HttpApiServer::wait() {
    server_->wait();
}

void HttpApiServer::stop() {
    server_->stop();
}

}  // namespace kfc::server
