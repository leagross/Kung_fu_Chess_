#include "kfc/server/http_api.hpp"

#include <string>
#include <string_view>

#include <ixwebsocket/IXConnectionState.h>
#include <ixwebsocket/IXHttpServer.h>
#include <ixwebsocket/IXNetSystem.h>
#include <nlohmann/json.hpp>

#include "kfc/database/user_repository.hpp"
#include "kfc/protocol/file_logger.hpp"

namespace kfc::server {

namespace {

using nlohmann::json;

constexpr std::string_view kHistoryPrefix = "/api/history/";

// The reason phrase HttpResponse::description ends up as (the "Created" in
// "HTTP/1.1 201 Created") -- not the content type, which is set through
// headers below. Only the codes this API actually returns need a name here.
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
        case 404:
            return "Not Found";
        case 409:
            return "Conflict";
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

// No body, but still application/json -- there is nothing to say beyond the
// status code (409/401), matching the Java api-gateway this replaces.
ix::HttpResponsePtr empty_response(int status) {
    ix::WebSocketHttpHeaders headers{{"Content-Type", "application/json"}};
    return std::make_shared<ix::HttpResponse>(status, reason_phrase(status), ix::HttpErrorCode::Ok, headers,
                                              std::string());
}

json auth_body(const std::string& username, int rating) {
    return json{{"username", username}, {"rating", rating}};
}

ix::HttpResponsePtr handle_register(kfc::database::UserRepository& users, const json& request) {
    std::string username = request.at("username").get<std::string>();
    std::string password = request.at("password").get<std::string>();
    if (users.user_exists(username)) {
        return empty_response(409);
    }
    // Unseen username: authenticate() registers it (see UserRepository).
    kfc::database::IUserStore::AuthOutcome auth = users.authenticate(username, password);
    return json_response(201, auth_body(username, auth.rating));
}

ix::HttpResponsePtr handle_login(kfc::database::UserRepository& users, const json& request) {
    std::string username = request.at("username").get<std::string>();
    std::string password = request.at("password").get<std::string>();
    if (!users.user_exists(username)) {
        // Checked separately from authenticate() so an unknown username is
        // rejected rather than silently registered -- see user_exists's doc.
        return empty_response(401);
    }
    kfc::database::IUserStore::AuthOutcome auth = users.authenticate(username, password);
    if (!auth.ok) {
        return empty_response(401);
    }
    return json_response(200, auth_body(username, auth.rating));
}

ix::HttpResponsePtr handle_history(kfc::database::UserRepository& users, const std::string& username) {
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
    return json_response(200, games);  // 200 [] for an unknown username too -- not a 404.
}

// A liveness/readiness check for a load balancer or `docker compose`
// healthcheck: not just "the process exists", but "the account store this
// process holds a connection to is actually answering queries". The
// placeholder username is never expected to exist; only whether the query
// itself succeeds is being checked.
ix::HttpResponsePtr handle_health(kfc::database::UserRepository& users) {
    try {
        (void)users.rating_of("__kfc_health_check__");
    } catch (const std::exception&) {
        return empty_response(503);
    }
    return json_response(200, json{{"status", "ok"}});
}

ix::HttpResponsePtr dispatch(kfc::database::UserRepository& users, const ix::HttpRequestPtr& request) {
    try {
        if (request->method == "GET" && request->uri == "/health") {
            return handle_health(users);
        }
        if (request->method == "POST" && request->uri == "/api/auth/register") {
            return handle_register(users, json::parse(request->body));
        }
        if (request->method == "POST" && request->uri == "/api/auth/login") {
            return handle_login(users, json::parse(request->body));
        }
        if (request->method == "GET" && request->uri.rfind(kHistoryPrefix, 0) == 0) {
            return handle_history(users, request->uri.substr(kHistoryPrefix.size()));
        }
    } catch (const json::exception&) {
        return empty_response(400);  // malformed body, or a required field missing
    }
    return empty_response(404);
}

}  // namespace

HttpApiServer::HttpApiServer(int port, kfc::database::UserRepository& users, kfc::protocol::FileLogger& logger)
    : port_(port), users_(users), logger_(logger) {
    // Paired with uninitNetSystem() in the destructor, exactly like
    // WebSocketGameServer -- ix's own init/uninit are reference-counted, so
    // two servers in one process each doing this is the normal pattern.
    ix::initNetSystem();
    server_ = std::make_unique<ix::HttpServer>(port_, "0.0.0.0");

    server_->setOnConnectionCallback(
        [this](ix::HttpRequestPtr request, const std::shared_ptr<ix::ConnectionState>&) -> ix::HttpResponsePtr {
            return dispatch(users_, request);
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
