#include "FreePunchRepository.h"

namespace meos_db {

void FreePunchRepository::insert(const oFreePunch& punch) {
    std::vector<DbParam> params{
        DbParam::Text(std::to_string(punch.getId())),
        DbParam::Text(std::to_string(punch.getCardNo())),
        DbParam::Text(std::to_string(punch.getTypeCode())),
        DbParam::Text(std::to_string(punch.getTimeInt())),
        DbParam::Text(std::to_string(punch.getTiedRunnerId()))
    };
    db_.executeMixed(
        "INSERT OR REPLACE INTO free_punches (id, card_no, type_code, time_int, runner_id) VALUES (?, ?, ?, ?, ?)",
        params);
}

void FreePunchRepository::update(const oFreePunch& punch) {
    insert(punch);
}

void FreePunchRepository::remove(int id) {
    db_.executeParams("DELETE FROM free_punches WHERE id = ?", {std::to_string(id)});
}

std::optional<FreePunchRow> FreePunchRepository::findById(int id) const {
    std::vector<DbParam> params{DbParam::Text(std::to_string(id))};
    auto rows = db_.queryMixed(
        "SELECT id, card_no, type_code, time_int, runner_id FROM free_punches WHERE id = ?",
        params);
    if (rows.empty()) return std::nullopt;
    const auto& row = rows[0];
    FreePunchRow r;
    r.id       = std::stoi(row[0].second.text);
    r.cardNo   = std::stoi(row[1].second.text);
    r.typeCode = std::stoi(row[2].second.text);
    r.timeInt  = std::stoi(row[3].second.text);
    r.runnerId = std::stoi(row[4].second.text);
    return r;
}

std::vector<FreePunchRow> FreePunchRepository::findAll() const {
    auto rows = db_.queryMixed("SELECT id, card_no, type_code, time_int, runner_id FROM free_punches", {});
    std::vector<FreePunchRow> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        FreePunchRow r;
        r.id       = std::stoi(row[0].second.text);
        r.cardNo   = std::stoi(row[1].second.text);
        r.typeCode = std::stoi(row[2].second.text);
        r.timeInt  = std::stoi(row[3].second.text);
        r.runnerId = std::stoi(row[4].second.text);
        result.push_back(std::move(r));
    }
    return result;
}

} // namespace meos_db
