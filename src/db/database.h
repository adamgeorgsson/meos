#pragma once

#include "entities.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace meos::db {

class Database {
 public:
  explicit Database(const std::string& path);
  ~Database();

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  void createTables();

  std::vector<domain::Club> getAllClubs();
  std::optional<domain::Club> getClubById(int id);

  std::vector<domain::Control> getAllControls();
  std::optional<domain::Control> getControlById(int id);

  std::vector<domain::Course> getAllCourses();
  std::optional<domain::Course> getCourseById(int id);

  std::vector<domain::Class> getAllClasses();
  std::optional<domain::Class> getClassById(int id);

  std::vector<domain::Runner> getAllRunners();
  std::optional<domain::Runner> getRunnerById(int id);

  std::vector<domain::Team> getAllTeams();
  std::optional<domain::Team> getTeamById(int id);

  std::vector<domain::Competition> getAllCompetitions();
  std::optional<domain::Competition> getCompetitionById(int id);

  std::vector<domain::Result> getAllResults();
  std::optional<domain::Result> getResultById(int id);

  std::vector<domain::StartListEntry> getAllStartList();
  std::optional<domain::StartListEntry> getStartListEntryById(int id);

  sqlite3* handle() { return db_; }

 private:
  sqlite3* db_ = nullptr;
  void exec(const std::string& sql);
};

}  // namespace meos::db
