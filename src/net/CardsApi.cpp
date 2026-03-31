// CardsApi.cpp — REST endpoints for card readout and manual status (US-006)
#include "CardsApi.h"
#include "../util/meos_util.h"
#include "../domain/oAbstractRunner.h"
#include "../domain/oCard.h"
#include "../domain/oPunch.h"
#include "../domain/oBase.h"
#include "JsonUtils.h"
#include <string>
#include <vector>

using ChangeType = oBase::ChangeType;

namespace meos_net {

static RunnerStatus parseStatus(const std::string& s) {
    if (s == "DNS")  return StatusDNS;
    if (s == "DNF")  return StatusDNF;
    if (s == "DSQ" || s == "DQ") return StatusDQ;
    if (s == "OK")   return StatusOK;
    if (s == "MP")   return StatusMP;
    throw badRequest("Unknown status: " + s + ". Valid values: DNS, DNF, DSQ, OK, MP");
}

static std::string statusToString(RunnerStatus s) {
    switch (s) {
        case StatusOK:               return "OK";
        case StatusMP:               return "MP";
        case StatusDNF:              return "DNF";
        case StatusDQ:               return "DSQ";
        case StatusDNS:              return "DNS";
        case StatusUnknown:          return "Unknown";
        case StatusNoTiming:         return "NoTiming";
        case StatusMAX:              return "MAX";
        case StatusOutOfCompetition: return "OOC";
        case StatusCANCEL:           return "CANCEL";
        case StatusNotCompeting:     return "NotCompeting";
        default:                     return "Unknown";
    }
}

void registerCardsRoutes(ApiRouter& router, oEvent& oe) {

    // POST /api/v1/cards
    // Body: { "cardNo": N, "startTime": N, "finishTime": N,
    //         "punches": [{"type": N, "time": N}, ...] }
    // Finds the runner assigned this card number and applies the readout.
    router.POST("/cards", [&oe](const Request& req) -> json {
        json body = parseBody(req.body);

        int cardNo = optionalInt(body, "cardNo", 0);
        if (cardNo <= 0) throw badRequest("cardNo must be a positive integer");

        // Build the card
        oCard newCard(&oe);
        newCard.setCardNo(cardNo);

        int startTime  = optionalInt(body, "startTime",  0);
        int finishTime = optionalInt(body, "finishTime", 0);

        if (startTime > 0)
            newCard.addPunch(oPunch::PunchStart, startTime, 0, 0, oCard::PunchOrigin::Original);

        if (body.contains("punches") && body["punches"].is_array()) {
            for (auto& p : body["punches"]) {
                int type = optionalInt(p, "type", 0);
                int time = optionalInt(p, "time", 0);
                if (type > 0)
                    newCard.addPunch(type, time, 0, 0, oCard::PunchOrigin::Original);
            }
        }

        if (finishTime > 0)
            newCard.addPunch(oPunch::PunchFinish, finishTime, 0, 0, oCard::PunchOrigin::Original);

        pCard pc = oe.addCard(newCard);
        if (!pc) throw internalError("Failed to create card");

        // Find runner by card number
        pRunner runner = oe.getRunnerByCardNo(cardNo, 0, oEvent::CardLookupProperty::Any);

        json result = json{{"cardId", pc->getId()}, {"cardNo", cardNo}};

        if (runner) {
            std::vector<std::pair<int, pControl>> missingPunches;
            runner->addCard(pc, missingPunches);

            json missing = json::array();
            for (auto& mp : missingPunches)
                missing.push_back(mp.first);

            result["runnerId"]      = runner->getId();
            result["runnerName"]    = toUTF8(runner->getName());
            result["status"]        = statusToString(runner->getStatus());
            result["runningTime"]   = runner->getRunningTime(false);
            result["missingPunches"] = missing;
        } else {
            result["runnerId"]   = nullptr;
            result["runnerName"] = nullptr;
        }

        return okResponse(result);
    });

    // POST /api/v1/runners/{id}/status
    // Body: { "status": "DNS"|"DNF"|"DSQ"|"OK"|"MP" }
    router.POST(R"(/runners/(\d+)/status)", [&oe](const Request& req) -> json {
        int id = std::stoi(std::string(req.matches[1]));
        pRunner r = oe.getRunner(id, 0);
        if (!r) throw notFound("Runner not found: " + std::to_string(id));

        json body = parseBody(req.body);
        std::string statusStr = requireString(body, "status");

        RunnerStatus st = parseStatus(statusStr);
        r->setStatus(st, true, ChangeType::Update);

        return okResponse(json{
            {"id",     r->getId()},
            {"name",   toUTF8(r->getName())},
            {"status", statusToString(r->getStatus())}
        });
    });
}

} // namespace meos_net
