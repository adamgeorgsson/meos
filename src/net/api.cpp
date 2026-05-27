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

void registerClassesRoutes(httplib::Server& svr, meos::db::Database& db) {
    svr.Get("/api/v1/classes",
            [&db](const httplib::Request&, httplib::Response& res) {
                auto classes = db.getAllClasses();
                json arr = json::array();
                for (const auto& c : classes) {
                    json j;
                    j["id"] = c.id;
                    j["name"] = c.name;
                    if (c.courseId) j["courseId"] = *c.courseId;
                    if (c.startMethod) j["startMethod"] = *c.startMethod;
                    arr.push_back(j);
                }
                res.set_content(arr.dump(), "application/json");
            });

    svr.Get(R"(/api/v1/classes/(\d+))",
            [&db](const httplib::Request& req, httplib::Response& res) {
                int id = std::stoi(req.matches[1]);
                auto cls = db.getClassById(id);
                if (!cls) {
                    res.status = 404;
                    res.set_content(makeError(404, "Not found").dump(),
                                    "application/json");
                } else {
                    json j;
                    j["id"] = cls->id;
                    j["name"] = cls->name;
                    if (cls->courseId) j["courseId"] = *cls->courseId;
                    if (cls->startMethod) j["startMethod"] = *cls->startMethod;
                    res.set_content(j.dump(), "application/json");
                }
            });
}

namespace {

json runnerToJson(const meos::domain::Runner& r) {
    json j;
    j["id"] = r.id;
    j["name"] = r.name;
    if (r.clubId) j["clubId"] = *r.clubId;
    if (r.classId) j["classId"] = *r.classId;
    if (r.startTime) j["startTime"] = *r.startTime;
    if (r.cardNumber) j["cardNumber"] = *r.cardNumber;
    if (r.status) j["status"] = *r.status;
    return j;
}

}  // namespace

void registerRunnersRoutes(httplib::Server& svr, meos::db::Database& db) {
    svr.Get("/api/v1/runners",
            [&db](const httplib::Request&, httplib::Response& res) {
                auto runners = db.getAllRunners();
                json arr = json::array();
                for (const auto& r : runners) arr.push_back(runnerToJson(r));
                res.set_content(arr.dump(), "application/json");
            });

    svr.Get(R"(/api/v1/runners/(\d+))",
            [&db](const httplib::Request& req, httplib::Response& res) {
                int id = std::stoi(req.matches[1]);
                auto runner = db.getRunnerById(id);
                if (!runner) {
                    res.status = 404;
                    res.set_content(makeError(404, "Not found").dump(),
                                    "application/json");
                } else {
                    res.set_content(runnerToJson(*runner).dump(),
                                    "application/json");
                }
            });
}

namespace {

json teamToJson(const meos::domain::Team& t) {
    json j;
    j["id"] = t.id;
    j["name"] = t.name;
    if (t.clubId) j["clubId"] = *t.clubId;
    if (t.classId) j["classId"] = *t.classId;
    j["members"] = t.members;
    return j;
}

}  // namespace

void registerTeamsRoutes(httplib::Server& svr, meos::db::Database& db) {
    svr.Get("/api/v1/teams",
            [&db](const httplib::Request&, httplib::Response& res) {
                auto teams = db.getAllTeams();
                json arr = json::array();
                for (const auto& t : teams) arr.push_back(teamToJson(t));
                res.set_content(arr.dump(), "application/json");
            });

    svr.Get(R"(/api/v1/teams/(\d+))",
            [&db](const httplib::Request& req, httplib::Response& res) {
                int id = std::stoi(req.matches[1]);
                auto team = db.getTeamById(id);
                if (!team) {
                    res.status = 404;
                    res.set_content(makeError(404, "Not found").dump(),
                                    "application/json");
                } else {
                    res.set_content(teamToJson(*team).dump(),
                                    "application/json");
                }
            });
}

}  // namespace meos::net
