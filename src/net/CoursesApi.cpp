// CoursesApi.cpp — REST endpoints for courses (US-005c)
#include "CoursesApi.h"
#include "../util/meos_util.h"
#include "JsonUtils.h"
#include <string>
#include <sstream>

namespace meos_net {

/// Parse semicolon/comma-separated control-ID string into a JSON array.
static json controlIdsToArray(const std::string& controlIdsStr) {
    json arr = json::array();
    const char* s = controlIdsStr.c_str();
    while (*s) {
        int cid = atoi(s);
        while (*s && *s != ';' && *s != ',' && *s != ' ') ++s;
        while (*s && (*s == ';' || *s == ',' || *s == ' ')) ++s;
        if (cid > 0) arr.push_back(cid);
    }
    return arr;
}

/// Convert JSON array of int IDs to semicolon-separated string for importControls.
static std::string jsonArrayToControlIdStr(const json& arr) {
    std::string result;
    for (const auto& elem : arr) {
        if (!result.empty()) result += ";";
        result += std::to_string(elem.get<int>());
    }
    return result;
}

static json courseToJson(const oCourse& c) {
    return json{
        {"id",              c.getId()},
        {"name",            toUTF8(c.getName())},
        {"length",          c.getLength()},
        {"controlIds",      controlIdsToArray(c.getControls())},
        {"startControlId",  c.getStartId()},
        {"finishControlId", c.getFinishId()}
    };
}

void registerCourseRoutes(ApiRouter& router, oEvent& oe, meos_db::CourseRepository& repo) {

    // GET /api/v1/courses[?name=filter]
    router.GET("/courses", [&oe](const Request& req) -> json {
        std::vector<pCourse> courses;
        oe.getCourses(courses);
        std::string nameFilter;
        if (req.has_param("name"))
            nameFilter = req.get_param_value("name");
        json arr = json::array();
        for (pCourse c : courses) {
            if (!c) continue;
            if (!nameFilter.empty() &&
                toUTF8(c->getName()).find(nameFilter) == std::string::npos)
                continue;
            arr.push_back(courseToJson(*c));
        }
        return okResponse(arr);
    });

    // GET /api/v1/courses/{id}
    router.GET(R"(/courses/(\d+))", [&oe](const Request& req) -> json {
        int id = std::stoi(std::string(req.matches[1]));
        pCourse c = oe.getCourse(id);
        if (!c) throw notFound("Course not found: " + std::to_string(id));
        return okResponse(courseToJson(*c));
    });

    // POST /api/v1/courses
    router.POST("/courses", [&oe, &repo](const Request& req) -> json {
        json body = parseBody(req.body);
        std::string name = requireString(body, "name");
        if (name.empty()) throw badRequest("name must not be empty");
        int length = optionalInt(body, "length", 0);
        int newId  = oe.getFreeCourseId();
        pCourse c  = oe.addCourse(fromUTF8(name), length, newId);
        if (!c) throw internalError("Failed to create course");
        if (body.contains("controlIds") && body["controlIds"].is_array()) {
            std::string ctrlStr = jsonArrayToControlIdStr(body["controlIds"]);
            c->importControls(ctrlStr, true, false);
        }
        repo.insert(*c);
        return okResponse(courseToJson(*c));
    });

    // PUT /api/v1/courses/{id}
    router.PUT(R"(/courses/(\d+))", [&oe, &repo](const Request& req) -> json {
        int id = std::stoi(std::string(req.matches[1]));
        pCourse c = oe.getCourse(id);
        if (!c) throw notFound("Course not found: " + std::to_string(id));
        json body = parseBody(req.body);
        if (body.contains("name")) {
            std::string name = requireString(body, "name");
            if (name.empty()) throw badRequest("name must not be empty");
            c->setName(fromUTF8(name));
        }
        if (body.contains("length"))
            c->setLength(optionalInt(body, "length", c->getLength()));
        if (body.contains("controlIds") && body["controlIds"].is_array()) {
            std::string ctrlStr = jsonArrayToControlIdStr(body["controlIds"]);
            c->importControls(ctrlStr, true, false);
        }
        repo.update(*c);
        return okResponse(courseToJson(*c));
    });

    // DELETE /api/v1/courses/{id}
    router.DEL(R"(/courses/(\d+))", [&oe, &repo](const Request& req) -> json {
        int id = std::stoi(std::string(req.matches[1]));
        pCourse c = oe.getCourse(id);
        if (!c) throw notFound("Course not found: " + std::to_string(id));
        repo.remove(id);
        oe.removeCourse(id);
        return okResponse(json{{"deleted", id}});
    });
}

} // namespace meos_net
