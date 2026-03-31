#pragma once
#include "SQLiteDatabase.h"
#include "../domain/oEvent.h"
#include <string>
#include <map>
#include <optional>

namespace meos_db {

struct EventRow {
    int id = 0;
    std::string name;            ///< UTF-8 encoded
    std::string date;            ///< UTF-8 encoded
    int zeroTime = 0;
    /// Serialised eventProperties: key\x01value\x02key\x01value (no JSON dependency)
    std::string propertiesEncoded;
};

class EventRepository {
public:
    explicit EventRepository(SQLiteDatabase& db) : db_(db) {}

    /// Persist the event. Uses id=1 as the singleton row (single-competition model).
    void save(const oEvent& ev, int id = 1);

    /// Load event row by id.
    std::optional<EventRow> load(int id = 1) const;

    /// Decode the propertiesEncoded string back into a map<string,wstring>.
    static std::map<std::string, std::wstring> decodeProperties(const std::string& encoded);

    /// Encode eventProperties map to the stored string format.
    static std::string encodeProperties(const std::map<std::string, std::wstring>& props);

private:
    SQLiteDatabase& db_;
};

} // namespace meos_db
