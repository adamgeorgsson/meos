#include "ClubRepository.h"
#include "../util/meos_util.h"

namespace meos_db {

void ClubRepository::insert(const oClub& club) {
    const auto* odata = club.getOData();
    int size = oClub::getODataBlobSize();
    std::vector<DbParam> params{
        DbParam::Text(std::to_string(club.getId())),
        DbParam::Text(toUTF8(club.getName())),
        DbParam::Blob(reinterpret_cast<const uint8_t*>(odata), static_cast<size_t>(size))
    };
    db_.executeMixed(
        "INSERT OR REPLACE INTO clubs (id, name, odata_blob) VALUES (?, ?, ?)",
        params);
}

void ClubRepository::update(const oClub& club) {
    insert(club); // INSERT OR REPLACE handles upsert
}

void ClubRepository::remove(int id) {
    db_.executeParams("DELETE FROM clubs WHERE id = ?", {std::to_string(id)});
}

std::optional<ClubRow> ClubRepository::findById(int id) const {
    std::vector<DbParam> params{DbParam::Text(std::to_string(id))};
    auto rows = db_.queryMixed(
        "SELECT id, name, odata_blob FROM clubs WHERE id = ?",
        params);
    if (rows.empty()) return std::nullopt;
    ClubRow r;
    r.id    = std::stoi(rows[0][0].second.text);
    r.name  = rows[0][1].second.text;
    r.odata = rows[0][2].second.blob;
    return r;
}

std::vector<ClubRow> ClubRepository::findAll() const {
    auto rows = db_.queryMixed("SELECT id, name, odata_blob FROM clubs", {});
    std::vector<ClubRow> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        ClubRow r;
        r.id    = std::stoi(row[0].second.text);
        r.name  = row[1].second.text;
        r.odata = row[2].second.blob;
        result.push_back(std::move(r));
    }
    return result;
}

} // namespace meos_db
