#include "api.h"

#include <nlohmann/json.hpp>

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

json makeError(int status, const std::string& message) {
    return json{{"message", message}, {"status", status}};
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

}  // namespace meos::net
