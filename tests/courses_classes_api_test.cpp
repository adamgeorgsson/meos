// US-005c: REST API tests for courses and classes
#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include "SQLiteDatabase.h"
#include "DbMigration.h"
#include "CourseRepository.h"
#include "ClassRepository.h"
#include "ControlRepository.h"
#include "CoursesApi.h"
#include "ClassesApi.h"
#include "ControlsApi.h"
#include "HttpServer.h"
#include "oEvent.h"

using namespace meos_net;
using namespace meos_db;
using json = nlohmann::json;

static constexpr int COURSES_PORT = 18083;
static constexpr int CLASSES_PORT = 18084;

// ─── Courses API ───────────────────────────────────────────────────────────────

class CourseApiTest : public ::testing::Test {
protected:
    SQLiteDatabase    db;
    oEvent            oe;
    CourseRepository  courseRepo{db};
    ControlRepository ctrlRepo{db};
    HttpServer        server;
    httplib::Client   client{"localhost", COURSES_PORT};

    void SetUp() override {
        db.open(":memory:");
        DbMigrationManager mgr(db);
        mgr.applyMigrations(SchemaV4::migrations());

        oe.newCompetition(L"TestComp");

        auto router = server.router();
        registerCourseRoutes(router, oe, courseRepo);
        registerControlRoutes(router, oe, ctrlRepo);

        server.start("localhost", COURSES_PORT);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    void TearDown() override {
        server.stop();
    }

    json postCourse(const std::string& name, int length = 0) {
        json req = {{"name", name}, {"length", length}};
        auto res = client.Post("/api/v1/courses", req.dump(), "application/json");
        EXPECT_TRUE(res != nullptr);
        return json::parse(res->body);
    }

    /// Create a control via the REST API and return its id.
    int postControl(const std::string& name, int number = 0) {
        json req = {{"name", name}, {"number", number}};
        auto res = client.Post("/api/v1/controls", req.dump(), "application/json");
        EXPECT_TRUE(res != nullptr);
        auto body = json::parse(res->body);
        return body["data"]["id"].get<int>();
    }
};

TEST_F(CourseApiTest, PostCourseCreates) {
    auto res = client.Post("/api/v1/courses",
                           R"({"name":"Sprint","length":3000})",
                           "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"]["name"], "Sprint");
    EXPECT_EQ(body["data"]["length"].get<int>(), 3000);
    EXPECT_GT(body["data"]["id"].get<int>(), 0);
}

TEST_F(CourseApiTest, PostCoursePersistsToDb) {
    json resp = postCourse("Forest", 5000);
    int id = resp["data"]["id"].get<int>();
    auto row = courseRepo.findById(id);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->name, "Forest");
    EXPECT_EQ(row->length, 5000);
}

TEST_F(CourseApiTest, GetCourseById) {
    json resp = postCourse("Middle", 2500);
    int id = resp["data"]["id"].get<int>();

    auto res = client.Get("/api/v1/courses/" + std::to_string(id));
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"]["id"].get<int>(), id);
    EXPECT_EQ(body["data"]["name"], "Middle");
}

TEST_F(CourseApiTest, GetCourseByIdNotFound) {
    auto res = client.Get("/api/v1/courses/99999");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404);
    json body = json::parse(res->body);
    EXPECT_EQ(body["error"]["code"], 404);
}

TEST_F(CourseApiTest, GetCoursesListAll) {
    postCourse("Course A");
    postCourse("Course B");

    auto res = client.Get("/api/v1/courses");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_GE(body["data"].size(), 2u);
}

TEST_F(CourseApiTest, GetCoursesFilterByName) {
    postCourse("Alpha Course");
    postCourse("Beta Course");

    auto res = client.Get("/api/v1/courses?name=Alpha");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"].size(), 1u);
    EXPECT_EQ(body["data"][0]["name"], "Alpha Course");
}

