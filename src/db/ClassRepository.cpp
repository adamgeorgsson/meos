#include "ClassRepository.h"
#include "../util/meos_util.h"

namespace meos_db {

void ClassRepository::insert(const oClass& cls) {
    const auto* odata = cls.getOData();
    int size = oClass::getODataBlobSize();
    std::vector<DbParam> params{
        DbParam::Text(std::to_string(cls.getId())),
        DbParam::Text(toUTF8(cls.getName())),
        DbParam::Text(std::to_string(cls.getCourseId())),
        DbParam::Text(std::to_string(cls.getNumLegs())),
        DbParam::Blob(reinterpret_cast<const uint8_t*>(odata), static_cast<size_t>(size))
    };
    db_.executeMixed(
        "INSERT OR REPLACE INTO classes (id, name, course_id, num_legs, odata_blob) VALUES (?, ?, ?, ?, ?)",
        params);
}

void ClassRepository::update(const oClass& cls) {
    insert(cls);
}

void ClassRepository::remove(int id) {
    db_.executeParams("DELETE FROM classes WHERE id = ?", {std::to_string(id)});
}

std::optional<ClassRow> ClassRepository::findById(int id) const {
    std::vector<DbParam> params{DbParam::Text(std::to_string(id))};
    auto rows = db_.queryMixed(
        "SELECT id, name, course_id, num_legs, odata_blob FROM classes WHERE id = ?",
        params);
    if (rows.empty()) return std::nullopt;
    ClassRow r;
    r.id       = std::stoi(rows[0][0].second.text);
    r.name     = rows[0][1].second.text;
    r.courseId = std::stoi(rows[0][2].second.text);
    r.numLegs  = std::stoi(rows[0][3].second.text);
    r.odata    = rows[0][4].second.blob;
    return r;
}

std::vector<ClassRow> ClassRepository::findAll() const {
    auto rows = db_.queryMixed("SELECT id, name, course_id, num_legs, odata_blob FROM classes", {});
    std::vector<ClassRow> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        ClassRow r;
        r.id       = std::stoi(row[0].second.text);
        r.name     = row[1].second.text;
        r.courseId = std::stoi(row[2].second.text);
        r.numLegs  = std::stoi(row[3].second.text);
        r.odata    = row[4].second.blob;
        result.push_back(std::move(r));
    }
    return result;
}

} // namespace meos_db
