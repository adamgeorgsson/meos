#include "EventRepository.h"
#include "../util/meos_util.h"

namespace meos_db {

// ---------------------------------------------------------------------------
// Property encoding: pairs separated by \x02, key and value separated by \x01
// Avoids JSON dependency while remaining round-trip safe for well-formed keys.
// ---------------------------------------------------------------------------

std::string EventRepository::encodeProperties(const std::map<std::string, std::wstring>& props) {
    std::string result;
    for (const auto& kv : props) {
        if (!result.empty()) result += '\x02';
        result += kv.first;
        result += '\x01';
        result += toUTF8(kv.second);
    }
    return result;
}

std::map<std::string, std::wstring> EventRepository::decodeProperties(const std::string& encoded) {
    std::map<std::string, std::wstring> result;
    if (encoded.empty()) return result;
    size_t pos = 0;
    while (pos < encoded.size()) {
        size_t sep = encoded.find('\x01', pos);
        if (sep == std::string::npos) break;
        std::string key = encoded.substr(pos, sep - pos);
        size_t end = encoded.find('\x02', sep + 1);
        std::string valUtf8 = (end == std::string::npos)
            ? encoded.substr(sep + 1)
            : encoded.substr(sep + 1, end - sep - 1);
        result[key] = fromUTF8(valUtf8);
        pos = (end == std::string::npos) ? encoded.size() : end + 1;
    }
    return result;
}

// ---------------------------------------------------------------------------

void EventRepository::save(const oEvent& ev, int id) {
    std::string propsEncoded = encodeProperties(ev.eventProperties);
    std::vector<DbParam> params{
        DbParam::Text(std::to_string(id)),
        DbParam::Text(toUTF8(ev.getName())),
        DbParam::Text(toUTF8(ev.getDate())),
        DbParam::Text(std::to_string(ev.getZeroTimeNum())),
        DbParam::Text(propsEncoded)
    };
    db_.executeMixed(
        "INSERT OR REPLACE INTO events (id, name, date, zero_time, properties_json) "
        "VALUES (?, ?, ?, ?, ?)",
        params);
}

std::optional<EventRow> EventRepository::load(int id) const {
    std::vector<DbParam> params{DbParam::Text(std::to_string(id))};
    auto rows = db_.queryMixed(
        "SELECT id, name, date, zero_time, properties_json FROM events WHERE id = ?",
        params);
    if (rows.empty()) return std::nullopt;
    const auto& row = rows[0];
    EventRow r;
    r.id               = std::stoi(row[0].second.text);
    r.name             = row[1].second.text;
    r.date             = row[2].second.text;
    r.zeroTime         = row[3].second.text.empty() ? 0 : std::stoi(row[3].second.text);
    r.propertiesEncoded = row[4].second.text;
    return r;
}

} // namespace meos_db
