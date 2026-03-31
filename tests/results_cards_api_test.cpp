// US-006: REST API tests for results, startlist, card readout, and runner status
#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include "SQLiteDatabase.h"
#include "DbMigration.h"
#include "RunnerRepository.h"
#include "ClubRepository.h"
#include "ClassRepository.h"
#include "CourseRepository.h"
#include "ControlRepository.h"
#include "RunnersApi.h"
#include "ClubsApi.h"
#include "ClassesApi.h"
#include "CoursesApi.h"
#include "ControlsApi.h"
#include "ResultsApi.h"
#include "CardsApi.h"
#include "HttpServer.h"
#include "oEvent.h"
#include "oRunner.h"
#include "oClass.h"

using namespace meos_net;
using namespace meos_db;
using json = nlohmann::json;

static constexpr int RESULTS_PORT = 18088;
static constexpr int CARDS_PORT   = 18089;

// ─── Results + Startlist API ─────────────────────────────────────────────────

class ResultsApiTest : public ::testing::Test {
protected:
    SQLiteDatabase    db;
    oEvent            oe;
    RunnerRepository  runnerRepo{db};
    ClubRepository    clubRepo{db};
    ClassRepository   classRepo{db};
    HttpServer        server;
    httplib::Client   client{"localhost", RESULTS_PORT};