TEST_F(CourseApiTest, PostCourseWithControlIds) {
    int c1 = postControl("Ctrl1", 31);
    int c2 = postControl("Ctrl2", 32);
    int c3 = postControl("Ctrl3", 33);

    json req = {{"name", "WithControls"}, {"length", 4000},
                {"controlIds", json::array({c1, c2, c3})}};
    auto res = client.Post("/api/v1/courses", req.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"]["controlIds"].size(), 3u);
    EXPECT_EQ(body["data"]["controlIds"][0].get<int>(), c1);
    EXPECT_EQ(body["data"]["controlIds"][1].get<int>(), c2);
    EXPECT_EQ(body["data"]["controlIds"][2].get<int>(), c3);
}

TEST_F(CourseApiTest, PutCourseUpdates) {
    json resp = postCourse("OldName", 1000);
    int id = resp["data"]["id"].get<int>();

    json upd = {{"name", "NewName"}, {"length", 2000}};
    auto res = client.Put("/api/v1/courses/" + std::to_string(id),
                          upd.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"]["name"], "NewName");
    EXPECT_EQ(body["data"]["length"].get<int>(), 2000);

    auto row = courseRepo.findById(id);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->name, "NewName");
    EXPECT_EQ(row->length, 2000);
}

TEST_F(CourseApiTest, PutCourseUpdatesControlIds) {
    int c1 = postControl("C1", 51);
    int c2 = postControl("C2", 52);

    json resp = postCourse("Course X");
    int id = resp["data"]["id"].get<int>();

    json upd = {{"controlIds", json::array({c1, c2})}};
    auto res = client.Put("/api/v1/courses/" + std::to_string(id),
                          upd.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"]["controlIds"].size(), 2u);
}

TEST_F(CourseApiTest, PutCourseNotFound) {
    json upd = {{"name", "X"}};
    auto res = client.Put("/api/v1/courses/99999", upd.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404);
}

TEST_F(CourseApiTest, DeleteCourse) {
    json resp = postCourse("ToDelete");
    int id = resp["data"]["id"].get<int>();

    auto res = client.Delete("/api/v1/courses/" + std::to_string(id));
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"]["deleted"].get<int>(), id);

    EXPECT_EQ(oe.getCourse(id), nullptr);
    EXPECT_FALSE(courseRepo.findById(id).has_value());
}

TEST_F(CourseApiTest, DeleteCourseNotFound) {
    auto res = client.Delete("/api/v1/courses/99999");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404);
}

TEST_F(CourseApiTest, PostCourseMissingNameReturns400) {
    auto res = client.Post("/api/v1/courses", R"({"length":1000})", "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

TEST_F(CourseApiTest, PostCourseInvalidJsonReturns400) {
    auto res = client.Post("/api/v1/courses", "not-json", "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

// ─── Classes API ───────────────────────────────────────────────────────────────

class ClassApiTest : public ::testing::Test {
protected:
    SQLiteDatabase   db;
    oEvent           oe;
    ClassRepository  classRepo{db};
    CourseRepository courseRepo{db};
    HttpServer       server;
    httplib::Client  client{"localhost", CLASSES_PORT};

    void SetUp() override {
        db.open(":memory:");
        DbMigrationManager mgr(db);
        mgr.applyMigrations(SchemaV4::migrations());

        oe.newCompetition(L"TestComp");

        auto router = server.router();
        registerClassRoutes(router, oe, classRepo);
        registerCourseRoutes(router, oe, courseRepo);

        server.start("localhost", CLASSES_PORT);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    void TearDown() override {
        server.stop();
    }

    json postClass(const std::string& name, int courseId = 0) {
        json req = {{"name", name}, {"courseId", courseId}};
        auto res = client.Post("/api/v1/classes", req.dump(), "application/json");
        EXPECT_TRUE(res != nullptr);
        return json::parse(res->body);
    }

    int postCourse(const std::string& name) {
        json req = {{"name", name}};
        auto res = client.Post("/api/v1/courses", req.dump(), "application/json");
        EXPECT_TRUE(res != nullptr);
        return json::parse(res->body)["data"]["id"].get<int>();
    }
};

TEST_F(ClassApiTest, PostClassCreates) {
    auto res = client.Post("/api/v1/classes",
                           R"({"name":"M21E"})",
                           "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"]["name"], "M21E");
    EXPECT_GT(body["data"]["id"].get<int>(), 0);
}

TEST_F(ClassApiTest, PostClassPersistsToDb) {
    json resp = postClass("W21E");
    int id = resp["data"]["id"].get<int>();
    auto row = classRepo.findById(id);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->name, "W21E");
}

TEST_F(ClassApiTest, GetClassById) {
    json resp = postClass("M35");
    int id = resp["data"]["id"].get<int>();

    auto res = client.Get("/api/v1/classes/" + std::to_string(id));
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"]["id"].get<int>(), id);
    EXPECT_EQ(body["data"]["name"], "M35");
}

TEST_F(ClassApiTest, GetClassByIdNotFound) {
    auto res = client.Get("/api/v1/classes/99999");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404);
    json body = json::parse(res->body);
    EXPECT_EQ(body["error"]["code"], 404);
}

TEST_F(ClassApiTest, GetClassesListAll) {
    postClass("Class A");
    postClass("Class B");

    auto res = client.Get("/api/v1/classes");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_GE(body["data"].size(), 2u);
}

TEST_F(ClassApiTest, GetClassesFilterByName) {
    postClass("Alpha class");
    postClass("Beta class");

    auto res = client.Get("/api/v1/classes?name=Alpha");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"].size(), 1u);
    EXPECT_EQ(body["data"][0]["name"], "Alpha class");
}

TEST_F(ClassApiTest, PostClassWithCourseId) {
    int courseId = postCourse("Sprint");

    json resp = postClass("M21E", courseId);
    EXPECT_EQ(resp["data"]["courseId"].get<int>(), courseId);
}

TEST_F(ClassApiTest, PutClassUpdatesName) {
    json resp = postClass("OldClass");
    int id = resp["data"]["id"].get<int>();

    json upd = {{"name", "NewClass"}};
    auto res = client.Put("/api/v1/classes/" + std::to_string(id),
                          upd.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"]["name"], "NewClass");

    auto row = classRepo.findById(id);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->name, "NewClass");
}

TEST_F(ClassApiTest, PutClassAssignsCourse) {
    int courseId = postCourse("Forest");
    json resp = postClass("W35");
    int id = resp["data"]["id"].get<int>();

    json upd = {{"courseId", courseId}};
    auto res = client.Put("/api/v1/classes/" + std::to_string(id),
                          upd.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"]["courseId"].get<int>(), courseId);
}

TEST_F(ClassApiTest, PutClassClearsCourseWithZero) {
    int courseId = postCourse("Long");
    json resp = postClass("M40", courseId);
    int id = resp["data"]["id"].get<int>();

    json upd = {{"courseId", 0}};
    auto res = client.Put("/api/v1/classes/" + std::to_string(id),
                          upd.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"]["courseId"].get<int>(), 0);
}

TEST_F(ClassApiTest, PutClassNotFound) {
    json upd = {{"name", "X"}};
    auto res = client.Put("/api/v1/classes/99999", upd.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404);
}

TEST_F(ClassApiTest, DeleteClass) {
    json resp = postClass("ToDelete");
    int id = resp["data"]["id"].get<int>();

    auto res = client.Delete("/api/v1/classes/" + std::to_string(id));
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"]["deleted"].get<int>(), id);

    EXPECT_EQ(oe.getClass(id), nullptr);
    EXPECT_FALSE(classRepo.findById(id).has_value());
}

TEST_F(ClassApiTest, DeleteClassNotFound) {
    auto res = client.Delete("/api/v1/classes/99999");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404);
}

TEST_F(ClassApiTest, PostClassMissingNameReturns400) {
    auto res = client.Post("/api/v1/classes", R"({})", "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

TEST_F(ClassApiTest, PostClassInvalidJsonReturns400) {
    auto res = client.Post("/api/v1/classes", "bad json", "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}
