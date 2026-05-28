#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <thread>

#include "api.h"
#include "database.h"
#include "seed.h"

static constexpr int TEST_PORT = 18011;

class RunnersApiTest : public ::testing::Test {
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

// --- GET list (paginated envelope) ---

TEST_F(RunnersApiTest, GetAllRunners_ReturnsPaginatedEnvelope) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/runners");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto body = nlohmann::json::parse(res->body);
    EXPECT_TRUE(body.contains("data"));
    EXPECT_TRUE(body.contains("total"));
    EXPECT_TRUE(body.contains("page"));
    EXPECT_TRUE(body.contains("pageSize"));
    EXPECT_TRUE(body["data"].is_array());
    EXPECT_GE(body["total"].get<int>(), 1);
    EXPECT_EQ(body["page"].get<int>(), 1);
}

TEST_F(RunnersApiTest, GetAllRunners_ContainsAnnaLindstrom) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/runners");
    ASSERT_NE(res, nullptr);

    auto body = nlohmann::json::parse(res->body);
    bool found = false;
    for (const auto& item : body["data"]) {
        if (item["id"].get<int>() == 1) {
            found = true;
            EXPECT_TRUE(item.contains("clubId"));
            EXPECT_TRUE(item.contains("classId"));
        }
    }
    EXPECT_TRUE(found) << "Expected runner id=1 in response";
}

// --- GET by ID ---

TEST_F(RunnersApiTest, GetRunnerById_ReturnsRunner) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/runners/1");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_EQ(res->get_header_value("Content-Type"), "application/json");

    auto body = nlohmann::json::parse(res->body);
    EXPECT_TRUE(body.is_object());
    EXPECT_EQ(body["id"].get<int>(), 1);
    EXPECT_FALSE(body["name"].get<std::string>().empty());
}

TEST_F(RunnersApiTest, GetRunnerById_NotFound_Returns404) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/runners/9999");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 404);

    auto body = nlohmann::json::parse(res->body);
    EXPECT_EQ(body["status"].get<int>(), 404);
}

// --- POST ---

TEST_F(RunnersApiTest, PostRunner_CreatesAndReturns201) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json req;
    req["name"]   = "Test Runner";
    req["clubId"] = 1;
    req["status"] = "ok";
    auto res = cli.Post("/api/v1/runners", req.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 201);

    auto body = nlohmann::json::parse(res->body);
    EXPECT_GT(body["id"].get<int>(), 0);
    EXPECT_EQ(body["name"].get<std::string>(), "Test Runner");
}

TEST_F(RunnersApiTest, PostRunner_MissingName_Returns400) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json req;
    req["clubId"] = 1;
    auto res = cli.Post("/api/v1/runners", req.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 400);
}

// --- PUT ---

TEST_F(RunnersApiTest, PutRunner_UpdatesAndReturns200) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json req;
    req["name"]   = "Updated Runner";
    req["status"] = "dns";
    auto res = cli.Put("/api/v1/runners/1", req.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto body = nlohmann::json::parse(res->body);
    EXPECT_EQ(body["name"].get<std::string>(), "Updated Runner");
}

TEST_F(RunnersApiTest, PutRunner_NotFound_Returns404) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json req;
    req["name"] = "Ghost";
    auto res = cli.Put("/api/v1/runners/9999", req.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 404);
}

// --- DELETE ---

TEST_F(RunnersApiTest, DeleteRunner_Returns204) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    // Create a new runner first (no FK dependencies)
    nlohmann::json req;
    req["name"] = "ToDelete Runner";
    auto post = cli.Post("/api/v1/runners", req.dump(), "application/json");
    ASSERT_NE(post, nullptr);
    ASSERT_EQ(post->status, 201);
    int newId = nlohmann::json::parse(post->body)["id"].get<int>();

    auto res = cli.Delete("/api/v1/runners/" + std::to_string(newId));
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 204);
}

TEST_F(RunnersApiTest, DeleteRunner_NotFound_Returns404) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Delete("/api/v1/runners/9999");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 404);
}

// --- Filtering ---

TEST_F(RunnersApiTest, GetRunners_FilterByName) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/runners?name=Anna");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto body = nlohmann::json::parse(res->body);
    for (const auto& item : body["data"]) {
        EXPECT_NE(item["name"].get<std::string>().find("Anna"), std::string::npos);
    }
    EXPECT_GE(body["data"].size(), 1u);
}

TEST_F(RunnersApiTest, GetRunners_FilterByClassId) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/runners?classId=1");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto body = nlohmann::json::parse(res->body);
    for (const auto& item : body["data"]) {
        EXPECT_EQ(item["classId"].get<int>(), 1);
    }
}

// --- Pagination ---

TEST_F(RunnersApiTest, GetRunners_Pagination) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/runners?page=1&pageSize=2");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto body = nlohmann::json::parse(res->body);
    EXPECT_EQ(body["page"].get<int>(), 1);
    EXPECT_EQ(body["pageSize"].get<int>(), 2);
    EXPECT_LE(body["data"].size(), 2u);
    EXPECT_GE(body["total"].get<int>(), body["data"].size());
}

