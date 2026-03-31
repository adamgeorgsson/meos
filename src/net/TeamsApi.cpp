// TeamsApi.cpp — REST endpoints for teams (US-005d)
#include "TeamsApi.h"
#include "../util/meos_util.h"
#include "../domain/oAbstractRunner.h"
#include "../domain/oBase.h"
#include "JsonUtils.h"
#include <string>
#include <algorithm>

using ChangeType = oBase::ChangeType;

namespace meos_net {

static json runnerIdsToArray(const std::string& ids) {
    json arr = json::array();
    size_t pos = 0;
    while (pos < ids.size()) {
        size_t semi = ids.find(';', pos);
        if (semi == std::string::npos) break;
        std::string tok = ids.substr(pos, semi - pos);
        if (!tok.empty()) arr.push_back(std::stoi(tok));
        pos = semi + 1;
    }
    return arr;
}

static json teamToJson(const oTeam& t) {
    return json{
        {"id",        t.getId()},
        {"name",      toUTF8(t.getName())},
        {"clubId",    t.getClubId()},
        {"classId",   t.getClassId(false)},
        {"startNo",   t.getStartNo()},
        {"startTime", t.getStartTime()},
        {"status",    static_cast<int>(t.getStatus())},
        {"runnerIds", runnerIdsToArray(t.getRunnerIdString())}
    };
}

void registerTeamRoutes(ApiRouter& router, oEvent& oe, meos_db::TeamRepository& repo) {

    // GET /api/v1/teams[?name=&clubId=N&classId=N&page=N&pageSize=N]
    router.GET("/teams", [&oe](const Request& req) -> json {
        std::vector<pTeam> teams;
        oe.getTeams(0, teams, false);

        std::string nameFilter;
        int clubIdFilter  = 0;
        int classIdFilter = 0;
        if (req.has_param("name"))    nameFilter    = req.get_param_value("name");
        if (req.has_param("clubId"))  clubIdFilter  = std::stoi(req.get_param_value("clubId"));
        if (req.has_param("classId")) classIdFilter = std::stoi(req.get_param_value("classId"));

        std::vector<pTeam> filtered;
        for (pTeam t : teams) {
            if (!t) continue;
            if (!nameFilter.empty() &&
                toUTF8(t->getName()).find(nameFilter) == std::string::npos)
                continue;
            if (clubIdFilter  > 0 && t->getClubId()      != clubIdFilter)  continue;
            if (classIdFilter > 0 && t->getClassId(false) != classIdFilter) continue;
            filtered.push_back(t);
        }

        int total    = static_cast<int>(filtered.size());
        int page     = 1;
        int pageSize = 100;
        if (req.has_param("page"))     page     = std::max(1, std::stoi(req.get_param_value("page")));
        if (req.has_param("pageSize")) pageSize = std::max(1, std::stoi(req.get_param_value("pageSize")));

        int start = (page - 1) * pageSize;
        int end   = std::min(start + pageSize, total);

        json arr = json::array();
        for (int i = start; i < end; i++)
            arr.push_back(teamToJson(*filtered[i]));

        return okResponse(json{{"data", arr}, {"total", total}, {"page", page}, {"pageSize", pageSize}});
    });

    // GET /api/v1/teams/{id}
    router.GET(R"(/teams/(\d+))", [&oe](const Request& req) -> json {
        int id = std::stoi(std::string(req.matches[1]));
        pTeam t = oe.getTeam(id);
        if (!t) throw notFound("Team not found: " + std::to_string(id));
        return okResponse(teamToJson(*t));
    });

    // POST /api/v1/teams
    router.POST("/teams", [&oe, &repo](const Request& req) -> json {
        json body = parseBody(req.body);
        std::string name = requireString(body, "name");
        if (name.empty()) throw badRequest("name must not be empty");

        int clubId  = body.contains("clubId")  && body["clubId"].is_number()  ? body["clubId"].get<int>()  : 0;
        int classId = body.contains("classId") && body["classId"].is_number() ? body["classId"].get<int>() : 0;

        pTeam pt = oe.addTeam(fromUTF8(name), clubId, classId);
        if (!pt) throw internalError("Failed to create team");

        if (body.contains("startNo") && body["startNo"].is_number())
            pt->setStartNo(body["startNo"].get<int>(), ChangeType::Quiet);

        repo.insert(*pt);
        return okResponse(teamToJson(*pt));
    });

    // PUT /api/v1/teams/{id}
    router.PUT(R"(/teams/(\d+))", [&oe, &repo](const Request& req) -> json {
        int id = std::stoi(std::string(req.matches[1]));
        pTeam t = oe.getTeam(id);
        if (!t) throw notFound("Team not found: " + std::to_string(id));

        json body = parseBody(req.body);
        if (body.contains("name") && body["name"].is_string()) {
            std::string n = body["name"].get<std::string>();
            if (n.empty()) throw badRequest("name must not be empty");
            t->setName(fromUTF8(n), false);
        }
        if (body.contains("clubId")  && body["clubId"].is_number())
            t->setClubId(body["clubId"].get<int>());
        if (body.contains("classId") && body["classId"].is_number())
            t->setClassId(body["classId"].get<int>(), false);
        if (body.contains("startNo") && body["startNo"].is_number())
            t->setStartNo(body["startNo"].get<int>(), ChangeType::Quiet);
        if (body.contains("startTime") && body["startTime"].is_number())
            t->setStartTime(body["startTime"].get<int>(), true, ChangeType::Update, false);
        if (body.contains("status") && body["status"].is_number())
            t->setStatus(RunnerStatus(body["status"].get<int>()), true, ChangeType::Update, false);

        repo.update(*t);
        return okResponse(teamToJson(*t));
    });

    // DELETE /api/v1/teams/{id}
    router.DEL(R"(/teams/(\d+))", [&oe, &repo](const Request& req) -> json {
        int id = std::stoi(std::string(req.matches[1]));
        pTeam t = oe.getTeam(id);
        if (!t) throw notFound("Team not found: " + std::to_string(id));
        repo.remove(id);
        oe.removeTeam(id);
        return okResponse(json{{"deleted", id}});
    });
}

} // namespace meos_net
