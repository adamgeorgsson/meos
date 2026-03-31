// US-005a: REST API framework tests
#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <thread>
#include "ApiError.h"
#include "ApiRouter.h"
#include "HttpServer.h"
#include "JsonUtils.h"

using namespace meos_net;
using json = nlohmann::json;

// ─── ApiException ─────────────────────────────────────────────────────────────

TEST(ApiError, BadRequest) {
    auto ex = badRequest("missing field");
    EXPECT_EQ(ex.httpStatus(), 400);
    EXPECT_EQ(ex.message(), "missing field");
}

TEST(ApiError, NotFound) {
    auto ex = notFound("item not found");
    EXPECT_EQ(ex.httpStatus(), 404);
}

TEST(ApiError, Conflict) {
    auto ex = conflict("duplicate");
    EXPECT_EQ(ex.httpStatus(), 409);
}

TEST(ApiError, InternalError) {
    auto ex = internalError("oops");
    EXPECT_EQ(ex.httpStatus(), 500);
}

// ─── JsonUtils ────────────────────────────────────────────────────────────────

TEST(JsonUtils, OkResponseWrapsData) {
    json resp = okResponse(json{{"id", 1}});
    EXPECT_TRUE(resp.contains("data"));
    EXPECT_EQ(resp["data"]["id"], 1);
}

TEST(JsonUtils, ErrorResponseFromException) {
    auto ex = badRequest("bad input");
    json resp = errorResponse(ex);
    EXPECT_TRUE(resp.contains("error"));
    EXPECT_EQ(resp["error"]["code"], 400);
    EXPECT_EQ(resp["error"]["message"], "bad input");
}

TEST(JsonUtils, ErrorResponseFromString) {
    json resp = errorResponse("something failed");
    EXPECT_EQ(resp["error"]["code"], 500);
    EXPECT_EQ(resp["error"]["message"], "something failed");
}

TEST(JsonUtils, ParseBodyValid) {
    json body = parseBody(R"({"name":"test"})");
    EXPECT_EQ(body["name"], "test");
}

TEST(JsonUtils, ParseBodyInvalidThrows) {
    EXPECT_THROW(parseBody("not json"), ApiException);
    try {
        parseBody("{bad}");
    } catch (const ApiException& ex) {
        EXPECT_EQ(ex.httpStatus(), 400);
    }
}

TEST(JsonUtils, RequireStringPresent) {
    json body = json{{"name", "hello"}};
    EXPECT_EQ(requireString(body, "name"), "hello");
}

TEST(JsonUtils, RequireStringMissingThrows) {
    json body = json{};
    EXPECT_THROW(requireString(body, "name"), ApiException);
}

TEST(JsonUtils, OptionalInt) {
    json body = json{{"count", 5}};
    EXPECT_EQ(optionalInt(body, "count"), 5);
    EXPECT_EQ(optionalInt(body, "missing", 42), 42);
}

// ─── HttpServer + ApiRouter integration ────────────────────────────────────────

static constexpr int TEST_PORT = 18080;

class HttpServerTest : public ::testing::Test {
protected:
    HttpServer server;
    httplib::Client client{"localhost", TEST_PORT};

    void SetUp() override {
        auto router = server.router();

        // Simple GET endpoint
        router.GET("/ping", [](const Request&) -> json {
            return okResponse(json{{"pong", true}});
        });

        // POST endpoint that parses JSON body
        router.POST("/echo", [](const Request& req) -> json {
            json body = parseBody(req.body);
            return okResponse(body);
        });

        // Endpoint that throws ApiException
        router.GET("/error", [](const Request&) -> json {
            throw notFound("resource not found");
            return {};
        });

        // Endpoint that throws std::runtime_error
        router.GET("/crash", [](const Request&) -> json {
            throw std::runtime_error("unexpected failure");
            return {};
        });

        server.start("localhost", TEST_PORT);
        // Give server a moment to bind
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    void TearDown() override {
        server.stop();
    }
};

TEST_F(HttpServerTest, GetPingReturns200) {
    auto res = client.Get("/api/v1/ping");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"]["pong"], true);
}

TEST_F(HttpServerTest, PostEchoReturns200) {
    auto res = client.Post("/api/v1/echo", R"({"msg":"hello"})", "application/json");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 200);
    json body = json::parse(res->body);
    EXPECT_EQ(body["data"]["msg"], "hello");
}

TEST_F(HttpServerTest, ApiExceptionReturns404) {
    auto res = client.Get("/api/v1/error");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404);
    json body = json::parse(res->body);
    EXPECT_EQ(body["error"]["code"], 404);
    EXPECT_EQ(body["error"]["message"], "resource not found");
}

TEST_F(HttpServerTest, UnhandledExceptionReturns500) {
    auto res = client.Get("/api/v1/crash");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 500);
    json body = json::parse(res->body);
    EXPECT_EQ(body["error"]["code"], 500);
}

TEST_F(HttpServerTest, UnknownRouteReturns404) {
    auto res = client.Get("/api/v1/does-not-exist");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404);
    json body = json::parse(res->body);
    EXPECT_TRUE(body.contains("error"));
}

TEST_F(HttpServerTest, ApiVersioningPrefixRequired) {
    // Requests without /api/v1 prefix should not find the route
    auto res = client.Get("/ping");
    ASSERT_TRUE(res);
    EXPECT_EQ(res->status, 404);
}

TEST_F(HttpServerTest, ContentTypeIsJson) {
    auto res = client.Get("/api/v1/ping");
    ASSERT_TRUE(res);
    EXPECT_NE(res->get_header_value("Content-Type").find("application/json"), std::string::npos);
}

TEST_F(HttpServerTest, IsRunningAfterStart) {
    EXPECT_TRUE(server.isRunning());
}
