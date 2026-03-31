// ClubsApi.cpp — REST endpoints for clubs (US-005b)
#include "ClubsApi.h"
#include "../util/meos_util.h"
#include "JsonUtils.h"
#include <string>

namespace meos_net {

static json clubToJson(const oClub& c) {
    return json{{"id", c.getId()}, {"name", toUTF8(c.getName())}};
}

void registerClubRoutes(ApiRouter& router, oEvent& oe, meos_db::ClubRepository& repo) {

    // GET /api/v1/clubs[?name=filter]
    router.GET("/clubs", [&oe](const Request& req) -> json {
        std::vector<pClub> clubs;
        oe.getClubs(clubs, true);
        std::string nameFilter;
        if (req.has_param("name"))
            nameFilter = req.get_param_value("name");
        json arr = json::array();
        for (pClub c : clubs) {
            if (!c) continue;
            if (!nameFilter.empty() &&
                toUTF8(c->getName()).find(nameFilter) == std::string::npos)
                continue;
            arr.push_back(clubToJson(*c));
        }
        return okResponse(arr);
    });

    // GET /api/v1/clubs/{id}
    router.GET(R"(/clubs/(\d+))", [&oe](const Request& req) -> json {
        int id = std::stoi(std::string(req.matches[1]));
        pClub c = oe.getClub(id);
        if (!c) throw notFound("Club not found: " + std::to_string(id));
        return okResponse(clubToJson(*c));
    });

    // POST /api/v1/clubs
    router.POST("/clubs", [&oe, &repo](const Request& req) -> json {
        json body = parseBody(req.body);
        std::string name = requireString(body, "name");
        if (name.empty()) throw badRequest("name must not be empty");
        pClub c = oe.addClub(fromUTF8(name));
        if (!c) throw internalError("Failed to create club");
        repo.insert(*c);
        return okResponse(clubToJson(*c));
    });

    // PUT /api/v1/clubs/{id}
    router.PUT(R"(/clubs/(\d+))", [&oe, &repo](const Request& req) -> json {
        int id = std::stoi(std::string(req.matches[1]));
        pClub c = oe.getClub(id);
        if (!c) throw notFound("Club not found: " + std::to_string(id));
        json body = parseBody(req.body);
        std::string name = requireString(body, "name");
        if (name.empty()) throw badRequest("name must not be empty");
        c->setName(fromUTF8(name));
        repo.update(*c);
        return okResponse(clubToJson(*c));
    });

    // DELETE /api/v1/clubs/{id}
    router.DEL(R"(/clubs/(\d+))", [&oe, &repo](const Request& req) -> json {
        int id = std::stoi(std::string(req.matches[1]));
        pClub c = oe.getClub(id);
        if (!c) throw notFound("Club not found: " + std::to_string(id));
        repo.remove(id);
        oe.removeClub(id);
        return okResponse(json{{"deleted", id}});
    });
}

} // namespace meos_net
