#include "RunnerRepository.h"
#include "../util/meos_util.h"

namespace meos_db {

// Parse an integer field that may be NULL (stored as empty string).
static int parseIntOrZero(const std::string& s) {
    return s.empty() ? 0 : std::stoi(s);
}

void RunnerRepository::insert(const oRunner& runner) {
    const auto* odata = runner.getOData();
    int size = oRunner::getODataBlobSize();
    int clubId  = runner.getClubId();
    std::vector<DbParam> params{
        DbParam::Text(std::to_string(runner.getId())),
        DbParam::Text(toUTF8(runner.getName())),
        clubId > 0 ? DbParam::Text(std::to_string(clubId)) : DbParam::Null(),
        DbParam::Text(std::to_string(runner.getCardNo())),
        DbParam::Text(std::to_string(runner.getClassId(false))),
        DbParam::Text(std::to_string(runner.getCourseId())),
        DbParam::Text(std::to_string(runner.getStartNo())),
        DbParam::Text(std::to_string(runner.getStartTime())),
        DbParam::Text(std::to_string(runner.getFinishTime())),
        DbParam::Text(std::to_string(static_cast<int>(runner.getStatusComputed(false)))),
        DbParam::Blob(reinterpret_cast<const uint8_t*>(odata), static_cast<size_t>(size))
    };
    db_.executeMixed(
        "INSERT OR REPLACE INTO runners "
        "(id, name, club_id, card_no, class_id, course_id, start_no, start_time, finish_time, status, odata_blob) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)",
        params);
}

void RunnerRepository::update(const oRunner& runner) {
    insert(runner);
}

void RunnerRepository::remove(int id) {
    db_.executeParams("DELETE FROM runners WHERE id = ?", {std::to_string(id)});
}

std::optional<RunnerRow> RunnerRepository::findById(int id) const {
    std::vector<DbParam> params{DbParam::Text(std::to_string(id))};
    auto rows = db_.queryMixed(
        "SELECT id, name, club_id, card_no, class_id, course_id, start_no, start_time, finish_time, status, odata_blob "
        "FROM runners WHERE id = ?",
        params);
    if (rows.empty()) return std::nullopt;
    const auto& row = rows[0];
    RunnerRow r;
    r.id         = std::stoi(row[0].second.text);
    r.name       = row[1].second.text;
    r.clubId     = parseIntOrZero(row[2].second.text);
    r.cardNo     = parseIntOrZero(row[3].second.text);
    r.classId    = parseIntOrZero(row[4].second.text);
    r.courseId   = parseIntOrZero(row[5].second.text);
    r.startNo    = parseIntOrZero(row[6].second.text);
    r.startTime  = parseIntOrZero(row[7].second.text);
    r.finishTime = parseIntOrZero(row[8].second.text);
    r.status     = parseIntOrZero(row[9].second.text);
    r.odata      = row[10].second.blob;
    return r;
}

std::vector<RunnerRow> RunnerRepository::findAll() const {
    auto rows = db_.queryMixed(
        "SELECT id, name, club_id, card_no, class_id, course_id, start_no, start_time, finish_time, status, odata_blob "
        "FROM runners", {});
    std::vector<RunnerRow> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        RunnerRow r;
        r.id         = std::stoi(row[0].second.text);
        r.name       = row[1].second.text;
        r.clubId     = parseIntOrZero(row[2].second.text);
        r.cardNo     = parseIntOrZero(row[3].second.text);
        r.classId    = parseIntOrZero(row[4].second.text);
        r.courseId   = parseIntOrZero(row[5].second.text);
        r.startNo    = parseIntOrZero(row[6].second.text);
        r.startTime  = parseIntOrZero(row[7].second.text);
        r.finishTime = parseIntOrZero(row[8].second.text);
        r.status     = parseIntOrZero(row[9].second.text);
        r.odata      = row[10].second.blob;
        result.push_back(std::move(r));
    }
    return result;
}

} // namespace meos_db
