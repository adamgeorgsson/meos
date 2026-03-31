// US-005d: REST API tests for runners, teams, and competition metadata
#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include "SQLiteDatabase.h"
#include "DbMigration.h"
#include "RunnerRepository.h"
#include "TeamRepository.h"
#include "EventRepository.h"
#include "ClubRepository.h"
#include "ClassRepository.h"
#include "RunnersApi.h"
#include "TeamsApi.h"
#include "CompetitionApi.h"
#include "ClubsApi.h"
#include "ClassesApi.h"
#include "HttpServer.h"
#include "oEvent.h"

using namespace meos_net;
using namespace meos_db;
using json = nlohmann::json;

static constexpr int RUNNERS_PORT     = 18085;
static constexpr int TEAMS_PORT       = 18086;
static constexpr int COMPETITION_PORT = 18087;

// ─── Runners API ──────────────────────────────────────────────────────────────

class RunnerApiTest : public ::testing::Test {
protected:
    SQLiteDatabase   db;
    oEvent           oe;
    RunnerRepository runnerRepo{db};
    ClubRepository   clubRepo{db};
    ClassRepository  classRepo{db};
    HttpServer       server;
    httplib::Client  client{"localhost", RUNNERS_PORT};

    void SetUp() override {
        db.open(":memory:");
        DbMigrationManager mgr(db);
        mgr.applyMigrations(SchemaV4::migrations());

        oe.newCompetition(L"TestComp");

        auto router = server.router();
        registerRunnerRoutes(router, oe, runnerRepo);
        registerClubRoutes(router, oe, clubRepo);
        registerClassRoutes(router, oe, classRepo);

        server.start("localhost", RUNNERS_PORT);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    void TearDown() override { server.stop(); }

    json postRunner(const std::string& name, int clubId = 0, int classId = 0) {
        json req = {{"name", name}};
        if (clubId  > 0) req["clubId"]  = clubId;
        if (classId > 0) req["classId"] = classId;
        auto res = client.Post("/api/v1/runners", req.dump(), "application/json");
        return json::parse(res->body);
    }
};

TEST_F(RunnerApiTest, PostRunnerCreates) {
    auto resp = postRunner("Anna Svensson");
    ASSERT_TRUE(resp.contains("data"));
    EXPECT_GT(resp["data"]["id"].get<int>(), 0);
    EXPECT_EQ(resp["data"]["name"], "Anna Svensson");
}

TEST_F(RunnerApiTest, PostRunnerPersistsToDb) {
    auto resp = postRunner("Bo Eriksson");
    int id = resp["data"]["id"].get<int>();

    auto row = runnerRepo.findById(id);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->name, "Bo Eriksson");
}

TEST_F(RunnerApiTest, GetRunnerById) {
    auto created = postRunner("Cecilia Holm");
    int id = created["data"]["id"].get<int>();

    auto res = client.Get(("/api/v1/runners/" + std::to_string(id)).c_str());
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_EQ(resp["data"]["name"], "Cecilia Holm");
}

TEST_F(RunnerApiTest, GetRunnerByIdNotFound) {
    auto res = client.Get("/api/v1/runners/99999");
    EXPECT_EQ(res->status, 404);
}

TEST_F(RunnerApiTest, GetRunnersListAll) {
    postRunner("Runner One");
    postRunner("Runner Two");

    auto res = client.Get("/api/v1/runners");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_GE(resp["data"]["total"].get<int>(), 2);
    EXPECT_FALSE(resp["data"]["data"].empty());
}

TEST_F(RunnerApiTest, GetRunnersFilterByName) {
    postRunner("Alpha Runner");
    postRunner("Beta Runner");

    auto res = client.Get("/api/v1/runners?name=Alpha");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_EQ(resp["data"]["total"].get<int>(), 1);
    EXPECT_EQ(resp["data"]["data"][0]["name"], "Alpha Runner");
}

TEST_F(RunnerApiTest, GetRunnersPagination) {
    for (int i = 0; i < 5; i++)
        postRunner("Runner " + std::to_string(i));

    auto res = client.Get("/api/v1/runners?page=1&pageSize=2");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_EQ(resp["data"]["page"].get<int>(), 1);
    EXPECT_EQ(resp["data"]["pageSize"].get<int>(), 2);
    EXPECT_EQ(resp["data"]["data"].size(), 2u);
    EXPECT_GE(resp["data"]["total"].get<int>(), 5);
}

TEST_F(RunnerApiTest, PutRunnerUpdates) {
    auto created = postRunner("Old Name");
    int id = created["data"]["id"].get<int>();

    json update = {{"name", "New Name"}, {"cardNo", 12345}};
    auto res = client.Put(("/api/v1/runners/" + std::to_string(id)).c_str(),
                          update.dump(), "application/json");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_EQ(resp["data"]["name"],   "New Name");
    EXPECT_EQ(resp["data"]["cardNo"], 12345);
}

