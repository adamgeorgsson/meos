#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <thread>

#include "api.h"
#include "database.h"
#include "seed.h"

static constexpr int TEST_PORT = 18010;

class ClassesApiTest : public ::testing::Test {
protected:
    meos::db::Database db_{":memory:"};
    httplib::Server svr_;
    std::thread server_thread_;

    void SetUp() override {
        db_.createTables();
        meos::db::seedIfEmpty(db_);
        meos::net::registerClassesRoutes(svr_, db_);
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

TEST_F(ClassesApiTest, GetAllClasses_ReturnsArray) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/classes");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_EQ(res->get_header_value("Content-Type"), "application/json");

    auto body = nlohmann::json::parse(res->body);
    ASSERT_TRUE(body.is_array());
    EXPECT_GE(body.size(), 1u);
    EXPECT_TRUE(body[0].contains("id"));
    EXPECT_TRUE(body[0].contains("name"));
}

TEST_F(ClassesApiTest, GetAllClasses_ContainsExpectedClass) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/classes");
    ASSERT_NE(res, nullptr);

    auto body = nlohmann::json::parse(res->body);
    bool found = false;
    for (const auto& item : body) {
        if (item["id"].get<int>() == 1) {
            found = true;
            EXPECT_EQ(item["name"].get<std::string>(), "H21E");
            EXPECT_TRUE(item.contains("courseId"));
            EXPECT_EQ(item["courseId"].get<int>(), 1);
        }
    }
    EXPECT_TRUE(found) << "Expected class id=1 name='H21E' in response";
}

TEST_F(ClassesApiTest, GetClassById_ReturnsClass) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/classes/1");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_EQ(res->get_header_value("Content-Type"), "application/json");

    auto body = nlohmann::json::parse(res->body);
    EXPECT_TRUE(body.is_object());
    EXPECT_EQ(body["id"].get<int>(), 1);
    EXPECT_EQ(body["name"].get<std::string>(), "H21E");
}

TEST_F(ClassesApiTest, GetClassById_NotFound_Returns404) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/classes/9999");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 404);
    EXPECT_EQ(res->get_header_value("Content-Type"), "application/json");

    auto body = nlohmann::json::parse(res->body);
    EXPECT_EQ(body["message"].get<std::string>(), "Not found");
    EXPECT_EQ(body["status"].get<int>(), 404);
}

TEST_F(ClassesApiTest, PostClass_CreatesAndReturns201) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json req;
    req["name"] = "D21";
    req["courseId"] = 1;
    req["startMethod"] = "individual";
    auto res = cli.Post("/api/v1/classes", req.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 201);

    auto body = nlohmann::json::parse(res->body);
    EXPECT_TRUE(body["id"].get<int>() > 0);
    EXPECT_EQ(body["name"].get<std::string>(), "D21");
    EXPECT_EQ(body["courseId"].get<int>(), 1);
    EXPECT_EQ(body["startMethod"].get<std::string>(), "individual");
}

TEST_F(ClassesApiTest, PostClass_MissingName_Returns400) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json req;
    req["courseId"] = 1;
    auto res = cli.Post("/api/v1/classes", req.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 400);
}

TEST_F(ClassesApiTest, PutClass_UpdatesAndReturns200) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json req;
    req["name"] = "H21E Updated";
    req["courseId"] = 1;
    auto res = cli.Put("/api/v1/classes/1", req.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto body = nlohmann::json::parse(res->body);
    EXPECT_EQ(body["name"].get<std::string>(), "H21E Updated");
    EXPECT_EQ(body["courseId"].get<int>(), 1);

    // Verify persisted
    auto get = cli.Get("/api/v1/classes/1");
    ASSERT_NE(get, nullptr);
    auto getBody = nlohmann::json::parse(get->body);
    EXPECT_EQ(getBody["name"].get<std::string>(), "H21E Updated");
}

TEST_F(ClassesApiTest, PutClass_CourseIdZero_ClearsAssignment) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json req;
    req["name"] = "H21E";
    req["courseId"] = 0;  // 0 should clear the course assignment
    auto res = cli.Put("/api/v1/classes/1", req.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto body = nlohmann::json::parse(res->body);
    EXPECT_FALSE(body.contains("courseId")) << "courseId=0 should clear the assignment";
}

TEST_F(ClassesApiTest, PutClass_NotFound_Returns404) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json req;
    req["name"] = "Ghost";
    auto res = cli.Put("/api/v1/classes/9999", req.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 404);
}

TEST_F(ClassesApiTest, DeleteClass_Returns204) {
    // Create one to delete
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json req;
    req["name"] = "ToDelete";
    auto post = cli.Post("/api/v1/classes", req.dump(), "application/json");
    ASSERT_NE(post, nullptr);
    int newId = nlohmann::json::parse(post->body)["id"].get<int>();

    auto del = cli.Delete("/api/v1/classes/" + std::to_string(newId));
    ASSERT_NE(del, nullptr);
    EXPECT_EQ(del->status, 204);

    // Verify gone
    auto get = cli.Get("/api/v1/classes/" + std::to_string(newId));
    ASSERT_NE(get, nullptr);
    EXPECT_EQ(get->status, 404);
}

TEST_F(ClassesApiTest, DeleteClass_NotFound_Returns404) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Delete("/api/v1/classes/9999");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 404);
}
