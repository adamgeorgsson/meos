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

}  // namespace meos::domain