TEST_F(RunnerApiTest, PutRunnerUpdatesDb) {
    auto created = postRunner("Update Me");
    int id = created["data"]["id"].get<int>();

    json update = {{"name", "Updated"}};
    client.Put(("/api/v1/runners/" + std::to_string(id)).c_str(),
               update.dump(), "application/json");

    auto row = runnerRepo.findById(id);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->name, "Updated");
}

TEST_F(RunnerApiTest, PutRunnerNotFound) {
    json update = {{"name", "X"}};
    auto res = client.Put("/api/v1/runners/99999", update.dump(), "application/json");
    EXPECT_EQ(res->status, 404);
}

TEST_F(RunnerApiTest, DeleteRunner) {
    auto created = postRunner("Delete Me");
    int id = created["data"]["id"].get<int>();

    auto res = client.Delete(("/api/v1/runners/" + std::to_string(id)).c_str());
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_EQ(resp["data"]["deleted"], id);

    auto getRes = client.Get(("/api/v1/runners/" + std::to_string(id)).c_str());
    EXPECT_EQ(getRes->status, 404);
}

TEST_F(RunnerApiTest, DeleteRunnerNotFound) {
    auto res = client.Delete("/api/v1/runners/99999");
    EXPECT_EQ(res->status, 404);
}

TEST_F(RunnerApiTest, PostRunnerMissingNameReturns400) {
    json req = {{"cardNo", 1234}};
    auto res = client.Post("/api/v1/runners", req.dump(), "application/json");
    EXPECT_EQ(res->status, 400);
}

TEST_F(RunnerApiTest, PostRunnerInvalidJsonReturns400) {
    auto res = client.Post("/api/v1/runners", "not json", "application/json");
    EXPECT_EQ(res->status, 400);
}

// ─── Teams API ────────────────────────────────────────────────────────────────

class TeamApiTest : public ::testing::Test {
protected:
    SQLiteDatabase   db;
    oEvent           oe;
    TeamRepository   teamRepo{db};
    ClassRepository  classRepo{db};
    HttpServer       server;
    httplib::Client  client{"localhost", TEAMS_PORT};

    void SetUp() override {
        db.open(":memory:");
        DbMigrationManager mgr(db);
        mgr.applyMigrations(SchemaV4::migrations());

        oe.newCompetition(L"TestComp");

        auto router = server.router();
        registerTeamRoutes(router, oe, teamRepo);
        registerClassRoutes(router, oe, classRepo);

        server.start("localhost", TEAMS_PORT);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    void TearDown() override { server.stop(); }

    json postTeam(const std::string& name, int classId = 0) {
        json req = {{"name", name}};
        if (classId > 0) req["classId"] = classId;
        auto res = client.Post("/api/v1/teams", req.dump(), "application/json");
        return json::parse(res->body);
    }
};

TEST_F(TeamApiTest, PostTeamCreates) {
    auto resp = postTeam("Team Alpha");
    ASSERT_TRUE(resp.contains("data"));
    EXPECT_GT(resp["data"]["id"].get<int>(), 0);
    EXPECT_EQ(resp["data"]["name"], "Team Alpha");
}

TEST_F(TeamApiTest, PostTeamPersistsToDb) {
    auto resp = postTeam("Team Bravo");
    int id = resp["data"]["id"].get<int>();

    auto row = teamRepo.findById(id);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->name, "Team Bravo");
}

TEST_F(TeamApiTest, GetTeamById) {
    auto created = postTeam("Team Charlie");
    int id = created["data"]["id"].get<int>();

    auto res = client.Get(("/api/v1/teams/" + std::to_string(id)).c_str());
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_EQ(resp["data"]["name"], "Team Charlie");
}

TEST_F(TeamApiTest, GetTeamByIdNotFound) {
    auto res = client.Get("/api/v1/teams/99999");
    EXPECT_EQ(res->status, 404);
}

TEST_F(TeamApiTest, GetTeamsListAll) {
    postTeam("Team One");
    postTeam("Team Two");

    auto res = client.Get("/api/v1/teams");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_GE(resp["data"]["total"].get<int>(), 2);
}

TEST_F(TeamApiTest, GetTeamsFilterByName) {
    postTeam("Red Team");
    postTeam("Blue Team");

    auto res = client.Get("/api/v1/teams?name=Red");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_EQ(resp["data"]["total"].get<int>(), 1);
    EXPECT_EQ(resp["data"]["data"][0]["name"], "Red Team");
}