    void SetUp() override {
        db.open(":memory:");
        DbMigrationManager mgr(db);
        mgr.applyMigrations(SchemaV4::migrations());

        oe.newCompetition(L"TestComp");

        auto router = server.router();
        registerRunnerRoutes(router, oe, runnerRepo);
        registerClubRoutes(router, oe, clubRepo);
        registerClassRoutes(router, oe, classRepo);
        registerResultsRoutes(router, oe);

        server.start("localhost", RESULTS_PORT);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    void TearDown() override { server.stop(); }

    // Helper: create a class and return its ID
    int createClass(const std::string& name) {
        json req = {{"name", name}};
        auto res = client.Post("/api/v1/classes", req.dump(), "application/json");
        return json::parse(res->body)["data"]["id"].get<int>();
    }

    // Helper: create a runner in a class, set start/finish/status in memory
    pRunner createRunner(const std::wstring& name, int classId, int startTime, int finishTime, RunnerStatus st) {
        oRunner r(&oe);
        r.setName(name, false);
        r.setClassId(classId, false);
        pRunner pr = oe.addRunner(r);
        if (pr) {
            pr->setStartTime(startTime, true, oBase::ChangeType::Quiet, false);
            pr->setFinishTime(finishTime);
            pr->setStatus(st, true, oBase::ChangeType::Quiet, false);
        }
        return pr;
    }
};

TEST_F(ResultsApiTest, GetResultsEmpty) {
    auto res = client.Get("/api/v1/results");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    ASSERT_TRUE(resp.contains("data"));
    EXPECT_TRUE(resp["data"].is_array());
    EXPECT_EQ(resp["data"].size(), 0u);
}

TEST_F(ResultsApiTest, GetResultsWithRunners) {
    int cid = createClass("Elite");
    // Start=3600 (1h), Finish=4800 (1h20m) → running time = 1200
    createRunner(L"Alice", cid, 3600, 4800, StatusOK);
    // Start=3700, Finish=5100 → running time = 1400
    createRunner(L"Bob",   cid, 3700, 5100, StatusOK);

    auto res = client.Get("/api/v1/results");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    ASSERT_TRUE(resp["data"].is_array());
    ASSERT_EQ(resp["data"].size(), 1u);

    auto& clsResult = resp["data"][0];
    EXPECT_EQ(clsResult["classId"].get<int>(), cid);
    EXPECT_EQ(clsResult["className"], "Elite");

    auto& results = clsResult["results"];
    ASSERT_EQ(results.size(), 2u);
    // Alice (1200s) should be before Bob (1400s)
    EXPECT_EQ(results[0]["name"], "Alice");
    EXPECT_EQ(results[1]["name"], "Bob");
    EXPECT_EQ(results[0]["place"].get<int>(), 1);
    EXPECT_EQ(results[1]["place"].get<int>(), 2);
}

TEST_F(ResultsApiTest, GetResultsFilterByClass) {
    int cid1 = createClass("Elite");
    int cid2 = createClass("Veteran");
    createRunner(L"Alice",   cid1, 3600, 4800, StatusOK);
    createRunner(L"Charlie", cid2, 3600, 5200, StatusOK);

    auto res = client.Get(("/api/v1/results?classId=" + std::to_string(cid1)).c_str());
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    ASSERT_EQ(resp["data"].size(), 1u);
    EXPECT_EQ(resp["data"][0]["classId"].get<int>(), cid1);
}

TEST_F(ResultsApiTest, GetResultsDNFRunnerHasZeroPlace) {
    int cid = createClass("Elite");
    createRunner(L"Alice", cid, 3600, 4800, StatusOK);
    createRunner(L"Dave",  cid, 3600,    0, StatusDNF);

    auto res = client.Get("/api/v1/results");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    auto& results = resp["data"][0]["results"];
    ASSERT_EQ(results.size(), 2u);

    // Find Dave
    for (auto& r : results) {
        if (r["name"] == "Dave") {
            EXPECT_EQ(r["place"].get<int>(), 0);
        }
    }
}

TEST_F(ResultsApiTest, GetStartlistEmpty) {
    auto res = client.Get("/api/v1/startlist");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_TRUE(resp["data"].is_array());
}

TEST_F(ResultsApiTest, GetStartlistReturnsRunners) {
    int cid = createClass("Elite");
    createRunner(L"Alice", cid, 3600, 0, StatusUnknown);
    createRunner(L"Bob",   cid, 3700, 0, StatusUnknown);

    auto res = client.Get("/api/v1/startlist");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    ASSERT_GE(resp["data"].size(), 2u);

    // Check fields exist
    auto& entry = resp["data"][0];
    EXPECT_TRUE(entry.contains("id"));
    EXPECT_TRUE(entry.contains("name"));
    EXPECT_TRUE(entry.contains("classId"));
    EXPECT_TRUE(entry.contains("startTime"));
}

TEST_F(ResultsApiTest, GetStartlistSortedByClassThenStartTime) {
    int cid1 = createClass("Elite");
    int cid2 = createClass("Junior");
    createRunner(L"Alice",  cid1, 3700, 0, StatusUnknown);
    createRunner(L"Bob",    cid1, 3600, 0, StatusUnknown);
    createRunner(L"Charlie",cid2, 3500, 0, StatusUnknown);

    auto res = client.Get(("/api/v1/startlist?classId=" + std::to_string(cid1)).c_str());
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    auto& data = resp["data"];
    ASSERT_EQ(data.size(), 2u);
    EXPECT_EQ(data[0]["name"], "Bob");   // startTime 3600 < 3700
    EXPECT_EQ(data[1]["name"], "Alice");
}

// ─── Cards + Status API ───────────────────────────────────────────────────────

class CardsApiTest : public ::testing::Test {
protected:
    SQLiteDatabase    db;
    oEvent            oe;
    RunnerRepository  runnerRepo{db};
    ClubRepository    clubRepo{db};
    ClassRepository   classRepo{db};
    HttpServer        server;
    httplib::Client   client{"localhost", CARDS_PORT};

