// ControlsApi.cpp — REST endpoints for controls (US-005b)
#include "ControlsApi.h"
#include "../util/meos_util.h"
#include "JsonUtils.h"
#include <string>

namespace meos_net {

static json controlToJson(const oControl& c) {
    return json{
        {"id",     c.getId()},
        {"name",   toUTF8(c.getName())},
        {"number", c.getFirstNumber()},
        {"status", static_cast<int>(c.getStatus())}
    };
}

void registerControlRoutes(ApiRouter& router, oEvent& oe, meos_db::ControlRepository& repo) {

    // GET /api/v1/controls[?name=filter]
    router.GET("/controls", [&oe](const Request& req) -> json {
        std::vector<pControl> controls;
        oe.getControls(controls, false);
        std::string nameFilter;
        if (req.has_param("name"))
            nameFilter = req.get_param_value("name");
        json arr = json::array();
        for (pControl c : controls) {
            if (!c) continue;
            if (!nameFilter.empty() &&
                toUTF8(c->getName()).find(nameFilter) == std::string::npos)
                continue;
            arr.push_back(controlToJson(*c));
        }
        return okResponse(arr);
    });

    // GET /api/v1/controls/{id}
    router.GET(R"(/controls/(\d+))", [&oe](const Request& req) -> json {
        int id = std::stoi(std::string(req.matches[1]));
        pControl c = oe.getControl(id);
        if (!c) throw notFound("Control not found: " + std::to_string(id));
        return okResponse(controlToJson(*c));
    });

    // POST /api/v1/controls
    router.POST("/controls", [&oe, &repo](const Request& req) -> json {
        json body = parseBody(req.body);
        std::string name = requireString(body, "name");
        if (name.empty()) throw badRequest("name must not be empty");
        int number    = optionalInt(body, "number", 0);
        int statusInt = optionalInt(body, "status", 0);
        int newId = oe.getFreeControlId();
        pControl c = oe.getControl(newId, true, false);
        if (!c) throw internalError("Failed to create control");
        c->set(newId, number, fromUTF8(name));
        c->setStatus(static_cast<oControl::ControlStatus>(statusInt));
        repo.insert(*c);
        return okResponse(controlToJson(*c));
    });

    // PUT /api/v1/controls/{id}
    router.PUT(R"(/controls/(\d+))", [&oe, &repo](const Request& req) -> json {
        int id = std::stoi(std::string(req.matches[1]));
        pControl c = oe.getControl(id);
        if (!c) throw notFound("Control not found: " + std::to_string(id));
        json body = parseBody(req.body);
        std::string name = requireString(body, "name");
        if (name.empty()) throw badRequest("name must not be empty");
        int number    = optionalInt(body, "number", c->getFirstNumber());
        int statusInt = optionalInt(body, "status", static_cast<int>(c->getStatus()));
        c->set(id, number, fromUTF8(name));
        c->setStatus(static_cast<oControl::ControlStatus>(statusInt));
        repo.update(*c);
        return okResponse(controlToJson(*c));
    });

    // DELETE /api/v1/controls/{id}
    router.DEL(R"(/controls/(\d+))", [&oe, &repo](const Request& req) -> json {
        int id = std::stoi(std::string(req.matches[1]));
        pControl c = oe.getControl(id);
        if (!c) throw notFound("Control not found: " + std::to_string(id));
        repo.remove(id);
        oe.removeControl(id);
        return okResponse(json{{"deleted", id}});
    });
}

} // namespace meos_net
