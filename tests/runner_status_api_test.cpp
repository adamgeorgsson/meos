#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <thread>

#include "api.h"
#include "database.h"
#include "seed.h"

static constexpr int TEST_PORT = 18017;

class RunnerStatusApiTest : public ::testing::Test {
protected:
    meos::db::Database db_{":memory:"};
    httplib::Server svr_;
    std::thread server_thread_;

    void SetUp() override {
        db_.createTables();
        meos::db::seedIfEmpty(db_);
        meos::net::registerRunnersRoutes(svr_, db_);
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

TEST_F(RunnerStatusApiTest, PostStatus_ChangesRunnerStatus) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json body;
    body["status"] = "dnf";

    // Runner 2 (Erik Johansson) has status "ok" in seed
    auto res = cli.Post("/api/v1/runners/2/status", body.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_EQ(res->get_header_value("Content-Type"), "application/json");

    auto rbody = nlohmann::json::parse(res->body);
    EXPECT_EQ(rbody["id"].get<int>(), 2);
    EXPECT_EQ(rbody["status"].get<std::string>(), "dnf");
}

TEST_F(RunnerStatusApiTest, PostStatus_GetConfirmsChange) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json body;
    body["status"] = "dns";

    cli.Post("/api/v1/runners/1/status", body.dump(), "application/json");

    auto res = cli.Get("/api/v1/runners/1");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    auto rbody = nlohmann::json::parse(res->body);
    EXPECT_EQ(rbody["status"].get<std::string>(), "dns");
}

TEST_F(RunnerStatusApiTest, PostStatus_NotFound_Returns404) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json body;
    body["status"] = "ok";

    auto res = cli.Post("/api/v1/runners/9999/status", body.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 404);

    auto rbody = nlohmann::json::parse(res->body);
    EXPECT_EQ(rbody["status"].get<int>(), 404);
}

TEST_F(RunnerStatusApiTest, PostStatus_MissingStatus_Returns400) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json body;
    body["foo"] = "bar";

    auto res = cli.Post("/api/v1/runners/1/status", body.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 400);

    auto rbody = nlohmann::json::parse(res->body);
    EXPECT_EQ(rbody["status"].get<int>(), 400);
}

TEST_F(RunnerStatusApiTest, PostStatus_InvalidStatus_Returns400) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json body;
    body["status"] = "flying";

    auto res = cli.Post("/api/v1/runners/1/status", body.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 400);

    auto rbody = nlohmann::json::parse(res->body);
    EXPECT_EQ(rbody["status"].get<int>(), 400);
}

TEST_F(RunnerStatusApiTest, PostStatus_AllValidStatuses_Return200) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    for (const auto& s : {"ok", "dns", "dnf", "dq", "mp", "nc", "inactive"}) {
        nlohmann::json body;
        body["status"] = s;
        auto res = cli.Post("/api/v1/runners/3/status", body.dump(), "application/json");
        ASSERT_NE(res, nullptr) << "Status: " << s;
        EXPECT_EQ(res->status, 200) << "For status value: " << s;
    }
}
