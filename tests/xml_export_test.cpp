#include <gtest/gtest.h>
#include <httplib.h>

#include <string>
#include <thread>

#include "api.h"
#include "database.h"
#include "seed.h"

static constexpr int TEST_PORT = 18016;

class XmlExportTest : public ::testing::Test {
protected:
    meos::db::Database db_{":memory:"};
    httplib::Server svr_;
    std::thread server_thread_;

    void SetUp() override {
        db_.createTables();
        meos::db::seedIfEmpty(db_);
        meos::net::registerXmlExportRoutes(svr_, db_);
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

TEST_F(XmlExportTest, ResultsExport_ContentTypeIsApplicationXml) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/results/export/xml");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_EQ(res->get_header_value("Content-Type"), "application/xml");
}

TEST_F(XmlExportTest, ResultsExport_ContainsIofNamespace) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/results/export/xml");
    ASSERT_NE(res, nullptr);
    EXPECT_NE(res->body.find("http://www.orienteering.org/datastandard/3.0"),
              std::string::npos);
}

TEST_F(XmlExportTest, ResultsExport_ContainsResultListElement) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/results/export/xml");
    ASSERT_NE(res, nullptr);
    EXPECT_NE(res->body.find("<ResultList"), std::string::npos);
    EXPECT_NE(res->body.find("</ResultList>"), std::string::npos);
}

TEST_F(XmlExportTest, StartListExport_ContentTypeIsApplicationXml) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/startlist/export/xml");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_EQ(res->get_header_value("Content-Type"), "application/xml");
}

TEST_F(XmlExportTest, StartListExport_ContainsIofNamespace) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/startlist/export/xml");
    ASSERT_NE(res, nullptr);
    EXPECT_NE(res->body.find("http://www.orienteering.org/datastandard/3.0"),
              std::string::npos);
}

TEST_F(XmlExportTest, StartListExport_ContainsStartListElement) {
    httplib::Client cli("127.0.0.1", TEST_PORT);
    auto res = cli.Get("/api/v1/startlist/export/xml");
    ASSERT_NE(res, nullptr);
    EXPECT_NE(res->body.find("<StartList"), std::string::npos);
    EXPECT_NE(res->body.find("</StartList>"), std::string::npos);
}
