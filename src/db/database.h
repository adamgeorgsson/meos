#pragma once

#include "entities.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace meos::db {

// A versioned database schema migration.
struct Migration {
  int version;
  std::string description;
  std::string sql;  // DDL to execute (no transaction wrappers)
};

class Database {
 public:
  explicit Database(const std::string& path);
  ~Database();

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  // Legacy: create all tables in one shot (no migration tracking).
  void createTables();

  // Apply pending migrations atomically. Creates the _migrations tracking
  // table on first use. Each migration is wrapped in its own transaction;
  // on failure the transaction is rolled back and the exception re-thrown,
  // leaving _migrations at the last good version.
  void applyMigrations(const std::vector<Migration>& migrations);

  // Return the highest applied migration version, or 0 if none.
  int schemaVersion() const;

  // Canonical V1 migration: runners and clubs tables.
  static std::vector<Migration> v1Migrations();

  // V2 migration: controls and courses tables (courses use semicolon-separated
  // control IDs; controls have an odata BLOB column for DataContainer fields).
  static std::vector<Migration> v2Migrations();

  // V3 migration: classes, cards, and free_punches tables (complex entities
  // with FK relationships). Runners already exist from V1 but gain CRUD here.
  static std::vector<Migration> v3Migrations();

  // V4 migration: events (single-row competition metadata) and teams tables.
  static std::vector<Migration> v4Migrations();

  // CRUD — Teams (runner membership stored via team_members join table)
  int insertTeam(const domain::Team& t);
  void updateTeam(const domain::Team& t);
  void deleteTeam(int id);

  // Event upsert — single-row INSERT OR REPLACE with id=1.
  void saveEvent(const domain::Event& e);
  std::optional<domain::Event> getEvent() const;

  // CRUD — Clubs
  int insertClub(const domain::Club& c);   // returns last_insert_rowid
  void updateClub(const domain::Club& c);
  void deleteClub(int id);

  // CRUD — Controls
  int insertControl(const domain::Control& c);
  void updateControl(const domain::Control& c);
  void deleteControl(int id);

  // CRUD — Courses (controls stored as semicolon-separated IDs in TEXT column)
  int insertCourse(const domain::Course& c);
  void updateCourse(const domain::Course& c);
  void deleteCourse(int id);

  // CRUD — Classes
  int insertClass(const domain::Class& c);
  void updateClass(const domain::Class& c);
  void deleteClass(int id);

  // CRUD — Runners
  int insertRunner(const domain::Runner& r);
  void updateRunner(const domain::Runner& r);
  void deleteRunner(int id);

  // CRUD — Cards (punches stored as punch_string TEXT)
  int insertCard(const domain::Card& c);
  void updateCard(const domain::Card& c);
  void deleteCard(int id);

  // CRUD — Free Punches
  int insertFreePunch(const domain::FreePunch& fp);
  void updateFreePunch(const domain::FreePunch& fp);
  void deleteFreePunch(int id);

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

  std::vector<domain::Card> getAllCards();
  std::optional<domain::Card> getCardById(int id);

  std::vector<domain::FreePunch> getAllFreePunches();
  std::optional<domain::FreePunch> getFreePunchById(int id);

  std::vector<domain::Team> getAllTeams();
  std::optional<domain::Team> getTeamById(int id);

  // Competition upsert — inserts or updates the row with the given id.
  void upsertCompetition(const domain::Competition& c);

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
