#include "api.h"
#include "api_utils.h"

#include <nlohmann/json.hpp>

#include <map>
#include <sstream>

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

namespace {

json courseToJson(const meos::domain::Course& c) {
    json j;
    j["id"] = c.id;
    j["name"] = c.name;
    if (c.length) j["length"] = *c.length;
    j["controls"] = c.controls;
    return j;
}

}  // namespace

void registerCoursesRoutes(httplib::Server& svr, meos::db::Database& db) {
    svr.Get("/api/v1/courses",
            [&db](const httplib::Request&, httplib::Response& res) {
                auto courses = db.getAllCourses();
                json arr = json::array();
                for (const auto& c : courses) arr.push_back(courseToJson(c));
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
                    res.set_content(courseToJson(*course).dump(),
                                    "application/json");
                }
            });

    svr.Post("/api/v1/courses",
             [&db](const httplib::Request& req, httplib::Response& res) {
                 try {
                     auto body = json::parse(req.body);
                     meos::domain::Course c;
                     c.id = 0;
                     c.name = requireString(body, "name");
                     if (body.contains("length") && body["length"].is_number_integer())
                         c.length = body["length"].get<int>();
                     if (body.contains("controls") && body["controls"].is_array())
                         c.controls = body["controls"].get<std::vector<int>>();
                     c.id = db.insertCourse(c);
                     res.status = 201;
                     res.set_content(courseToJson(c).dump(), "application/json");
                 } catch (const std::invalid_argument& e) {
                     res.status = 400;
                     res.set_content(makeError(400, e.what()).dump(), "application/json");
                 } catch (...) {
                     res.status = 500;
                     res.set_content(makeError(500, "Internal error").dump(), "application/json");
                 }
             });

    svr.Put(R"(/api/v1/courses/(\d+))",
            [&db](const httplib::Request& req, httplib::Response& res) {
                try {
                    int id = std::stoi(req.matches[1]);
                    if (!db.getCourseById(id)) {
                        res.status = 404;
                        res.set_content(makeError(404, "Not found").dump(), "application/json");
                        return;
                    }
                    auto body = json::parse(req.body);
                    meos::domain::Course c;
                    c.id = id;
                    c.name = requireString(body, "name");
                    if (body.contains("length") && body["length"].is_number_integer())
                        c.length = body["length"].get<int>();
                    if (body.contains("controls") && body["controls"].is_array())
                        c.controls = body["controls"].get<std::vector<int>>();
                    db.updateCourse(c);
                    res.set_content(courseToJson(c).dump(), "application/json");
                } catch (const std::invalid_argument& e) {
                    res.status = 400;
                    res.set_content(makeError(400, e.what()).dump(), "application/json");
                } catch (...) {
                    res.status = 500;
                    res.set_content(makeError(500, "Internal error").dump(), "application/json");
                }
            });

    svr.Delete(R"(/api/v1/courses/(\d+))",
               [&db](const httplib::Request& req, httplib::Response& res) {
                   int id = std::stoi(req.matches[1]);
                   if (!db.getCourseById(id)) {
                       res.status = 404;
                       res.set_content(makeError(404, "Not found").dump(), "application/json");
                       return;
                   }
                   db.deleteCourse(id);
                   res.status = 204;
               });
}

namespace {

json classToJson(const meos::domain::Class& c) {
    json j;
    j["id"] = c.id;
    j["name"] = c.name;
    if (c.courseId) j["courseId"] = *c.courseId;
    if (c.startMethod) j["startMethod"] = *c.startMethod;
    return j;
}

// Parse courseId from JSON: absent/null = nullopt, 0 = nullopt (clear), >0 = set.
std::optional<int> parseCourseId(const json& body) {
    if (!body.contains("courseId")) return std::nullopt;
    if (body["courseId"].is_null()) return std::nullopt;
    int v = body["courseId"].get<int>();
    if (v == 0) return std::nullopt;  // 0 clears the course assignment
    return v;
}

}  // namespace

