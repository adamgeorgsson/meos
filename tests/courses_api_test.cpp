#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include <thread>

#include "api.h"
#include "database.h"
#include "seed.h"

static constexpr int TEST_PORT = 18009;

class CoursesApiTest : public ::testing::Test {
protected:
    meos::db::Database db_{":memory:"};
    httplib::Server svr_;
    std::thread server_thread_;

    void SetUp() override {
        db_.createTables();
        meos::db::seedIfEmpty(db_);
        meos::net::registerCoursesRoutes(svr_, db_);
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

TEST_F(CoursesApiTest, GetAllCourses_ReturnsArray) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/courses");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_EQ(res->get_header_value("Content-Type"), "application/json");

    auto body = nlohmann::json::parse(res->body);
    ASSERT_TRUE(body.is_array());
    EXPECT_GE(body.size(), 1u);
    EXPECT_TRUE(body[0].contains("id"));
    EXPECT_TRUE(body[0].contains("name"));
    EXPECT_TRUE(body[0].contains("controls"));
}

TEST_F(CoursesApiTest, GetAllCourses_ControlsIsArray) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/courses");
    ASSERT_NE(res, nullptr);

    auto body = nlohmann::json::parse(res->body);
    bool found = false;
    for (const auto& item : body) {
        if (item["id"].get<int>() == 1) {
            found = true;
            EXPECT_EQ(item["name"].get<std::string>(), "Long");
            EXPECT_TRUE(item["controls"].is_array());
            EXPECT_FALSE(item["controls"].empty());
        }
    }
    EXPECT_TRUE(found) << "Expected course id=1 name='Long' in response";
}

TEST_F(CoursesApiTest, GetCourseById_ReturnsCourse) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/courses/1");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_EQ(res->get_header_value("Content-Type"), "application/json");

    auto body = nlohmann::json::parse(res->body);
    EXPECT_TRUE(body.is_object());
    EXPECT_EQ(body["id"].get<int>(), 1);
    EXPECT_EQ(body["name"].get<std::string>(), "Long");
    EXPECT_TRUE(body["controls"].is_array());
}

TEST_F(CoursesApiTest, GetCourseById_NotFound_Returns404) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/courses/9999");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 404);
    EXPECT_EQ(res->get_header_value("Content-Type"), "application/json");

    auto body = nlohmann::json::parse(res->body);
    EXPECT_EQ(body["message"].get<std::string>(), "Not found");
    EXPECT_EQ(body["status"].get<int>(), 404);
}

TEST_F(CoursesApiTest, PostCourse_CreatesAndReturns201) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json req;
    req["name"] = "Sprint";
    req["length"] = 2500;
    req["controls"] = {101, 102, 103};
    auto res = cli.Post("/api/v1/courses", req.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 201);

    auto body = nlohmann::json::parse(res->body);
    EXPECT_TRUE(body["id"].get<int>() > 0);
    EXPECT_EQ(body["name"].get<std::string>(), "Sprint");
    EXPECT_EQ(body["length"].get<int>(), 2500);
    EXPECT_TRUE(body["controls"].is_array());
    EXPECT_EQ(body["controls"].size(), 3u);
}

TEST_F(CoursesApiTest, PostCourse_MissingName_Returns400) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json req;
    req["length"] = 2500;
    auto res = cli.Post("/api/v1/courses", req.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 400);
}

TEST_F(CoursesApiTest, PutCourse_UpdatesAndReturns200) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json req;
    req["name"] = "Long Updated";
    req["length"] = 9000;
    req["controls"] = {101, 102};
    auto res = cli.Put("/api/v1/courses/1", req.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);

    auto body = nlohmann::json::parse(res->body);
    EXPECT_EQ(body["name"].get<std::string>(), "Long Updated");
    EXPECT_EQ(body["length"].get<int>(), 9000);
    EXPECT_EQ(body["controls"].size(), 2u);

    // Verify persisted
    auto get = cli.Get("/api/v1/courses/1");
    ASSERT_NE(get, nullptr);
    auto getBody = nlohmann::json::parse(get->body);
    EXPECT_EQ(getBody["name"].get<std::string>(), "Long Updated");
}

TEST_F(CoursesApiTest, PutCourse_NotFound_Returns404) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json req;
    req["name"] = "Ghost";
    auto res = cli.Put("/api/v1/courses/9999", req.dump(), "application/json");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 404);
}

TEST_F(CoursesApiTest, DeleteCourse_Returns204) {
    // First create one to delete
    httplib::Client cli("127.0.0.1", TEST_PORT);
    nlohmann::json req;
    req["name"] = "ToDelete";
    auto post = cli.Post("/api/v1/courses", req.dump(), "application/json");
    ASSERT_NE(post, nullptr);
    int newId = nlohmann::json::parse(post->body)["id"].get<int>();

    auto del = cli.Delete("/api/v1/courses/" + std::to_string(newId));
    ASSERT_NE(del, nullptr);
    EXPECT_EQ(del->status, 204);

    // Verify gone
    auto get = cli.Get("/api/v1/courses/" + std::to_string(newId));
    ASSERT_NE(get, nullptr);
    EXPECT_EQ(get->status, 404);
}

TEST_F(CoursesApiTest, DeleteCourse_NotFound_Returns404) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Delete("/api/v1/courses/9999");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 404);
}
