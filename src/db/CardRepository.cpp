#include "CardRepository.h"
#include "../domain/oRunner.h"

namespace meos_db {

// Parse an integer field that may be NULL (stored as empty string).
static int parseIntOrZero(const std::string& s) {
    return s.empty() ? 0 : std::stoi(s);
}

void CardRepository::insert(const oCard& card) {
    int ownerId = card.getOwner() ? card.getOwner()->getId() : 0;
    std::vector<DbParam> params{
        DbParam::Text(std::to_string(card.getId())),
        DbParam::Text(std::to_string(card.getCardNo())),
        ownerId > 0 ? DbParam::Text(std::to_string(ownerId)) : DbParam::Null(),
        DbParam::Text(card.getPunchString())
    };
    db_.executeMixed(
        "INSERT OR REPLACE INTO cards (id, card_no, runner_id, punch_string) VALUES (?, ?, ?, ?)",
        params);
}

void CardRepository::update(const oCard& card) {
    insert(card);
}

void CardRepository::remove(int id) {
    db_.executeParams("DELETE FROM cards WHERE id = ?", {std::to_string(id)});
}

std::optional<CardRow> CardRepository::findById(int id) const {
    std::vector<DbParam> params{DbParam::Text(std::to_string(id))};
    auto rows = db_.queryMixed(
        "SELECT id, card_no, runner_id, punch_string FROM cards WHERE id = ?",
        params);
    if (rows.empty()) return std::nullopt;
    const auto& row = rows[0];
    CardRow r;
    r.id          = std::stoi(row[0].second.text);
    r.cardNo      = std::stoi(row[1].second.text);
    r.runnerId    = parseIntOrZero(row[2].second.text);
    r.punchString = row[3].second.text;
    return r;
}

std::vector<CardRow> CardRepository::findAll() const {
    auto rows = db_.queryMixed("SELECT id, card_no, runner_id, punch_string FROM cards", {});
    std::vector<CardRow> result;
    result.reserve(rows.size());
    for (const auto& row : rows) {
        CardRow r;
        r.id          = std::stoi(row[0].second.text);
        r.cardNo      = std::stoi(row[1].second.text);
        r.runnerId    = parseIntOrZero(row[2].second.text);
        r.punchString = row[3].second.text;
        result.push_back(std::move(r));
    }
    return result;
}

} // namespace meos_db