void registerClassesRoutes(httplib::Server& svr, meos::db::Database& db) {
    svr.Get("/api/v1/classes",
            [&db](const httplib::Request&, httplib::Response& res) {
                auto classes = db.getAllClasses();
                json arr = json::array();
                for (const auto& c : classes) arr.push_back(classToJson(c));
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
                    res.set_content(classToJson(*cls).dump(), "application/json");
                }
            });

    svr.Post("/api/v1/classes",
             [&db](const httplib::Request& req, httplib::Response& res) {
                 try {
                     auto body = json::parse(req.body);
                     meos::domain::Class c;
                     c.id = 0;
                     c.name = requireString(body, "name");
                     c.courseId = parseCourseId(body);
                     if (body.contains("startMethod") && body["startMethod"].is_string())
                         c.startMethod = body["startMethod"].get<std::string>();
                     c.id = db.insertClass(c);
                     res.status = 201;
                     res.set_content(classToJson(c).dump(), "application/json");
                 } catch (const std::invalid_argument& e) {
                     res.status = 400;
                     res.set_content(makeError(400, e.what()).dump(), "application/json");
                 } catch (...) {
                     res.status = 500;
                     res.set_content(makeError(500, "Internal error").dump(), "application/json");
                 }
             });

    svr.Put(R"(/api/v1/classes/(\d+))",
            [&db](const httplib::Request& req, httplib::Response& res) {
                try {
                    int id = std::stoi(req.matches[1]);
                    if (!db.getClassById(id)) {
                        res.status = 404;
                        res.set_content(makeError(404, "Not found").dump(), "application/json");
                        return;
                    }
                    auto body = json::parse(req.body);
                    meos::domain::Class c;
                    c.id = id;
                    c.name = requireString(body, "name");
                    c.courseId = parseCourseId(body);
                    if (body.contains("startMethod") && body["startMethod"].is_string())
                        c.startMethod = body["startMethod"].get<std::string>();
                    db.updateClass(c);
                    res.set_content(classToJson(c).dump(), "application/json");
                } catch (const std::invalid_argument& e) {
                    res.status = 400;
                    res.set_content(makeError(400, e.what()).dump(), "application/json");
                } catch (...) {
                    res.status = 500;
                    res.set_content(makeError(500, "Internal error").dump(), "application/json");
                }
            });

    svr.Delete(R"(/api/v1/classes/(\d+))",
               [&db](const httplib::Request& req, httplib::Response& res) {
                   int id = std::stoi(req.matches[1]);
                   if (!db.getClassById(id)) {
                       res.status = 404;
                       res.set_content(makeError(404, "Not found").dump(), "application/json");
                       return;
                   }
                   db.deleteClass(id);
                   res.status = 204;
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
    // GET /api/v1/runners  — supports ?name=, ?clubId=, ?classId=, ?page=, ?pageSize=
    svr.Get("/api/v1/runners",
            [&db](const httplib::Request& req, httplib::Response& res) {
                auto runners = db.getAllRunners();

                // Filtering
                std::string nameFilter   = req.get_param_value("name");
                std::string clubFilter   = req.get_param_value("clubId");
                std::string classFilter  = req.get_param_value("classId");

                std::vector<meos::domain::Runner> filtered;
                for (const auto& r : runners) {
                    if (!nameFilter.empty() &&
                        r.name.find(nameFilter) == std::string::npos) continue;
                    if (!clubFilter.empty()) {
                        int cid = std::stoi(clubFilter);
                        if (!r.clubId || *r.clubId != cid) continue;
                    }
                    if (!classFilter.empty()) {
                        int cid = std::stoi(classFilter);
                        if (!r.classId || *r.classId != cid) continue;
                    }
                    filtered.push_back(r);
                }

                int total    = static_cast<int>(filtered.size());
                int pageSize = 20;
                int page     = 1;
                if (!req.get_param_value("pageSize").empty())
                    pageSize = std::max(1, std::stoi(req.get_param_value("pageSize")));
                if (!req.get_param_value("page").empty())
                    page = std::max(1, std::stoi(req.get_param_value("page")));

                int start = (page - 1) * pageSize;
                int end   = std::min(start + pageSize, total);

                json data = json::array();
                for (int i = start; i < end; ++i)
                    data.push_back(runnerToJson(filtered[i]));

                json j;
                j["data"]     = data;
                j["total"]    = total;
                j["page"]     = page;
                j["pageSize"] = pageSize;
                res.set_content(j.dump(), "application/json");
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

    svr.Post("/api/v1/runners",
             [&db](const httplib::Request& req, httplib::Response& res) {
                 try {
                     auto body = json::parse(req.body);
                     meos::domain::Runner r;
                     r.id = 0;
                     r.name = requireString(body, "name");
                     if (body.contains("clubId") && body["clubId"].is_number_integer())
                         r.clubId = body["clubId"].get<int>();
                     if (body.contains("classId") && body["classId"].is_number_integer())
                         r.classId = body["classId"].get<int>();
                     if (body.contains("startTime") && body["startTime"].is_string())
                         r.startTime = body["startTime"].get<std::string>();
                     if (body.contains("cardNumber") && body["cardNumber"].is_number_integer())
                         r.cardNumber = body["cardNumber"].get<int>();
                     if (body.contains("status") && body["status"].is_string())
                         r.status = body["status"].get<std::string>();
                     r.id = db.insertRunner(r);
                     res.status = 201;
                     res.set_content(runnerToJson(r).dump(), "application/json");
                 } catch (const std::invalid_argument& e) {
                     res.status = 400;
                     res.set_content(makeError(400, e.what()).dump(), "application/json");
                 } catch (...) {
                     res.status = 500;
                     res.set_content(makeError(500, "Internal error").dump(), "application/json");
                 }
             });

    svr.Put(R"(/api/v1/runners/(\d+))",
            [&db](const httplib::Request& req, httplib::Response& res) {
                try {
                    int id = std::stoi(req.matches[1]);
                    if (!db.getRunnerById(id)) {
                        res.status = 404;
                        res.set_content(makeError(404, "Not found").dump(), "application/json");
                        return;
                    }
                    auto body = json::parse(req.body);
                    meos::domain::Runner r;
                    r.id = id;
                    r.name = requireString(body, "name");
                    if (body.contains("clubId") && body["clubId"].is_number_integer())
                        r.clubId = body["clubId"].get<int>();
                    if (body.contains("classId") && body["classId"].is_number_integer())
                        r.classId = body["classId"].get<int>();
                    if (body.contains("startTime") && body["startTime"].is_string())
                        r.startTime = body["startTime"].get<std::string>();
                    if (body.contains("cardNumber") && body["cardNumber"].is_number_integer())
                        r.cardNumber = body["cardNumber"].get<int>();
                    if (body.contains("status") && body["status"].is_string())
                        r.status = body["status"].get<std::string>();
                    db.updateRunner(r);
                    res.set_content(runnerToJson(r).dump(), "application/json");
                } catch (const std::invalid_argument& e) {
                    res.status = 400;
                    res.set_content(makeError(400, e.what()).dump(), "application/json");
                } catch (...) {
                    res.status = 500;
                    res.set_content(makeError(500, "Internal error").dump(), "application/json");
                }
            });

    svr.Delete(R"(/api/v1/runners/(\d+))",
               [&db](const httplib::Request& req, httplib::Response& res) {
                   int id = std::stoi(req.matches[1]);
                   if (!db.getRunnerById(id)) {
                       res.status = 404;
                       res.set_content(makeError(404, "Not found").dump(), "application/json");
                       return;
                   }
                   db.deleteRunner(id);
                   res.status = 204;
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
    // GET /api/v1/teams — supports ?name=, ?clubId=, ?classId=, ?page=, ?pageSize=
    svr.Get("/api/v1/teams",
            [&db](const httplib::Request& req, httplib::Response& res) {
                auto teams = db.getAllTeams();

                std::string nameFilter  = req.get_param_value("name");
                std::string clubFilter  = req.get_param_value("clubId");
                std::string classFilter = req.get_param_value("classId");

                std::vector<meos::domain::Team> filtered;
                for (const auto& t : teams) {
                    if (!nameFilter.empty() &&
                        t.name.find(nameFilter) == std::string::npos) continue;
                    if (!clubFilter.empty()) {
                        int cid = std::stoi(clubFilter);
                        if (!t.clubId || *t.clubId != cid) continue;
                    }
                    if (!classFilter.empty()) {
                        int cid = std::stoi(classFilter);
                        if (!t.classId || *t.classId != cid) continue;
                    }
                    filtered.push_back(t);
                }

                int total    = static_cast<int>(filtered.size());
                int pageSize = 20;
                int page     = 1;
                if (!req.get_param_value("pageSize").empty())
                    pageSize = std::max(1, std::stoi(req.get_param_value("pageSize")));
                if (!req.get_param_value("page").empty())
                    page = std::max(1, std::stoi(req.get_param_value("page")));

                int start = (page - 1) * pageSize;
                int end   = std::min(start + pageSize, total);

                json data = json::array();
                for (int i = start; i < end; ++i)
                    data.push_back(teamToJson(filtered[i]));

                json j;
                j["data"]     = data;
                j["total"]    = total;
                j["page"]     = page;
                j["pageSize"] = pageSize;
                res.set_content(j.dump(), "application/json");
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

    svr.Post("/api/v1/teams",
             [&db](const httplib::Request& req, httplib::Response& res) {
                 try {
                     auto body = json::parse(req.body);
                     meos::domain::Team t;
                     t.id = 0;
                     t.name = requireString(body, "name");
                     if (body.contains("clubId") && body["clubId"].is_number_integer())
                         t.clubId = body["clubId"].get<int>();
                     if (body.contains("classId") && body["classId"].is_number_integer())
                         t.classId = body["classId"].get<int>();
                     if (body.contains("members") && body["members"].is_array())
                         t.members = body["members"].get<std::vector<int>>();
                     t.id = db.insertTeam(t);
                     res.status = 201;
                     res.set_content(teamToJson(t).dump(), "application/json");
                 } catch (const std::invalid_argument& e) {
                     res.status = 400;
                     res.set_content(makeError(400, e.what()).dump(), "application/json");
                 } catch (...) {
                     res.status = 500;
                     res.set_content(makeError(500, "Internal error").dump(), "application/json");
                 }
             });

    svr.Put(R"(/api/v1/teams/(\d+))",
            [&db](const httplib::Request& req, httplib::Response& res) {
                try {
                    int id = std::stoi(req.matches[1]);
                    if (!db.getTeamById(id)) {
                        res.status = 404;
                        res.set_content(makeError(404, "Not found").dump(), "application/json");
                        return;
                    }
                    auto body = json::parse(req.body);
                    meos::domain::Team t;
                    t.id = id;
                    t.name = requireString(body, "name");
                    if (body.contains("clubId") && body["clubId"].is_number_integer())
                        t.clubId = body["clubId"].get<int>();
                    if (body.contains("classId") && body["classId"].is_number_integer())
                        t.classId = body["classId"].get<int>();
                    if (body.contains("members") && body["members"].is_array())
                        t.members = body["members"].get<std::vector<int>>();
                    db.updateTeam(t);
                    res.set_content(teamToJson(t).dump(), "application/json");
                } catch (const std::invalid_argument& e) {
                    res.status = 400;
                    res.set_content(makeError(400, e.what()).dump(), "application/json");
                } catch (...) {
                    res.status = 500;
                    res.set_content(makeError(500, "Internal error").dump(), "application/json");
                }
            });

    svr.Delete(R"(/api/v1/teams/(\d+))",
               [&db](const httplib::Request& req, httplib::Response& res) {
                   int id = std::stoi(req.matches[1]);
                   if (!db.getTeamById(id)) {
                       res.status = 404;
                       res.set_content(makeError(404, "Not found").dump(), "application/json");
                       return;
                   }
                   db.deleteTeam(id);
                   res.status = 204;
               });
}

void registerCompetitionsRoutes(httplib::Server& svr, meos::db::Database& db) {
    svr.Get("/api/v1/competitions",
            [&db](const httplib::Request&, httplib::Response& res) {
                auto comps = db.getAllCompetitions();
                if (comps.empty()) {
                    res.status = 404;
                    res.set_content(makeError(404, "Not found").dump(),
                                    "application/json");
                    return;
                }
                const auto& c = comps[0];
                json j;
                j["id"] = c.id;
                j["name"] = c.name;
                j["date"] = c.date;
                j["organizer"] = c.organizer;
                j["location"] = c.location;
                if (c.description) j["description"] = *c.description;
                res.set_content(j.dump(), "application/json");
            });

    svr.Put("/api/v1/competitions",
            [&db](const httplib::Request& req, httplib::Response& res) {
                try {
                    auto body = json::parse(req.body);
                    meos::domain::Competition c;
                    c.id = 1;
                    c.name = requireString(body, "name");
                    c.date = requireString(body, "date");
                    c.organizer = requireString(body, "organizer");
                    c.location = requireString(body, "location");
                    if (body.contains("description") && body["description"].is_string())
                        c.description = body["description"].get<std::string>();
                    db.upsertCompetition(c);
                    json j;
                    j["id"] = c.id;
                    j["name"] = c.name;
                    j["date"] = c.date;
                    j["organizer"] = c.organizer;
                    j["location"] = c.location;
                    if (c.description) j["description"] = *c.description;
                    res.set_content(j.dump(), "application/json");
                } catch (const std::invalid_argument& e) {
                    res.status = 400;
                    res.set_content(makeError(400, e.what()).dump(), "application/json");
                } catch (...) {
                    res.status = 500;
                    res.set_content(makeError(500, "Internal error").dump(), "application/json");
                }
            });
}

namespace {

json resultToJson(const meos::domain::Result& r) {
    json j;
    j["id"] = r.id;
    j["runnerId"] = r.runnerId;
    j["classId"] = r.classId;
    if (r.position) j["position"] = *r.position;
    if (r.totalTime) j["totalTime"] = *r.totalTime;
    j["status"] = r.status;
    json splits = json::array();
    for (const auto& s : r.splits) {
        splits.push_back({{"controlId", s.controlId}, {"time", s.time}});
    }
    j["splits"] = splits;
    return j;
}

}  // namespace

void registerResultsRoutes(httplib::Server& svr, meos::db::Database& db) {
    svr.Get("/api/v1/results",
            [&db](const httplib::Request&, httplib::Response& res) {
                auto results = db.getAllResults();
                json arr = json::array();
                for (const auto& r : results) arr.push_back(resultToJson(r));
                res.set_content(arr.dump(), "application/json");
            });

    svr.Get(R"(/api/v1/results/(\d+))",
            [&db](const httplib::Request& req, httplib::Response& res) {
                int id = std::stoi(req.matches[1]);
                auto result = db.getResultById(id);
                if (!result) {
                    res.status = 404;
                    res.set_content(makeError(404, "Not found").dump(),
                                    "application/json");
                } else {
                    res.set_content(resultToJson(*result).dump(),
                                    "application/json");
                }
            });
}

namespace {

json startListEntryToJson(const meos::domain::StartListEntry& e) {
    json j;
    j["id"] = e.id;
    j["runnerId"] = e.runnerId;
    j["classId"] = e.classId;
    j["startTime"] = e.startTime;
    if (e.bib) j["bib"] = *e.bib;
    return j;
}

}  // namespace

void registerStartListRoutes(httplib::Server& svr, meos::db::Database& db) {
    svr.Get("/api/v1/startlist",
            [&db](const httplib::Request&, httplib::Response& res) {
                auto entries = db.getAllStartList();
                json arr = json::array();
                for (const auto& e : entries) arr.push_back(startListEntryToJson(e));
                res.set_content(arr.dump(), "application/json");
            });

    svr.Get(R"(/api/v1/startlist/(\d+))",
            [&db](const httplib::Request& req, httplib::Response& res) {
                int id = std::stoi(req.matches[1]);
                auto entry = db.getStartListEntryById(id);
                if (!entry) {
                    res.status = 404;
                    res.set_content(makeError(404, "Not found").dump(),
                                    "application/json");
                } else {
                    res.set_content(startListEntryToJson(*entry).dump(),
                                    "application/json");
                }
            });
}

namespace {

static const char* IOF_NS =
    "http://www.orienteering.org/datastandard/3.0";

std::string xmlEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default:   out += c;        break;
        }
    }
    return out;
}

std::string toIofStatus(const std::string& s) {
    if (s == "ok")  return "OK";
    if (s == "dns") return "DidNotStart";
    if (s == "dnf") return "DidNotFinish";
    if (s == "mp")  return "MissingPunch";
    if (s == "dq")  return "Disqualified";
    return "OK";
}

std::string buildResultListXml(meos::db::Database& db) {
    auto results  = db.getAllResults();
    auto runners  = db.getAllRunners();
    auto classes  = db.getAllClasses();
    auto clubs    = db.getAllClubs();
    auto controls = db.getAllControls();

    std::map<int, meos::domain::Runner>  runnerMap;
    std::map<int, meos::domain::Class>   classMap;
    std::map<int, meos::domain::Club>    clubMap;
    std::map<int, int>                   controlCodeMap;

    for (auto& r : runners)  runnerMap[r.id]          = r;
    for (auto& c : classes)  classMap[c.id]            = c;
    for (auto& c : clubs)    clubMap[c.id]             = c;
    for (auto& c : controls) controlCodeMap[c.id]      = c.code;

    // Group results by classId
    std::map<int, std::vector<const meos::domain::Result*>> byClass;
    for (const auto& r : results) byClass[r.classId].push_back(&r);

    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<ResultList xmlns=\"" << IOF_NS << "\"\n"
        << "            xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
        << "            iofVersion=\"3.0\" status=\"Complete\">\n";

    for (const auto& [classId, resList] : byClass) {
        xml << "  <ClassResult>\n"
            << "    <Class>\n"
            << "      <Id>" << classId << "</Id>\n";
        if (classMap.count(classId))
            xml << "      <Name>" << xmlEscape(classMap.at(classId).name) << "</Name>\n";
        xml << "    </Class>\n";

        for (const auto* r : resList) {
            xml << "    <PersonResult>\n"
                << "      <Person>\n"
                << "        <Id>" << r->runnerId << "</Id>\n";
            if (runnerMap.count(r->runnerId)) {
                const auto& runner = runnerMap.at(r->runnerId);
                xml << "        <Name><Given>" << xmlEscape(runner.name) << "</Given></Name>\n";
            }
            xml << "      </Person>\n";

            if (runnerMap.count(r->runnerId)) {
                const auto& runner = runnerMap.at(r->runnerId);
                if (runner.clubId && clubMap.count(*runner.clubId)) {
                    const auto& club = clubMap.at(*runner.clubId);
                    xml << "      <Organisation>\n"
                        << "        <Id>" << club.id << "</Id>\n"
                        << "        <Name>" << xmlEscape(club.name) << "</Name>\n"
                        << "      </Organisation>\n";
                }
            }

            xml << "      <Result>\n";
            if (r->totalTime) xml << "        <Time>" << *r->totalTime << "</Time>\n";
            if (r->position)  xml << "        <Position>" << *r->position << "</Position>\n";
            xml << "        <Status>" << toIofStatus(r->status) << "</Status>\n";

            for (const auto& s : r->splits) {
                int code = controlCodeMap.count(s.controlId) ? controlCodeMap.at(s.controlId) : s.controlId;
                xml << "        <SplitTime>\n"
                    << "          <ControlCode>" << code << "</ControlCode>\n"
                    << "          <Time>" << s.time << "</Time>\n"
                    << "        </SplitTime>\n";
            }
            xml << "      </Result>\n"
                << "    </PersonResult>\n";
        }
        xml << "  </ClassResult>\n";
    }
    xml << "</ResultList>\n";
    return xml.str();
}

std::string buildStartListXml(meos::db::Database& db) {
    auto entries = db.getAllStartList();
    auto runners = db.getAllRunners();
    auto classes = db.getAllClasses();
    auto clubs   = db.getAllClubs();

    std::map<int, meos::domain::Runner> runnerMap;
    std::map<int, meos::domain::Class>  classMap;
    std::map<int, meos::domain::Club>   clubMap;

    for (auto& r : runners) runnerMap[r.id]  = r;
    for (auto& c : classes) classMap[c.id]   = c;
    for (auto& c : clubs)   clubMap[c.id]    = c;

    // Group entries by classId
    std::map<int, std::vector<const meos::domain::StartListEntry*>> byClass;
    for (const auto& e : entries) byClass[e.classId].push_back(&e);

    std::ostringstream xml;
    xml << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<StartList xmlns=\"" << IOF_NS << "\"\n"
        << "           xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
        << "           iofVersion=\"3.0\">\n";

    for (const auto& [classId, entryList] : byClass) {
        xml << "  <ClassStart>\n"
            << "    <Class>\n"
            << "      <Id>" << classId << "</Id>\n";
        if (classMap.count(classId))
            xml << "      <Name>" << xmlEscape(classMap.at(classId).name) << "</Name>\n";
        xml << "    </Class>\n";

        for (const auto* e : entryList) {
            xml << "    <PersonStart>\n"
                << "      <Person>\n"
                << "        <Id>" << e->runnerId << "</Id>\n";
            if (runnerMap.count(e->runnerId)) {
                const auto& runner = runnerMap.at(e->runnerId);
                xml << "        <Name><Given>" << xmlEscape(runner.name) << "</Given></Name>\n";
            }
            xml << "      </Person>\n";

            if (runnerMap.count(e->runnerId)) {
                const auto& runner = runnerMap.at(e->runnerId);
                if (runner.clubId && clubMap.count(*runner.clubId)) {
                    const auto& club = clubMap.at(*runner.clubId);
                    xml << "      <Organisation>\n"
                        << "        <Id>" << club.id << "</Id>\n"
                        << "        <Name>" << xmlEscape(club.name) << "</Name>\n"
                        << "      </Organisation>\n";
                }
            }

            xml << "      <Start>\n"
                << "        <StartTime>" << xmlEscape(e->startTime) << "</StartTime>\n";
            if (e->bib) xml << "        <BibNumber>" << *e->bib << "</BibNumber>\n";
            if (runnerMap.count(e->runnerId)) {
                const auto& runner = runnerMap.at(e->runnerId);
                if (runner.cardNumber)
                    xml << "        <ControlCard type=\"SI\">" << *runner.cardNumber << "</ControlCard>\n";
            }
            xml << "      </Start>\n"
                << "    </PersonStart>\n";
        }
        xml << "  </ClassStart>\n";
    }
    xml << "</StartList>\n";
    return xml.str();
}

}  // anonymous namespace

void registerXmlExportRoutes(httplib::Server& svr, meos::db::Database& db) {
    svr.Get("/api/v1/results/export/xml",
            [&db](const httplib::Request&, httplib::Response& res) {
                res.set_content(buildResultListXml(db), "application/xml");
            });

    svr.Get("/api/v1/startlist/export/xml",
            [&db](const httplib::Request&, httplib::Response& res) {
                res.set_content(buildStartListXml(db), "application/xml");
            });
}

}  // namespace meos::net
