// RunnersApi.cpp — REST endpoints for runners (US-005d)
#include "RunnersApi.h"
#include "../util/meos_util.h"
#include "../domain/oAbstractRunner.h"
#include "../domain/oBase.h"
#include "JsonUtils.h"
#include <string>
#include <algorithm>

using ChangeType = oBase::ChangeType;

namespace meos_net {

static json runnerToJson(const oRunner& r) {
    return json{
        {"id",        r.getId()},
        {"name",      toUTF8(r.getName())},
        {"clubId",    r.getClubId()},
        {"classId",   r.getClassId(false)},
        {"cardNo",    r.getCardNo()},
        {"startNo",   r.getStartNo()},
        {"startTime", r.getStartTime()},
        {"status",    static_cast<int>(r.getStatus())}
    };
}

void registerRunnerRoutes(ApiRouter& router, oEvent& oe, meos_db::RunnerRepository& repo) {

    // GET /api/v1/runners[?name=&clubId=N&classId=N&page=N&pageSize=N]
    router.GET("/runners", [&oe](const Request& req) -> json {
        std::vector<pRunner> runners;
        oe.getRunners(0, 0, runners, false);

        std::string nameFilter;
        int clubIdFilter  = 0;
        int classIdFilter = 0;
        if (req.has_param("name"))    nameFilter    = req.get_param_value("name");
        if (req.has_param("clubId"))  clubIdFilter  = std::stoi(req.get_param_value("clubId"));
        if (req.has_param("classId")) classIdFilter = std::stoi(req.get_param_value("classId"));

        std::vector<pRunner> filtered;
        for (pRunner r : runners) {
            if (!r) continue;
            if (!nameFilter.empty() &&
                toUTF8(r->getName()).find(nameFilter) == std::string::npos)
                continue;
            if (clubIdFilter  > 0 && r->getClubId()      != clubIdFilter)  continue;
            if (classIdFilter > 0 && r->getClassId(false) != classIdFilter) continue;
            filtered.push_back(r);
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
            arr.push_back(runnerToJson(*filtered[i]));

        return okResponse(json{{"data", arr}, {"total", total}, {"page", page}, {"pageSize", pageSize}});
    });

    // GET /api/v1/runners/{id}
    router.GET(R"(/runners/(\d+))", [&oe](const Request& req) -> json {
        int id = std::stoi(std::string(req.matches[1]));
        pRunner r = oe.getRunner(id, 0);
        if (!r) throw notFound("Runner not found: " + std::to_string(id));
        return okResponse(runnerToJson(*r));
    });

    // POST /api/v1/runners
    router.POST("/runners", [&oe, &repo](const Request& req) -> json {
        json body = parseBody(req.body);
        std::string name = requireString(body, "name");
        if (name.empty()) throw badRequest("name must not be empty");

        oRunner r(&oe);
        r.setName(fromUTF8(name), false);
        pRunner pr = oe.addRunner(r);
        if (!pr) throw internalError("Failed to create runner");

        if (body.contains("clubId")  && body["clubId"].is_number())
            pr->setClubId(body["clubId"].get<int>());
        if (body.contains("classId") && body["classId"].is_number())
            pr->setClassId(body["classId"].get<int>(), false);
        if (body.contains("cardNo")  && body["cardNo"].is_number())
            pr->setCardNo(body["cardNo"].get<int>(), false, false);
        if (body.contains("startNo") && body["startNo"].is_number())
            pr->setStartNo(body["startNo"].get<int>(), ChangeType::Quiet);

        repo.insert(*pr);
        return okResponse(runnerToJson(*pr));
    });

    // PUT /api/v1/runners/{id}
    router.PUT(R"(/runners/(\d+))", [&oe, &repo](const Request& req) -> json {
        int id = std::stoi(std::string(req.matches[1]));
        pRunner r = oe.getRunner(id, 0);
        if (!r) throw notFound("Runner not found: " + std::to_string(id));

        json body = parseBody(req.body);
        if (body.contains("name") && body["name"].is_string()) {
            std::string n = body["name"].get<std::string>();
            if (n.empty()) throw badRequest("name must not be empty");
            r->setName(fromUTF8(n), false);
        }
        if (body.contains("clubId")    && body["clubId"].is_number())
            r->setClubId(body["clubId"].get<int>());
        if (body.contains("classId")   && body["classId"].is_number())
            r->setClassId(body["classId"].get<int>(), false);
        if (body.contains("cardNo")    && body["cardNo"].is_number())
            r->setCardNo(body["cardNo"].get<int>(), false, false);
        if (body.contains("startNo")   && body["startNo"].is_number())
            r->setStartNo(body["startNo"].get<int>(), ChangeType::Quiet);
        if (body.contains("startTime") && body["startTime"].is_number())
            r->setStartTime(body["startTime"].get<int>(), true, ChangeType::Update, false);
        if (body.contains("status")    && body["status"].is_number())
            r->setStatus(RunnerStatus(body["status"].get<int>()), true, ChangeType::Update, false);

        repo.update(*r);
        return okResponse(runnerToJson(*r));
    });

    // DELETE /api/v1/runners/{id}
    router.DEL(R"(/runners/(\d+))", [&oe, &repo](const Request& req) -> json {
        int id = std::stoi(std::string(req.matches[1]));
        pRunner r = oe.getRunner(id, 0);
        if (!r) throw notFound("Runner not found: " + std::to_string(id));
        repo.remove(id);
        oe.removeRunner(id);
        return okResponse(json{{"deleted", id}});
    });
}

} // namespace meos_net
