#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <thread>

#include "api.h"
#include "database.h"
#include "seed.h"

static constexpr int TEST_PORT = 18008;

class ControlsApiTest : public ::testing::Test {
protected:
    meos::db::Database db_{":memory:"};
    httplib::Server svr_;
    std::thread server_thread_;

    void SetUp() override {
        db_.createTables();
        meos::db::seedIfEmpty(db_);
        meos::net::registerControlsRoutes(svr_, db_);
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

TEST_F(ControlsApiTest, GetAllControls_ReturnsArray) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/controls");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_EQ(res->get_header_value("Content-Type"), "application/json");

    auto body = nlohmann::json::parse(res->body);
    ASSERT_TRUE(body.is_array());
    EXPECT_GE(body.size(), 1u);
    EXPECT_TRUE(body[0].contains("id"));
    EXPECT_TRUE(body[0].contains("code"));
}

TEST_F(ControlsApiTest, GetAllControls_ContainsForkJunction) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/controls");
    ASSERT_NE(res, nullptr);

    auto body = nlohmann::json::parse(res->body);
    bool found = false;
    for (const auto& item : body) {
        if (item["id"].get<int>() == 1 &&
            item["code"].get<int>() == 101) {
            found = true;
            EXPECT_EQ(item["description"].get<std::string>(), "Fork junction");
            EXPECT_EQ(item["type"].get<std::string>(), "normal");
        }
    }
    EXPECT_TRUE(found) << "Expected control id=1 code=101 in response";
}

TEST_F(ControlsApiTest, GetControlById_ReturnsControl) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/controls/1");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_EQ(res->get_header_value("Content-Type"), "application/json");

    auto body = nlohmann::json::parse(res->body);
    EXPECT_TRUE(body.is_object());
    EXPECT_EQ(body["id"].get<int>(), 1);
    EXPECT_EQ(body["code"].get<int>(), 101);
}

TEST_F(ControlsApiTest, GetControlById_NotFound_Returns404) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/controls/9999");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 404);
    EXPECT_EQ(res->get_header_value("Content-Type"), "application/json");

    auto body = nlohmann::json::parse(res->body);
    EXPECT_EQ(body["message"].get<std::string>(), "Not found");
    EXPECT_EQ(body["status"].get<int>(), 404);
}
