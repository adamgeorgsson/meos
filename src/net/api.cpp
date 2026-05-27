#include "api.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace meos::net {

namespace {

json clubToJson(const meos::domain::Club& c) {
    json j;
    j["id"] = c.id;
    j["name"] = c.name;
    if (c.country) j["country"] = *c.country;
    return j;
}

json makeError(int status, const std::string& message) {
    return json{{"message", message}, {"status", status}};
}

}  // namespace

void registerClubsRoutes(httplib::Server& svr, meos::db::Database& db) {
    svr.Get("/api/v1/clubs",
            [&db](const httplib::Request&, httplib::Response& res) {
                auto clubs = db.getAllClubs();
                json arr = json::array();
                for (const auto& c : clubs) arr.push_back(clubToJson(c));
                res.set_content(arr.dump(), "application/json");
            });

    svr.Get(R"(/api/v1/clubs/(\d+))",
            [&db](const httplib::Request& req, httplib::Response& res) {
                int id = std::stoi(req.matches[1]);
                auto club = db.getClubById(id);
                if (!club) {
                    res.status = 404;
                    res.set_content(makeError(404, "Not found").dump(),
                                    "application/json");
                } else {
                    res.set_content(clubToJson(*club).dump(),
                                    "application/json");
                }
            });
}

void registerControlsRoutes(httplib::Server& svr, meos::db::Database& db) {
    svr.Get("/api/v1/controls",
            [&db](const httplib::Request&, httplib::Response& res) {
                auto controls = db.getAllControls();
                json arr = json::array();
                for (const auto& c : controls) {
                    json j;
                    j["id"] = c.id;
                    j["code"] = c.code;
                    if (c.description) j["description"] = *c.description;
                    if (c.type) j["type"] = *c.type;
                    arr.push_back(j);
                }
                res.set_content(arr.dump(), "application/json");
            });

    svr.Get(R"(/api/v1/controls/(\d+))",
            [&db](const httplib::Request& req, httplib::Response& res) {
                int id = std::stoi(req.matches[1]);
                auto ctrl = db.getControlById(id);
                if (!ctrl) {
                    res.status = 404;
                    res.set_content(makeError(404, "Not found").dump(),
                                    "application/json");
                } else {
                    json j;
                    j["id"] = ctrl->id;
                    j["code"] = ctrl->code;
                    if (ctrl->description) j["description"] = *ctrl->description;
                    if (ctrl->type) j["type"] = *ctrl->type;
                    res.set_content(j.dump(), "application/json");
                }
            });
}

void registerCoursesRoutes(httplib::Server& svr, meos::db::Database& db) {
    svr.Get("/api/v1/courses",
            [&db](const httplib::Request&, httplib::Response& res) {
                auto courses = db.getAllCourses();
                json arr = json::array();
                for (const auto& c : courses) {
                    json j;
                    j["id"] = c.id;
                    j["name"] = c.name;
                    if (c.length) j["length"] = *c.length;
                    j["controls"] = c.controls;
                    arr.push_back(j);
                }
                res.set_content(arr.dump(), "application/json");
            });

    svr.Get(R"(/api/v1/courses/(\d+))",
            [&db](const httplib::Request& req, httplib::Response& res) {
                int id = std::stoi(req.matches[1]);
                auto course = db.getCourseById(id);
                if (!course) {
                    res.status = 404;
                    res.set_content(makeError(404, "Not found").dump(),
                                    "application/json");
                } else {
                    json j;
                    j["id"] = course->id;
                    j["name"] = course->name;
                    if (course->length) j["length"] = *course->length;
                    j["controls"] = course->controls;
                    res.set_content(j.dump(), "application/json");
                }
            });
}

}  // namespace meos::net
