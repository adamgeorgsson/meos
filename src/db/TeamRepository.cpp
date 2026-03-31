#include "TeamRepository.h"
#include "../util/meos_util.h"

namespace meos_db {

static int parseIntOrZero(const std::string& s) {
    return s.empty() ? 0 : std::stoi(s);
}

void TeamRepository::insert(const oTeam& team) {
    const auto* odata = team.getOData();
    int size = oTeam::getODataBlobSize();
    std::vector<DbParam> params{
        DbParam::Text(std::to_string(team.getId())),
        DbParam::Text(toUTF8(team.getName())),
        DbParam::Text(std::to_string(team.getClubId())),
        DbParam::Text(std::to_string(team.getClassId(false))),
        DbParam::Text(std::to_string(team.getStartNo())),
        DbParam::Text(std::to_string(team.getStartTime())),
        DbParam::Text(std::to_string(team.getFinishTime())),
        DbParam::Text(std::to_string(static_cast<int>(team.getStatusComputed(false)))),
        DbParam::Text(team.getRunnerIdString()),
        DbParam::Blob(reinterpret_cast<const uint8_t*>(odata), static_cast<size_t>(size))
    };
    db_.executeMixed(
        "INSERT OR REPLACE INTO teams "
        "(id, name, club_id, class_id, start_no, start_time, finish_time, status, runner_ids, odata_blob) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
        params);
}

void TeamRepository::update(const oTeam& team) {
    insert(team);
}

void TeamRepository::remove(int id) {
    db_.executeParams("DELETE FROM teams WHERE id = ?", {std::to_string(id)});
}

TeamRow TeamRepository::rowFromResult(const DbExtRow& row) {
    TeamRow t;
    t.id         = std::stoi(row[0].second.text);
    t.name       = row[1].second.text;
    t.clubId     = parseIntOrZero(row[2].second.text);
    t.classId    = parseIntOrZero(row[3].second.text);
    t.startNo    = parseIntOrZero(row[4].second.text);
    t.startTime  = parseIntOrZero(row[5].second.text);
    t.finishTime = parseIntOrZero(row[6].second.text);
    t.status     = parseIntOrZero(row[7].second.text);
    t.runnerIds  = row[8].second.text;
    if (row[9].second.isBlobColumn) {
        t.odata = row[9].second.blob;
    }
    return t;
}

std::optional<TeamRow> TeamRepository::findById(int id) const {
    std::vector<DbParam> params{DbParam::Text(std::to_string(id))};
    auto rows = db_.queryMixed(
        "SELECT id, name, club_id, class_id, start_no, start_time, finish_time, status, runner_ids, odata_blob "
        "FROM teams WHERE id = ?",
        params);
    if (rows.empty()) return std::nullopt;
    return rowFromResult(rows[0]);
}

std::vector<TeamRow> TeamRepository::findAll() const {
    auto rows = db_.queryMixed(
        "SELECT id, name, club_id, class_id, start_no, start_time, finish_time, status, runner_ids, odata_blob "
        "FROM teams",
        {});
    std::vector<TeamRow> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        result.push_back(rowFromResult(row));
    }
    return result;
}

} // namespace meos_db
