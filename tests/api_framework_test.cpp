#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <thread>

#include "api.h"
#include "api_utils.h"
#include "database.h"
#include "http_server.h"
#include "seed.h"

static constexpr int TEST_PORT = 18006;

class ApiFrameworkTest : public ::testing::Test {
protected:
    meos::db::Database db_{":memory:"};
    httplib::Server svr_;
    std::thread server_thread_;

    void SetUp() override {
        db_.createTables();
        meos::db::seedIfEmpty(db_);
        // Register one route group so /api/v1/clubs/* tests routing.
        meos::net::registerClubsRoutes(svr_, db_);
        // CORS pre-routing on bare svr_ (not HttpServer wrapper).
        svr_.set_pre_routing_handler(
            [](const httplib::Request& req, httplib::Response& res) {
                res.set_header("Access-Control-Allow-Origin", "*");
                if (req.method == "OPTIONS" &&
                    req.path.rfind("/api/", 0) == 0) {
                    res.set_header("Access-Control-Allow-Methods",
                                   "GET, POST, PUT, DELETE, OPTIONS");
                    res.set_header("Access-Control-Allow-Headers",
                                   "Content-Type");
                    res.status = 204;
                    return httplib::Server::HandlerResponse::Handled;
                }
                return httplib::Server::HandlerResponse::Unhandled;
            });
        // JSON error handler: fills body only if handler left it empty.
        svr_.set_error_handler(
            [](const httplib::Request&, httplib::Response& res) {
                if (res.body.empty()) {
                    res.set_content(
                        meos::net::makeError(res.status, "Not found").dump(),
                        "application/json");
                }
            });

        server_thread_ = std::thread([this] {
            svr_.listen("127.0.0.1", TEST_PORT);
        });
        svr_.wait_until_ready();
    }

    void TearDown() override {
        svr_.stop();
        if (server_thread_.joinable()) server_thread_.join();
    }
};

// --- Error envelope shape -------------------------------------------------

TEST_F(ApiFrameworkTest, ErrorEnvelope_Has_MessageAndStatus) {
    auto j = meos::net::makeError(404, "Not found");
    EXPECT_EQ(j["status"].get<int>(), 404);
    EXPECT_EQ(j["message"].get<std::string>(), "Not found");
}

TEST_F(ApiFrameworkTest, GetById_NotFound_ErrorEnvelope) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/clubs/9999");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 404);
    EXPECT_EQ(res->get_header_value("Content-Type"), "application/json");

    auto body = nlohmann::json::parse(res->body);
    EXPECT_TRUE(body.contains("status"));
    EXPECT_TRUE(body.contains("message"));
    EXPECT_EQ(body["status"].get<int>(), 404);
    EXPECT_FALSE(body["message"].get<std::string>().empty());
}

// Handler-set 404 body must not be overwritten by the error handler.
TEST_F(ApiFrameworkTest, HandlerSet404_BodyNotOverwritten) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/clubs/9999");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 404);
    // Body must be the specific JSON set by the handler (not a generic error).
    auto body = nlohmann::json::parse(res->body);
    EXPECT_EQ(body["message"].get<std::string>(), "Not found");
}

// --- CORS headers ---------------------------------------------------------

TEST_F(ApiFrameworkTest, CorsHeader_PresentOnGetResponse) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/clubs");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->get_header_value("Access-Control-Allow-Origin"), "*");
}

TEST_F(ApiFrameworkTest, Options_ApiPath_Returns204WithCorsHeaders) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    httplib::Headers headers;
    auto res = cli.Options("/api/v1/clubs");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 204);
    EXPECT_FALSE(
        res->get_header_value("Access-Control-Allow-Methods").empty());
    EXPECT_FALSE(
        res->get_header_value("Access-Control-Allow-Headers").empty());
}

// --- API versioning -------------------------------------------------------

TEST_F(ApiFrameworkTest, ApiVersionPrefix_V1_Resolves) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/clubs");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
}

// --- Input validation utilities -------------------------------------------

TEST(ParsePathIdTest, ValidPositiveInt_ReturnsValue) {
    EXPECT_EQ(meos::net::parsePathId("1"), 1);
    EXPECT_EQ(meos::net::parsePathId("123"), 123);
    EXPECT_EQ(meos::net::parsePathId("999999"), 999999);
}

TEST(ParsePathIdTest, Zero_ReturnsNullopt) {
    EXPECT_FALSE(meos::net::parsePathId("0").has_value());
}

TEST(ParsePathIdTest, NegativeString_ReturnsNullopt) {
    EXPECT_FALSE(meos::net::parsePathId("-1").has_value());
}

TEST(ParsePathIdTest, NonNumericString_ReturnsNullopt) {
    EXPECT_FALSE(meos::net::parsePathId("abc").has_value());
    EXPECT_FALSE(meos::net::parsePathId("1a2").has_value());
}

TEST(ParsePathIdTest, EmptyString_ReturnsNullopt) {
    EXPECT_FALSE(meos::net::parsePathId("").has_value());
}

TEST(RequireStringTest, PresentField_ReturnsValue) {
    nlohmann::json j = {{"name", "Alice"}};
    EXPECT_NO_THROW({
        EXPECT_EQ(meos::net::requireString(j, "name"), "Alice");
    });
}

TEST(RequireStringTest, MissingField_Throws) {
    nlohmann::json j = {{"other", "value"}};
    EXPECT_THROW(meos::net::requireString(j, "name"),
                 std::invalid_argument);
}

TEST(RequireStringTest, WrongType_Throws) {
    nlohmann::json j = {{"name", 42}};
    EXPECT_THROW(meos::net::requireString(j, "name"),
                 std::invalid_argument);
}

TEST(RequireIntTest, PresentField_ReturnsValue) {
    nlohmann::json j = {{"age", 30}};
    EXPECT_EQ(meos::net::requireInt(j, "age"), 30);
}

TEST(RequireIntTest, MissingField_Throws) {
    nlohmann::json j = {{"other", "value"}};
    EXPECT_THROW(meos::net::requireInt(j, "age"), std::invalid_argument);
}

// --- Route registration pattern (regex path params) ----------------------

TEST_F(ApiFrameworkTest, RegexPathParam_MatchesNumericId) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/clubs/1");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    auto body = nlohmann::json::parse(res->body);
    EXPECT_EQ(body["id"].get<int>(), 1);
}

// --- HttpServer background thread + stop ----------------------------------

TEST(HttpServerTest, StartsAndStopsCleanly) {
    meos::net::HttpServer srv(18099);
    std::thread t([&srv] { srv.listen(); });
    srv.server().wait_until_ready();
    EXPECT_TRUE(srv.server().is_running());
    srv.stop();
    if (t.joinable()) t.join();
    EXPECT_FALSE(srv.server().is_running());
}
