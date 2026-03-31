// US-005b: REST API tests for clubs and controls
#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include "SQLiteDatabase.h"
#include "DbMigration.h"
#include "ClubRepository.h"
#include "ControlRepository.h"
#include "ClubsApi.h"
#include "ControlsApi.h"
#include "HttpServer.h"
#include "oEvent.h"

using namespace meos_net;
using namespace meos_db;
using json = nlohmann::json;

static constexpr int CLUBS_PORT    = 18081;
static constexpr int CONTROLS_PORT = 18082;

// ─── Clubs API ─────────────────────────────────────────────────────────────────

class ClubApiTest : public ::testing::Test {
protected:
    SQLiteDatabase   db;
    oEvent           oe;
    ClubRepository   clubRepo{db};
    HttpServer       server;
    httplib::Client  client{"localhost", CLUBS_PORT};

    void SetUp() override {
        db.open(":memory:");
        DbMigrationManager mgr(db);
        mgr.applyMigrations(SchemaV4::migrations());

        oe.newCompetition(L"TestComp");

        auto router = server.router();
        registerClubRoutes(router, oe, clubRepo);

        server.start("localhost", CLUBS_PORT);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    void TearDown() override {
        server.stop();
    }

    // Helper: POST /api/v1/clubs with given name, returns parsed body.
    json postClub(const std::string& name) {
        json req = {{"name", name}};
        auto res = client.Post("/api/v1/clubs", req.dump(), "application/json");
        EXPECT_TRUE(res != nullptr);
        return json::parse(res->body);
    }
};

TEST_F(ClubApiTest, PostClubCreatesClub) {
    auto res = client.Post("/api/v1/clubs", R"({"name":"Alfa OK"})", "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_TRUE(body.contains("data"));
    EXPECT_EQ(body["data"]["name"], "Alfa OK");
    EXPECT_GT(body["data"]["id"].get<int>(), 0);
}

TEST_F(ClubApiTest, PostClubPersistsToDb) {
    json resp = postClub("Beta SK");
    int id = resp["data"]["id"].get<int>();
    auto row = clubRepo.findById(id);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->name, "Beta SK");
}

TEST_F(ClubApiTest, GetClubById) {
    json resp = postClub("Gamma IF");
    int id = resp["data"]["id"].get<int>();

    auto res = client.Get("/api/v1/clubs/" + std::to_string(id));
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"]["id"].get<int>(), id);
    EXPECT_EQ(body["data"]["name"], "Gamma IF");
}

TEST_F(ClubApiTest, GetClubByIdNotFound) {
    auto res = client.Get("/api/v1/clubs/99999");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404);
    json body = json::parse(res->body);
    EXPECT_TRUE(body.contains("error"));
    EXPECT_EQ(body["error"]["code"], 404);
}

TEST_F(ClubApiTest, GetClubsListAll) {
    postClub("Club A");
    postClub("Club B");

    auto res = client.Get("/api/v1/clubs");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_GE(body["data"].size(), 2u);
}

TEST_F(ClubApiTest, GetClubsFilterByName) {
    postClub("Alpha OK");
    postClub("Beta SK");

    auto res = client.Get("/api/v1/clubs?name=Alpha");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"].size(), 1u);
    EXPECT_EQ(body["data"][0]["name"], "Alpha OK");
}

TEST_F(ClubApiTest, PutClubUpdatesName) {
    json resp = postClub("OldName IF");
    int id = resp["data"]["id"].get<int>();

    json upd = {{"name", "NewName IF"}};
    auto res = client.Put("/api/v1/clubs/" + std::to_string(id),
                          upd.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"]["name"], "NewName IF");

    // Verify DB updated
    auto row = clubRepo.findById(id);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->name, "NewName IF");
}

TEST_F(ClubApiTest, PutClubNotFound) {
    json upd = {{"name", "Whatever"}};
    auto res = client.Put("/api/v1/clubs/99999", upd.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404);
}

TEST_F(ClubApiTest, DeleteClub) {
    json resp = postClub("ToDelete SK");
    int id = resp["data"]["id"].get<int>();

    auto res = client.Delete("/api/v1/clubs/" + std::to_string(id));
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"]["deleted"].get<int>(), id);

    // Verify removed from event
    EXPECT_EQ(oe.getClub(id), nullptr);
    // Verify removed from DB
    EXPECT_FALSE(clubRepo.findById(id).has_value());
}

TEST_F(ClubApiTest, DeleteClubNotFound) {
    auto res = client.Delete("/api/v1/clubs/99999");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404);
}