TEST_F(TeamApiTest, GetTeamsPagination) {
    for (int i = 0; i < 5; i++)
        postTeam("Team " + std::to_string(i));

    auto res = client.Get("/api/v1/teams?page=1&pageSize=3");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_EQ(resp["data"]["pageSize"].get<int>(), 3);
    EXPECT_EQ(resp["data"]["data"].size(), 3u);
}

TEST_F(TeamApiTest, PutTeamUpdates) {
    auto created = postTeam("Old Team");
    int id = created["data"]["id"].get<int>();

    json update = {{"name", "New Team"}};
    auto res = client.Put(("/api/v1/teams/" + std::to_string(id)).c_str(),
                          update.dump(), "application/json");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_EQ(resp["data"]["name"], "New Team");
}

TEST_F(TeamApiTest, PutTeamNotFound) {
    json update = {{"name", "X"}};
    auto res = client.Put("/api/v1/teams/99999", update.dump(), "application/json");
    EXPECT_EQ(res->status, 404);
}

TEST_F(TeamApiTest, DeleteTeam) {
    auto created = postTeam("Delete Me");
    int id = created["data"]["id"].get<int>();

    auto res = client.Delete(("/api/v1/teams/" + std::to_string(id)).c_str());
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_EQ(resp["data"]["deleted"], id);

    auto getRes = client.Get(("/api/v1/teams/" + std::to_string(id)).c_str());
    EXPECT_EQ(getRes->status, 404);
}

TEST_F(TeamApiTest, DeleteTeamNotFound) {
    auto res = client.Delete("/api/v1/teams/99999");
    EXPECT_EQ(res->status, 404);
}

TEST_F(TeamApiTest, PostTeamMissingNameReturns400) {
    json req = {{"classId", 1}};
    auto res = client.Post("/api/v1/teams", req.dump(), "application/json");
    EXPECT_EQ(res->status, 400);
}

// ─── Competition API ──────────────────────────────────────────────────────────

class CompetitionApiTest : public ::testing::Test {
protected:
    SQLiteDatabase    db;
    oEvent            oe;
    EventRepository   eventRepo{db};
    HttpServer        server;
    httplib::Client   client{"localhost", COMPETITION_PORT};

    void SetUp() override {
        db.open(":memory:");
        DbMigrationManager mgr(db);
        mgr.applyMigrations(SchemaV4::migrations());

        oe.newCompetition(L"Test Competition");
        oe.setDate(L"2024-06-15");
        oe.setZeroTime(L"10:00:00");

        auto router = server.router();
        registerCompetitionRoutes(router, oe, eventRepo);

        server.start("localhost", COMPETITION_PORT);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    void TearDown() override { server.stop(); }
};

TEST_F(CompetitionApiTest, GetCompetitionReturnsMetadata) {
    auto res = client.Get("/api/v1/competition");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    ASSERT_TRUE(resp.contains("data"));
    EXPECT_EQ(resp["data"]["name"],     "Test Competition");
    EXPECT_EQ(resp["data"]["date"],     "2024-06-15");
    EXPECT_EQ(resp["data"]["zeroTime"], "10:00:00");
}

TEST_F(CompetitionApiTest, PutCompetitionUpdatesName) {
    json update = {{"name", "Summer Sprint"}};
    auto res = client.Put("/api/v1/competition", update.dump(), "application/json");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_EQ(resp["data"]["name"], "Summer Sprint");
    EXPECT_EQ(oe.getName(), L"Summer Sprint");
}

TEST_F(CompetitionApiTest, PutCompetitionUpdatesDate) {
    json update = {{"date", "2025-01-01"}};
    auto res = client.Put("/api/v1/competition", update.dump(), "application/json");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_EQ(resp["data"]["date"], "2025-01-01");
}

TEST_F(CompetitionApiTest, PutCompetitionUpdatesZeroTime) {
    json update = {{"zeroTime", "09:30:00"}};
    auto res = client.Put("/api/v1/competition", update.dump(), "application/json");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_EQ(resp["data"]["zeroTime"], "09:30:00");
}

TEST_F(CompetitionApiTest, PutCompetitionPersistsToDb) {
    json update = {{"name", "Persisted Race"}, {"date", "2024-12-25"}};
    client.Put("/api/v1/competition", update.dump(), "application/json");

    auto row = eventRepo.load();
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(row->name, "Persisted Race");
    EXPECT_EQ(row->date, "2024-12-25");
}

TEST_F(CompetitionApiTest, PutCompetitionInvalidJsonReturns400) {
    auto res = client.Put("/api/v1/competition", "bad json", "application/json");
    EXPECT_EQ(res->status, 400);
}
