#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <thread>

#include "api.h"
#include "database.h"
#include "seed.h"

static constexpr int TEST_PORT = 18013;

class CompetitionsApiTest : public ::testing::Test {
protected:
    meos::db::Database db_{":memory:"};
    httplib::Server svr_;
    std::thread server_thread_;

    void SetUp() override {
        db_.createTables();
        meos::db::seedIfEmpty(db_);
        meos::net::registerCompetitionsRoutes(svr_, db_);
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

TEST_F(CompetitionsApiTest, GetCompetition_ReturnsObject) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/competitions");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_EQ(res->get_header_value("Content-Type"), "application/json");

    auto body = nlohmann::json::parse(res->body);
    EXPECT_TRUE(body.is_object());
    EXPECT_TRUE(body.contains("id"));
    EXPECT_TRUE(body.contains("name"));
    EXPECT_TRUE(body.contains("date"));
    EXPECT_TRUE(body.contains("organizer"));
    EXPECT_TRUE(body.contains("location"));
}

TEST_F(CompetitionsApiTest, GetCompetition_ContainsSpringCup) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/competitions");
    ASSERT_NE(res, nullptr);

    auto body = nlohmann::json::parse(res->body);
    EXPECT_EQ(body["id"].get<int>(), 1);
    EXPECT_EQ(body["name"].get<std::string>(), "Spring Cup 2026");
    EXPECT_EQ(body["organizer"].get<std::string>(), "IF Berget");
}

TEST_F(CompetitionsApiTest, GetCompetition_HasDescription) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/competitions");
    ASSERT_NE(res, nullptr);

    auto body = nlohmann::json::parse(res->body);
    EXPECT_TRUE(body.contains("description"));
    EXPECT_FALSE(body["description"].get<std::string>().empty());
}