TEST_F(ClubApiTest, PostClubMissingNameReturns400) {
    auto res = client.Post("/api/v1/clubs", R"({})", "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
    json body = json::parse(res->body);
    EXPECT_EQ(body["error"]["code"], 400);
}

TEST_F(ClubApiTest, PostClubInvalidJsonReturns400) {
    auto res = client.Post("/api/v1/clubs", "not-json", "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

// ─── Controls API ──────────────────────────────────────────────────────────────

class ControlApiTest : public ::testing::Test {
protected:
    SQLiteDatabase     db;
    oEvent             oe;
    ControlRepository  ctrlRepo{db};
    HttpServer         server;
    httplib::Client    client{"localhost", CONTROLS_PORT};

    void SetUp() override {
        db.open(":memory:");
        DbMigrationManager mgr(db);
        mgr.applyMigrations(SchemaV4::migrations());

        oe.newCompetition(L"TestComp");

        auto router = server.router();
        registerControlRoutes(router, oe, ctrlRepo);

        server.start("localhost", CONTROLS_PORT);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    void TearDown() override {
        server.stop();
    }

    json postControl(const std::string& name, int number = 0, int status = 0) {
        json req = {{"name", name}, {"number", number}, {"status", status}};
        auto res = client.Post("/api/v1/controls", req.dump(), "application/json");
        EXPECT_TRUE(res != nullptr);
        return json::parse(res->body);
    }
};

TEST_F(ControlApiTest, PostControlCreatesControl) {
    auto res = client.Post("/api/v1/controls",
                           R"({"name":"Control 1","number":101})",
                           "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_TRUE(body.contains("data"));
    EXPECT_EQ(body["data"]["name"], "Control 1");
    EXPECT_EQ(body["data"]["number"].get<int>(), 101);
    EXPECT_GT(body["data"]["id"].get<int>(), 0);
}

TEST_F(ControlApiTest, PostControlPersistsToDb) {
    json resp = postControl("Finish", 250);
    int id = resp["data"]["id"].get<int>();
    auto row = ctrlRepo.findById(id);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->id, id);
}

TEST_F(ControlApiTest, GetControlById) {
    json resp = postControl("Start", 0);
    int id = resp["data"]["id"].get<int>();

    auto res = client.Get("/api/v1/controls/" + std::to_string(id));
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"]["id"].get<int>(), id);
    EXPECT_EQ(body["data"]["name"], "Start");
}

TEST_F(ControlApiTest, GetControlByIdNotFound) {
    auto res = client.Get("/api/v1/controls/99999");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404);
    json body = json::parse(res->body);
    EXPECT_EQ(body["error"]["code"], 404);
}

TEST_F(ControlApiTest, GetControlsListAll) {
    postControl("Ctrl A", 100);
    postControl("Ctrl B", 101);

    auto res = client.Get("/api/v1/controls");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_GE(body["data"].size(), 2u);
}

TEST_F(ControlApiTest, GetControlsFilterByName) {
    postControl("Alpha ctrl", 100);
    postControl("Beta ctrl", 200);

    auto res = client.Get("/api/v1/controls?name=Alpha");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"].size(), 1u);
    EXPECT_EQ(body["data"][0]["name"], "Alpha ctrl");
}

TEST_F(ControlApiTest, PutControlUpdates) {
    json resp = postControl("Old ctrl", 50);
    int id = resp["data"]["id"].get<int>();

    json upd = {{"name", "New ctrl"}, {"number", 99}};
    auto res = client.Put("/api/v1/controls/" + std::to_string(id),
                          upd.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"]["name"], "New ctrl");
    EXPECT_EQ(body["data"]["number"].get<int>(), 99);
}

TEST_F(ControlApiTest, PutControlNotFound) {
    json upd = {{"name", "X"}};
    auto res = client.Put("/api/v1/controls/99999", upd.dump(), "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404);
}

TEST_F(ControlApiTest, DeleteControl) {
    json resp = postControl("ToDelete", 77);
    int id = resp["data"]["id"].get<int>();

    auto res = client.Delete("/api/v1/controls/" + std::to_string(id));
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"]["deleted"].get<int>(), id);

    EXPECT_EQ(oe.getControl(id), nullptr);
    EXPECT_FALSE(ctrlRepo.findById(id).has_value());
}

TEST_F(ControlApiTest, DeleteControlNotFound) {
    auto res = client.Delete("/api/v1/controls/99999");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404);
}

TEST_F(ControlApiTest, PostControlMissingNameReturns400) {
    auto res = client.Post("/api/v1/controls", R"({"number":100})", "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}

TEST_F(ControlApiTest, PostControlInvalidJsonReturns400) {
    auto res = client.Post("/api/v1/controls", "bad json", "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 400);
}
