#include "CourseRepository.h"
#include "../util/meos_util.h"

namespace meos_db {

void CourseRepository::insert(const oCourse& course) {
    const auto* odata = course.getOData();
    int size = oCourse::getODataBlobSize();
    std::vector<DbParam> params{
        DbParam::Text(std::to_string(course.getId())),
        DbParam::Text(toUTF8(course.getName())),
        DbParam::Text(std::to_string(course.getLength())),
        DbParam::Text(course.getControls()),
        DbParam::Text(course.getLegLengths()),
        DbParam::Text(std::to_string(course.getStartId())),
        DbParam::Text(std::to_string(course.getFinishId())),
        DbParam::Blob(reinterpret_cast<const uint8_t*>(odata), static_cast<size_t>(size))
    };
    db_.executeMixed(
        "INSERT OR REPLACE INTO courses "
        "(id, name, length, control_ids, leg_lengths, start_control_id, finish_control_id, odata_blob) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
        params);
}

void CourseRepository::update(const oCourse& course) {
    insert(course);
}

void CourseRepository::remove(int id) {
    db_.executeParams("DELETE FROM courses WHERE id = ?", {std::to_string(id)});
}

std::optional<CourseRow> CourseRepository::findById(int id) const {
    std::vector<DbParam> params{DbParam::Text(std::to_string(id))};
    auto rows = db_.queryMixed(
        "SELECT id, name, length, control_ids, leg_lengths, start_control_id, finish_control_id, odata_blob "
        "FROM courses WHERE id = ?",
        params);
    if (rows.empty()) return std::nullopt;
    CourseRow r;
    r.id              = std::stoi(rows[0][0].second.text);
    r.name            = rows[0][1].second.text;
    r.length          = rows[0][2].second.text.empty() ? 0 : std::stoi(rows[0][2].second.text);
    r.controlIds      = rows[0][3].second.text;
    r.legLengths      = rows[0][4].second.text;
    r.startControlId  = rows[0][5].second.text.empty() ? 0 : std::stoi(rows[0][5].second.text);
    r.finishControlId = rows[0][6].second.text.empty() ? 0 : std::stoi(rows[0][6].second.text);
    r.odata           = rows[0][7].second.blob;
    return r;
}

std::vector<CourseRow> CourseRepository::findAll() const {
    auto rows = db_.queryMixed(
        "SELECT id, name, length, control_ids, leg_lengths, start_control_id, finish_control_id, odata_blob "
        "FROM courses", {});
    std::vector<CourseRow> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        CourseRow r;
        r.id              = std::stoi(row[0].second.text);
        r.name            = row[1].second.text;
        r.length          = row[2].second.text.empty() ? 0 : std::stoi(row[2].second.text);
        r.controlIds      = row[3].second.text;
        r.legLengths      = row[4].second.text;
        r.startControlId  = row[5].second.text.empty() ? 0 : std::stoi(row[5].second.text);
        r.finishControlId = row[6].second.text.empty() ? 0 : std::stoi(row[6].second.text);
        r.odata           = row[7].second.blob;
        result.push_back(std::move(r));
    }
    return result;
}

} // namespace meos_db
