// ResultsApi.cpp — REST endpoints for results and start lists (US-006)
#include "ResultsApi.h"
#include "../util/meos_util.h"
#include "../domain/oAbstractRunner.h"
#include "JsonUtils.h"
#include <algorithm>
#include <string>
#include <vector>

namespace meos_net {

static json runnerResultToJson(const oRunner& r) {
    int place = r.getTempResult().getPlace();
    return json{
        {"id",          r.getId()},
        {"name",        toUTF8(r.getName())},
        {"classId",     r.getClassId(false)},
        {"className",   toUTF8(r.getClass(false))},
        {"clubId",      r.getClubId()},
        {"startTime",   r.getStartTime()},
        {"finishTime",  r.getFinishTime()},
        {"runningTime", r.getRunningTime(false)},
        {"status",      static_cast<int>(r.getStatus())},
        {"place",       place > 0 && place < 10000 ? place : 0}
    };
}

static json runnerStartToJson(const oRunner& r) {
    return json{
        {"id",        r.getId()},
        {"name",      toUTF8(r.getName())},
        {"classId",   r.getClassId(false)},
        {"className", toUTF8(r.getClass(false))},
        {"clubId",    r.getClubId()},
        {"startNo",   r.getStartNo()},
        {"startTime", r.getStartTime()},
        {"cardNo",    r.getCardNo()}
    };
}

void registerResultsRoutes(ApiRouter& router, oEvent& oe) {

    // GET /api/v1/results[?classId=N&type=preliminary|final]
    router.GET("/results", [&oe](const Request& req) -> json {
        int classIdFilter = 0;
        if (req.has_param("classId")) classIdFilter = std::stoi(req.get_param_value("classId"));

        oe.calculateResults(classIdFilter);

        std::vector<pRunner> runners;
        oe.getRunners(0, 0, runners, false);

        // Group by class for a structured response
        json byClass = json::array();
        std::vector<pClass> classes;
        oe.getClasses(classes, false);

        for (pClass pc : classes) {
            if (!pc) continue;
            int cid = pc->getId();
            if (classIdFilter > 0 && cid != classIdFilter) continue;

            std::vector<pRunner> clsRunners;
            for (pRunner r : runners) {
                if (r && !r->isRemoved() && r->getClassId(false) == cid)
                    clsRunners.push_back(r);
            }
            if (clsRunners.empty()) continue;

            // Sort by place (0 = unplaced, put at end), then running time
            std::stable_sort(clsRunners.begin(), clsRunners.end(), [](pRunner a, pRunner b) {
                int pa = a->getTempResult().getPlace();
                int pb = b->getTempResult().getPlace();
                if ((pa > 0 && pa < 10000) != (pb > 0 && pb < 10000))
                    return (pa > 0 && pa < 10000);
                if (pa != pb) return pa < pb;
                return a->getRunningTime(false) < b->getRunningTime(false);
            });

            json arr = json::array();
            for (pRunner r : clsRunners)
                arr.push_back(runnerResultToJson(*r));

            byClass.push_back(json{
                {"classId",   cid},
                {"className", toUTF8(pc->getName())},
                {"results",   arr}
            });
        }

        return okResponse(byClass);
    });

    // GET /api/v1/startlist[?classId=N]
    router.GET("/startlist", [&oe](const Request& req) -> json {
        int classIdFilter = 0;
        if (req.has_param("classId")) classIdFilter = std::stoi(req.get_param_value("classId"));

        std::vector<pRunner> runners;
        oe.getRunners(0, 0, runners, false);

        if (classIdFilter > 0) {
            std::vector<pRunner> filtered;
            for (pRunner r : runners)
                if (r && r->getClassId(false) == classIdFilter) filtered.push_back(r);
            runners = std::move(filtered);
        }

        // Sort by classId, then startTime
        std::stable_sort(runners.begin(), runners.end(), [](pRunner a, pRunner b) {
            int ca = a->getClassId(false);
            int cb = b->getClassId(false);
            if (ca != cb) return ca < cb;
            return a->getStartTime() < b->getStartTime();
        });

        json arr = json::array();
        for (pRunner r : runners)
            if (r && !r->isRemoved()) arr.push_back(runnerStartToJson(*r));

        return okResponse(arr);
    });
}

} // namespace meos_net
