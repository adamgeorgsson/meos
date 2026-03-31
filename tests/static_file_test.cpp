// US-011: Static file serving from C++ server
#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <string>
#include <thread>
#include "HttpServer.h"
#include "ApiRouter.h"

using namespace meos_net;
using json = nlohmann::json;

static constexpr int STATIC_PORT = 18090;

// Creates a temp directory with test web assets and returns its path.
static std::string createWebRoot() {
    char tmpl[] = "/tmp/meos_web_XXXXXX";
    const char* dir = mkdtemp(tmpl);
    EXPECT_NE(dir, nullptr) << "mkdtemp failed";
    std::string root(dir);

    // Write index.html
    {
        std::ofstream f(root + "/index.html");
        f << "<!DOCTYPE html><html><body><div id=\"root\"></div></body></html>";
    }
    // Write a JS bundle
    {
        std::ofstream f(root + "/app.js");
        f << "console.log('meos');";
    }
    // Write a CSS file
    {
        std::ofstream f(root + "/style.css");
        f << "body { margin: 0; }";
    }
    // Write an SVG
    {
        std::ofstream f(root + "/icon.svg");
        f << "<svg xmlns='http://www.w3.org/2000/svg'></svg>";
    }
    return root;
}

// ─── StaticFileTest fixture ─────────────────────────────────────────────────

class StaticFileTest : public ::testing::Test {
protected:
    std::string webRoot;
    HttpServer server;
    httplib::Client client{"localhost", STATIC_PORT};

    void SetUp() override {
        webRoot = createWebRoot();
        auto r = server.router();
        r.GET("/ping", [](const httplib::Request&) {
            return json{{"data", "pong"}};
        });
        server.serveStatic(webRoot);
        server.start("localhost", STATIC_PORT);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        client.set_connection_timeout(2);
        client.set_read_timeout(2);
    }

    void TearDown() override {
        server.stop();
        // Remove temp files
        std::string cmd = "rm -rf " + webRoot;
        (void)system(cmd.c_str());
    }
};

// ─── Static asset serving ───────────────────────────────────────────────────

TEST_F(StaticFileTest, ServesIndexHtml) {
    auto res = client.Get("/index.html");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_NE(res->body.find("root"), std::string::npos);
}

TEST_F(StaticFileTest, ServesIndexHtmlAtRoot) {
    auto res = client.Get("/");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_NE(res->body.find("root"), std::string::npos);
}

TEST_F(StaticFileTest, ServesJavaScript) {
    auto res = client.Get("/app.js");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_NE(res->body.find("meos"), std::string::npos);
}

TEST_F(StaticFileTest, ServesCSS) {
    auto res = client.Get("/style.css");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_NE(res->body.find("margin"), std::string::npos);
}

TEST_F(StaticFileTest, ServesSVG) {
    auto res = client.Get("/icon.svg");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
}

// ─── MIME type checks ────────────────────────────────────────────────────────

TEST_F(StaticFileTest, HtmlMimeType) {
    auto res = client.Get("/index.html");
    ASSERT_NE(res, nullptr);
    EXPECT_NE(res->get_header_value("Content-Type").find("text/html"),
              std::string::npos);
}

TEST_F(StaticFileTest, JavaScriptMimeType) {
    auto res = client.Get("/app.js");
    ASSERT_NE(res, nullptr);
    // Accept application/javascript or text/javascript
    std::string ct = res->get_header_value("Content-Type");
    bool isJs = ct.find("javascript") != std::string::npos;
    EXPECT_TRUE(isJs) << "Expected javascript MIME, got: " << ct;
}

TEST_F(StaticFileTest, CssMimeType) {
    auto res = client.Get("/style.css");
    ASSERT_NE(res, nullptr);
    EXPECT_NE(res->get_header_value("Content-Type").find("css"),
              std::string::npos);
}

TEST_F(StaticFileTest, SvgMimeType) {
    auto res = client.Get("/icon.svg");
    ASSERT_NE(res, nullptr);
    std::string ct = res->get_header_value("Content-Type");
    EXPECT_NE(ct.find("svg"), std::string::npos) << "Got: " << ct;
}

// ─── SPA fallback ───────────────────────────────────────────────────────────

TEST_F(StaticFileTest, SpaFallbackForUnknownRoute) {
    auto res = client.Get("/some/deep/page");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_NE(res->body.find("root"), std::string::npos)
        << "SPA fallback should serve index.html";
}

TEST_F(StaticFileTest, SpaFallbackForAnotherPath) {
    auto res = client.Get("/runners/42");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    EXPECT_NE(res->body.find("root"), std::string::npos);
}

// ─── API routes still work after serveStatic ────────────────────────────────

TEST_F(StaticFileTest, ApiRouteStillWorks) {
    auto res = client.Get("/api/v1/ping");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 200);
    auto body = json::parse(res->body);
    EXPECT_EQ(body["data"], "pong");
}

TEST_F(StaticFileTest, ApiNotFoundReturnsJson) {
    auto res = client.Get("/api/v1/nonexistent");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->status, 404);
    // Should be JSON, not SPA fallback
    auto body = json::parse(res->body, nullptr, false);
    EXPECT_FALSE(body.is_discarded());
    EXPECT_TRUE(body.contains("error"));
}
