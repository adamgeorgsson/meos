#include "ControlRepository.h"
#include "../util/meos_util.h"

namespace meos_db {

void ControlRepository::insert(const oControl& ctrl) {
    const auto* odata = ctrl.getOData();
    int size = oControl::getODataBlobSize();
    std::vector<DbParam> params{
        DbParam::Text(std::to_string(ctrl.getId())),
        DbParam::Text(toUTF8(ctrl.getName())),
        DbParam::Text(toUTF8(ctrl.codeNumbers(';'))),
        DbParam::Text(std::to_string(static_cast<int>(ctrl.getStatus()))),
        DbParam::Blob(reinterpret_cast<const uint8_t*>(odata), static_cast<size_t>(size))
    };
    db_.executeMixed(
        "INSERT OR REPLACE INTO controls (id, name, numbers, status, odata_blob) VALUES (?, ?, ?, ?, ?)",
        params);
}

void ControlRepository::update(const oControl& ctrl) {
    insert(ctrl);
}

void ControlRepository::remove(int id) {
    db_.executeParams("DELETE FROM controls WHERE id = ?", {std::to_string(id)});
}

std::optional<ControlRow> ControlRepository::findById(int id) const {
    std::vector<DbParam> params{DbParam::Text(std::to_string(id))};
    auto rows = db_.queryMixed(
        "SELECT id, name, numbers, status, odata_blob FROM controls WHERE id = ?",
        params);
    if (rows.empty()) return std::nullopt;
    ControlRow r;
    r.id      = std::stoi(rows[0][0].second.text);
    r.name    = rows[0][1].second.text;
    r.numbers = rows[0][2].second.text;
    r.status  = rows[0][3].second.text.empty() ? 0 : std::stoi(rows[0][3].second.text);
    r.odata   = rows[0][4].second.blob;
    return r;
}

std::vector<ControlRow> ControlRepository::findAll() const {
    auto rows = db_.queryMixed("SELECT id, name, numbers, status, odata_blob FROM controls", {});
    std::vector<ControlRow> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        ControlRow r;
        r.id      = std::stoi(row[0].second.text);
        r.name    = row[1].second.text;
        r.numbers = row[2].second.text;
        r.status  = row[3].second.text.empty() ? 0 : std::stoi(row[3].second.text);
        r.odata   = row[4].second.blob;
        result.push_back(std::move(r));
    }
    return result;
}

} // namespace meos_db
