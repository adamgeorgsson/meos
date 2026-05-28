#pragma once

#include <optional>
#include <string>
#include <vector>

namespace meos::domain {

struct Competition {
  int id;
  std::string name;
  std::string date;
  std::string organizer;
  std::string location;
  std::optional<std::string> description;
};

// Single-row event metadata for the active competition.
// Persisted via INSERT OR REPLACE with id=1 (upsert semantics).
struct Event {
  std::string name;
  std::string date;           // YYYY-MM-DD
  int zeroTime = 0;           // competition zero time (seconds since midnight)
  std::optional<std::string> properties;  // serialized key=value settings
};

struct Club {
  int id;
  std::string name;
  std::optional<std::string> country;
};

struct Control {
  int id;
  int code;
  std::optional<std::string> description;
  std::optional<std::string> type;
};

struct Course {
  int id;
  std::string name;
  std::optional<int> length;
  std::vector<int> controls;
};

struct Class {
  int id;
  std::string name;
  std::optional<int> courseId;
  std::optional<std::string> startMethod;
};

struct Runner {
  int id;
  std::string name;
  std::optional<int> clubId;
  std::optional<int> classId;
  std::optional<std::string> startTime;
  std::optional<int> cardNumber;
  std::optional<std::string> status;
};

struct Team {
  int id;
  std::string name;
  std::optional<int> clubId;
  std::optional<int> classId;
  std::vector<int> members;
};

struct SplitTime {
  int controlId;
  int time;
};

struct Result {
  int id;
  int runnerId;
  int classId;
  std::optional<int> position;
  std::optional<int> totalTime;
  std::string status;
  std::vector<SplitTime> splits;
};

struct StartListEntry {
  int id;
  int runnerId;
  int classId;
  std::string startTime;
  std::optional<int> bib;
};

// SI card readout — punches serialized as punch_string (not oData blob).
struct Card {
  int id;
  std::optional<int> runnerId;  // NULL if unassigned
  int cardNumber;               // SI card number
  std::string punchString;      // serialized via getPunchString()
};

// A free punch not yet assigned to a card/runner result.
struct FreePunch {
  int id;
  int code;         // control code punched
  int punchTime;    // time in tenths-of-second units
  std::optional<int> runnerId;
  std::optional<int> cardNumber;
};

}  // namespace meos::domain