    void SetUp() override {
        db.open(":memory:");
        DbMigrationManager mgr(db);
        mgr.applyMigrations(SchemaV4::migrations());

        oe.newCompetition(L"TestComp");

        auto router = server.router();
        registerRunnerRoutes(router, oe, runnerRepo);
        registerClubRoutes(router, oe, clubRepo);
        registerClassRoutes(router, oe, classRepo);
        registerCardsRoutes(router, oe);

        server.start("localhost", CARDS_PORT);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    void TearDown() override { server.stop(); }

    pRunner createRunner(const std::wstring& name, int cardNo) {
        oRunner r(&oe);
        r.setName(name, false);
        pRunner pr = oe.addRunner(r);
        if (pr) pr->setCardNo(cardNo, false, false);
        return pr;
    }
};

TEST_F(CardsApiTest, PostCardNoRunner) {
    json body = {{"cardNo", 12345}, {"startTime", 3600}, {"finishTime", 4800}};
    auto res = client.Post("/api/v1/cards", body.dump(), "application/json");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_GT(resp["data"]["cardId"].get<int>(), 0);
    EXPECT_EQ(resp["data"]["cardNo"].get<int>(), 12345);
    EXPECT_TRUE(resp["data"]["runnerId"].is_null());
}

TEST_F(CardsApiTest, PostCardLinksToRunner) {
    pRunner pr = createRunner(L"Anna Svensson", 55100);
    ASSERT_TRUE(pr != nullptr);

    json body = {
        {"cardNo", 55100},
        {"startTime", 3600},
        {"finishTime", 5400},
        {"punches", json::array()}
    };
    auto res = client.Post("/api/v1/cards", body.dump(), "application/json");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_EQ(resp["data"]["runnerId"].get<int>(), pr->getId());
    EXPECT_EQ(resp["data"]["runnerName"], "Anna Svensson");
    // Running time = 5400 - 3600 = 1800 — no course so may be 0
    EXPECT_TRUE(resp["data"].contains("status"));
}

TEST_F(CardsApiTest, PostCardMissingCardNo) {
    json body = {{"startTime", 3600}};
    auto res = client.Post("/api/v1/cards", body.dump(), "application/json");
    EXPECT_EQ(res->status, 400);
}

TEST_F(CardsApiTest, PostCardWithPunches) {
    pRunner pr = createRunner(L"Bob Lindqvist", 55200);
    ASSERT_TRUE(pr != nullptr);

    json punches = json::array();
    punches.push_back({{"type", 31}, {"time", 3700}});
    punches.push_back({{"type", 32}, {"time", 3800}});
    punches.push_back({{"type", 33}, {"time", 3900}});

    json body = {
        {"cardNo", 55200},
        {"startTime", 3600},
        {"finishTime", 4200},
        {"punches", punches}
    };
    auto res = client.Post("/api/v1/cards", body.dump(), "application/json");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_EQ(resp["data"]["runnerId"].get<int>(), pr->getId());
}

TEST_F(CardsApiTest, PostStatusDNS) {
    pRunner pr = createRunner(L"Charlie Ek", 55300);
    ASSERT_TRUE(pr != nullptr);
    int rid = pr->getId();

    json body = {{"status", "DNS"}};
    auto res = client.Post(("/api/v1/runners/" + std::to_string(rid) + "/status").c_str(),
                            body.dump(), "application/json");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_EQ(resp["data"]["status"], "DNS");
    EXPECT_EQ(pr->getStatus(), StatusDNS);
}

TEST_F(CardsApiTest, PostStatusDNF) {
    pRunner pr = createRunner(L"Diana Berg", 55400);
    ASSERT_TRUE(pr != nullptr);
    int rid = pr->getId();

    json body = {{"status", "DNF"}};
    auto res = client.Post(("/api/v1/runners/" + std::to_string(rid) + "/status").c_str(),
                            body.dump(), "application/json");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_EQ(resp["data"]["status"], "DNF");
}

TEST_F(CardsApiTest, PostStatusDSQ) {
    pRunner pr = createRunner(L"Erik Strand", 55500);
    ASSERT_TRUE(pr != nullptr);
    int rid = pr->getId();

    json body = {{"status", "DSQ"}};
    auto res = client.Post(("/api/v1/runners/" + std::to_string(rid) + "/status").c_str(),
                            body.dump(), "application/json");
    ASSERT_EQ(res->status, 200);
    auto resp = json::parse(res->body);
    EXPECT_EQ(resp["data"]["status"], "DSQ");
    EXPECT_EQ(pr->getStatus(), StatusDQ);
}

TEST_F(CardsApiTest, PostStatusUnknownRunner) {
    json body = {{"status", "DNS"}};
    auto res = client.Post("/api/v1/runners/99999/status", body.dump(), "application/json");
    EXPECT_EQ(res->status, 404);
}

TEST_F(CardsApiTest, PostStatusInvalid) {
    pRunner pr = createRunner(L"Fred Holm", 55600);
    ASSERT_TRUE(pr != nullptr);

    json body = {{"status", "INVALID"}};
    auto res = client.Post(("/api/v1/runners/" + std::to_string(pr->getId()) + "/status").c_str(),
                            body.dump(), "application/json");
    EXPECT_EQ(res->status, 400);
}
