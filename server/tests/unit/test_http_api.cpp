#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include <ixwebsocket/IXHttpClient.h>
#include <ixwebsocket/IXNetSystem.h>
#include <nlohmann/json.hpp>

#include "kfc/database/user_repository.hpp"
#include "kfc/protocol/file_logger.hpp"
#include "kfc/server/http_api.hpp"

// Binds a real port and speaks real HTTP+JSON against it, the same choice
// test_websocket_end_to_end.cpp makes for WebSocketGameServer: HttpApiServer's
// own request-dispatch logic (method/uri routing, status codes, JSON shape) is
// exactly the kind of mistake that is invisible to a test calling C++ methods
// directly. Unlike that suite, there is no tick thread and no Match here, so
// none of its documented shutdown-deadlock risk applies -- each test's server
// serves a handful of stateless request/response round trips and stops.

using nlohmann::json;

namespace {

// Ports are picked well out of the way of anything a developer is likely to be
// running, and bumped per test so a socket left in TIME_WAIT by the previous
// one cannot make the next one flaky. A different range from
// test_websocket_end_to_end.cpp's 18730+ purely to keep the two apart at a
// glance.
int next_port() {
    static std::atomic<int> port{18830};
    return port.fetch_add(1);
}

std::string fresh_db_path() {
    static std::atomic<int> counter{0};
    std::filesystem::path path = std::filesystem::temp_directory_path() /
                                 ("kfc_http_api_test_" + std::to_string(counter.fetch_add(1)) + ".db");
    std::filesystem::remove(path);
    return path.string();
}

// The whole HTTP API on a real port, for one test.
class HttpApiFixture : public ::testing::Test {
protected:
    void SetUp() override {
        port_ = next_port();
        users_ = std::make_unique<kfc::database::UserRepository>(fresh_db_path());
        server_ = std::make_unique<kfc::server::HttpApiServer>(port_, *users_, logger_);
        ASSERT_TRUE(server_->listen());
        server_->start();
        base_url_ = "http://127.0.0.1:" + std::to_string(port_);
    }

    void TearDown() override { server_->stop(); }

    ix::HttpResponsePtr post(const std::string& path, const std::string& body) {
        ix::HttpClient client;
        return client.request(base_url_ + path, "POST", body, client.createRequest());
    }

    ix::HttpResponsePtr get(const std::string& path) {
        ix::HttpClient client;
        return client.get(base_url_ + path, client.createRequest());
    }

    kfc::protocol::FileLogger logger_{std::filesystem::temp_directory_path() / "kfc_http_api_test.log"};
    int port_ = 0;
    std::string base_url_;
    std::unique_ptr<kfc::database::UserRepository> users_;
    std::unique_ptr<kfc::server::HttpApiServer> server_;
};

}  // namespace

TEST_F(HttpApiFixture, RegisterNewUserReturns201WithUsernameAndStartingRating) {
    ix::HttpResponsePtr response = post("/api/auth/register", json{{"username", "alice"}, {"password", "hunter2"}}.dump());

    ASSERT_EQ(response->statusCode, 201);
    json body = json::parse(response->body);
    EXPECT_EQ(body.at("username").get<std::string>(), "alice");
    EXPECT_EQ(body.at("rating").get<int>(), 1200);
}

TEST_F(HttpApiFixture, RegisterAnExistingUsernameReturns409) {
    post("/api/auth/register", json{{"username", "alice"}, {"password", "hunter2"}}.dump());

    ix::HttpResponsePtr response = post("/api/auth/register", json{{"username", "alice"}, {"password", "other"}}.dump());

    EXPECT_EQ(response->statusCode, 409);
}

TEST_F(HttpApiFixture, LoginWithTheRightPasswordReturns200) {
    post("/api/auth/register", json{{"username", "alice"}, {"password", "hunter2"}}.dump());

    ix::HttpResponsePtr response = post("/api/auth/login", json{{"username", "alice"}, {"password", "hunter2"}}.dump());

    ASSERT_EQ(response->statusCode, 200);
    json body = json::parse(response->body);
    EXPECT_EQ(body.at("username").get<std::string>(), "alice");
}

TEST_F(HttpApiFixture, LoginWithTheWrongPasswordReturns401) {
    post("/api/auth/register", json{{"username", "alice"}, {"password", "hunter2"}}.dump());

    ix::HttpResponsePtr response = post("/api/auth/login", json{{"username", "alice"}, {"password", "wrong"}}.dump());

    EXPECT_EQ(response->statusCode, 401);
}

TEST_F(HttpApiFixture, LoginAsAnUnknownUserReturns401AndDoesNotRegisterThem) {
    ix::HttpResponsePtr response = post("/api/auth/login", json{{"username", "ghost"}, {"password", "x"}}.dump());

    EXPECT_EQ(response->statusCode, 401);
    EXPECT_FALSE(users_->user_exists("ghost")) << "login must never silently register an unknown username";
}

TEST_F(HttpApiFixture, MalformedRegisterBodyReturns400) {
    ix::HttpResponsePtr response = post("/api/auth/register", "not json");

    EXPECT_EQ(response->statusCode, 400);
}

TEST_F(HttpApiFixture, HealthReturns200WithStatusOk) {
    ix::HttpResponsePtr response = get("/health");

    ASSERT_EQ(response->statusCode, 200);
    json body = json::parse(response->body);
    EXPECT_EQ(body.at("status").get<std::string>(), "ok");
}

TEST_F(HttpApiFixture, UnknownRouteReturns404) {
    ix::HttpResponsePtr response = get("/api/nope");

    EXPECT_EQ(response->statusCode, 404);
}

TEST_F(HttpApiFixture, HistoryForAnUnknownUsernameIs200WithAnEmptyArray) {
    ix::HttpResponsePtr response = get("/api/history/nobody");

    ASSERT_EQ(response->statusCode, 200);
    json body = json::parse(response->body);
    EXPECT_TRUE(body.is_array());
    EXPECT_TRUE(body.empty());
}

TEST_F(HttpApiFixture, HistoryReflectsAGameRecordedDirectlyThroughUserRepository) {
    using namespace std::chrono;
    auto started = system_clock::now();
    users_->record_game("alice", "bob", "alice", "decisive", started, started + seconds(45));

    ix::HttpResponsePtr response = get("/api/history/bob");

    ASSERT_EQ(response->statusCode, 200);
    json games = json::parse(response->body);
    ASSERT_EQ(games.size(), 1u);
    EXPECT_EQ(games[0].at("whiteUsername").get<std::string>(), "alice");
    EXPECT_EQ(games[0].at("blackUsername").get<std::string>(), "bob");
    EXPECT_EQ(games[0].at("winnerUsername").get<std::string>(), "alice");
    EXPECT_EQ(games[0].at("endReason").get<std::string>(), "decisive");
    EXPECT_TRUE(games[0].contains("startedAt"));
    EXPECT_TRUE(games[0].contains("endedAt"));
}

TEST_F(HttpApiFixture, ADrawSerializesWinnerUsernameAsNull) {
    using namespace std::chrono;
    auto started = system_clock::now();
    users_->record_game("alice", "bob", std::nullopt, "draw", started, started + seconds(20));

    ix::HttpResponsePtr response = get("/api/history/alice");

    ASSERT_EQ(response->statusCode, 200);
    json games = json::parse(response->body);
    ASSERT_EQ(games.size(), 1u);
    EXPECT_TRUE(games[0].at("winnerUsername").is_null());
}
