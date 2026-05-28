#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <thread>

#include "api.h"
#include "database.h"
#include "seed.h"

static constexpr int TEST_PORT = 18016;

class CardsApiTest : public ::testing::Test {
protected:
    meos::db::Database db_{":memory:"};
    httplib::Server svr_;
    std::thread server_thread_;

    void SetUp() override {
        db_.createTables();
        meos::db::seedIfEmpty(db_);
        meos::net::registerCardsRoutes(svr_, db_);
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

TEST_F(CardsApiTest, PostCard_CreatesCard_Returns201) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json body;
    body["cardNumber"] = 9999001;
    body["punches"]    = nlohmann::json::array();

    auto res = cli.Post("/api/v1/cards", body.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 201);
    EXPECT_EQ(res->get_header_value("Content-Type"), "application/json");

    auto rbody = nlohmann::json::parse(res->body);
    EXPECT_TRUE(rbody.contains("id"));
    EXPECT_GT(rbody["id"].get<int>(), 0);
    EXPECT_EQ(rbody["cardNumber"].get<int>(), 9999001);
    EXPECT_TRUE(rbody.contains("punchString"));
}

TEST_F(CardsApiTest, PostCard_WithPunches_SerializesPunchString) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json body;
    body["cardNumber"] = 9999002;
    body["punches"]    = nlohmann::json::array({{{"code", 101}, {"time", 1230}},
                                                {{"code", 102}, {"time", 2560}}});

    auto res = cli.Post("/api/v1/cards", body.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 201);

    auto rbody = nlohmann::json::parse(res->body);
    std::string ps = rbody["punchString"].get<std::string>();
    // Expect format: "101-123.0;102-256.0;"
    EXPECT_NE(ps.find("101"), std::string::npos);
    EXPECT_NE(ps.find("102"), std::string::npos);
}

TEST_F(CardsApiTest, PostCard_LinksToRunnerByCardNumber) {
    // Seed runner 1 has cardNumber 2001234
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json body;
    body["cardNumber"] = 2001234;
    body["punches"]    = nlohmann::json::array();

    auto res = cli.Post("/api/v1/cards", body.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 201);

    auto rbody = nlohmann::json::parse(res->body);
    ASSERT_TRUE(rbody.contains("runnerId"));
    EXPECT_EQ(rbody["runnerId"].get<int>(), 1);  // runner id=1 has cardNumber=2001234
}

TEST_F(CardsApiTest, PostCard_UnknownCardNumber_NoRunnerLink) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json body;
    body["cardNumber"] = 8888888;
    body["punches"]    = nlohmann::json::array();

    auto res = cli.Post("/api/v1/cards", body.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 201);

    auto rbody = nlohmann::json::parse(res->body);
    EXPECT_FALSE(rbody.contains("runnerId"));
}

TEST_F(CardsApiTest, PostCard_MissingCardNumber_Returns400) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json body;
    body["punches"] = nlohmann::json::array();

    auto res = cli.Post("/api/v1/cards", body.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 400);

    auto rbody = nlohmann::json::parse(res->body);
    EXPECT_EQ(rbody["status"].get<int>(), 400);
}

TEST_F(CardsApiTest, GetAllCards_ReturnsArray) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/cards");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto body = nlohmann::json::parse(res->body);
    EXPECT_TRUE(body.is_array());
}

TEST_F(CardsApiTest, GetCardById_NotFound_Returns404) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/cards/9999");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 404);

    auto body = nlohmann::json::parse(res->body);
    EXPECT_EQ(body["status"].get<int>(), 404);
}
