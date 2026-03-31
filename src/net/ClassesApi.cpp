// ClassesApi.cpp — REST endpoints for classes (US-005c)
#include "ClassesApi.h"
#include "../util/meos_util.h"
#include "JsonUtils.h"
#include <string>

namespace meos_net {

static json classToJson(const oClass& c) {
    return json{
        {"id",       c.getId()},
        {"name",     toUTF8(c.getName())},
        {"courseId", c.getCourseId()}
    };
}

void registerClassRoutes(ApiRouter& router, oEvent& oe, meos_db::ClassRepository& repo) {

    // GET /api/v1/classes[?name=filter]
    router.GET("/classes", [&oe](const Request& req) -> json {
        std::vector<pClass> classes;
        oe.getClasses(classes, false);
        std::string nameFilter;
        if (req.has_param("name"))
            nameFilter = req.get_param_value("name");
        json arr = json::array();
        for (pClass c : classes) {
            if (!c) continue;
            if (!nameFilter.empty() &&
                toUTF8(c->getName()).find(nameFilter) == std::string::npos)
                continue;
            arr.push_back(classToJson(*c));
        }
        return okResponse(arr);
    });

    // GET /api/v1/classes/{id}
    router.GET(R"(/classes/(\d+))", [&oe](const Request& req) -> json {
        int id = std::stoi(std::string(req.matches[1]));
        pClass c = oe.getClass(id);
        if (!c) throw notFound("Class not found: " + std::to_string(id));
        return okResponse(classToJson(*c));
    });

    // POST /api/v1/classes
    router.POST("/classes", [&oe, &repo](const Request& req) -> json {
        json body = parseBody(req.body);
        std::string name = requireString(body, "name");
        if (name.empty()) throw badRequest("name must not be empty");
        int courseId = optionalInt(body, "courseId", 0);
        pClass c = oe.addClass(fromUTF8(name), courseId);
        if (!c) throw internalError("Failed to create class");
        repo.insert(*c);
        return okResponse(classToJson(*c));
    });

    // PUT /api/v1/classes/{id}
    router.PUT(R"(/classes/(\d+))", [&oe, &repo](const Request& req) -> json {
        int id = std::stoi(std::string(req.matches[1]));
        pClass c = oe.getClass(id);
        if (!c) throw notFound("Class not found: " + std::to_string(id));
        json body = parseBody(req.body);
        if (body.contains("name")) {
            std::string name = requireString(body, "name");
            if (name.empty()) throw badRequest("name must not be empty");
            c->setName(fromUTF8(name), false);
        }
        if (body.contains("courseId")) {
            int courseId = optionalInt(body, "courseId", 0);
            if (courseId == 0) {
                c->setCourse(nullptr);
            } else {
                pCourse pc = oe.getCourse(courseId);
                if (!pc) throw notFound("Course not found: " + std::to_string(courseId));
                c->setCourse(pc);
            }
        }
        repo.update(*c);
        return okResponse(classToJson(*c));
    });

    // DELETE /api/v1/classes/{id}
    router.DEL(R"(/classes/(\d+))", [&oe, &repo](const Request& req) -> json {
        int id = std::stoi(std::string(req.matches[1]));
        pClass c = oe.getClass(id);
        if (!c) throw notFound("Class not found: " + std::to_string(id));
        repo.remove(id);
        oe.removeClass(id);
        return okResponse(json{{"deleted", id}});
    });
}

} // namespace meos_net
