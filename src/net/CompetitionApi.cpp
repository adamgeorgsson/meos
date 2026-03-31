// CompetitionApi.cpp — REST endpoints for competition metadata (US-005d)
#include "CompetitionApi.h"
#include "../util/meos_util.h"
#include "JsonUtils.h"
#include <string>

namespace meos_net {

static json competitionToJson(const oEvent& oe) {
    return json{
        {"name",     toUTF8(oe.getName())},
        {"date",     toUTF8(oe.getDate())},
        {"zeroTime", toUTF8(oe.getZeroTime())}
    };
}

void registerCompetitionRoutes(ApiRouter& router, oEvent& oe, meos_db::EventRepository& repo) {

    // GET /api/v1/competition
    router.GET("/competition", [&oe](const Request& /*req*/) -> json {
        return okResponse(competitionToJson(oe));
    });

    // PUT /api/v1/competition
    router.PUT("/competition", [&oe, &repo](const Request& req) -> json {
        json body = parseBody(req.body);

        if (body.contains("name") && body["name"].is_string()) {
            std::string n = body["name"].get<std::string>();
            if (n.empty()) throw badRequest("name must not be empty");
            oe.setName(fromUTF8(n));
        }
        if (body.contains("date") && body["date"].is_string())
            oe.setDate(fromUTF8(body["date"].get<std::string>()));
        if (body.contains("zeroTime") && body["zeroTime"].is_string())
            oe.setZeroTime(fromUTF8(body["zeroTime"].get<std::string>()));

        repo.save(oe);
        return okResponse(competitionToJson(oe));
    });
}

} // namespace meos_net
