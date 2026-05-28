#include "database.h"

#include <sqlite3.h>
#include <sstream>
#include <stdexcept>

namespace meos::db {

Database::Database(const std::string& path) {
  int rc = sqlite3_open(path.c_str(), &db_);
  if (rc != SQLITE_OK) {
    std::string err = sqlite3_errmsg(db_);
    sqlite3_close(db_);
    db_ = nullptr;
    throw std::runtime_error("Failed to open database: " + err);
  }
  exec("PRAGMA foreign_keys=ON");
  exec("PRAGMA journal_mode=WAL");
}

Database::~Database() {
  if (db_) {
    sqlite3_close(db_);
  }
}

void Database::exec(const std::string& sql) {
  char* errMsg = nullptr;
  int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    std::string err = errMsg ? errMsg : "unknown error";
    sqlite3_free(errMsg);
    throw std::runtime_error("SQL error: " + err);
  }
}

void Database::createTables() {
  exec(R"(
    CREATE TABLE IF NOT EXISTS events (
      id         INTEGER PRIMARY KEY DEFAULT 1,
      name       TEXT    NOT NULL DEFAULT '',
      date       TEXT    NOT NULL DEFAULT '',
      zero_time  INTEGER NOT NULL DEFAULT 0,
      properties TEXT
    )
  )");

  exec(R"(
    CREATE TABLE IF NOT EXISTS competitions (
      id INTEGER PRIMARY KEY,
      name TEXT NOT NULL,
      date TEXT NOT NULL,
      organizer TEXT NOT NULL,
      location TEXT NOT NULL,
      description TEXT
    )
  )");

  exec(R"(
    CREATE TABLE IF NOT EXISTS clubs (
      id INTEGER PRIMARY KEY,
      name TEXT NOT NULL,
      country TEXT
    )
  )");

  exec(R"(
    CREATE TABLE IF NOT EXISTS controls (
      id INTEGER PRIMARY KEY,
      code INTEGER NOT NULL,
      description TEXT,
      type TEXT
    )
  )");

  exec(R"(
    CREATE TABLE IF NOT EXISTS courses (
      id INTEGER PRIMARY KEY,
      name TEXT NOT NULL,
      length INTEGER,
      controls TEXT
    )
  )");

  exec(R"(
    CREATE TABLE IF NOT EXISTS course_controls (
      course_id INTEGER NOT NULL,
      control_id INTEGER NOT NULL,
      sort_order INTEGER NOT NULL,
      FOREIGN KEY (course_id) REFERENCES courses(id),
      FOREIGN KEY (control_id) REFERENCES controls(id),
      PRIMARY KEY (course_id, sort_order)
    )
  )");

  exec(R"(
    CREATE TABLE IF NOT EXISTS classes (
      id INTEGER PRIMARY KEY,
      name TEXT NOT NULL,
      course_id INTEGER,
      start_method TEXT,
      FOREIGN KEY (course_id) REFERENCES courses(id)
    )
  )");

  exec(R"(
    CREATE TABLE IF NOT EXISTS runners (
      id INTEGER PRIMARY KEY,
      name TEXT NOT NULL,
      club_id INTEGER,
      class_id INTEGER,
      start_time TEXT,
      card_number INTEGER,
      status TEXT,
      FOREIGN KEY (club_id) REFERENCES clubs(id),
      FOREIGN KEY (class_id) REFERENCES classes(id)
    )
  )");

  exec(R"(
    CREATE TABLE IF NOT EXISTS teams (
      id INTEGER PRIMARY KEY,
      name TEXT NOT NULL,
      club_id INTEGER,
      class_id INTEGER,
      FOREIGN KEY (club_id) REFERENCES clubs(id),
      FOREIGN KEY (class_id) REFERENCES classes(id)
    )
  )");

  exec(R"(
    CREATE TABLE IF NOT EXISTS team_members (
      team_id INTEGER NOT NULL,
      runner_id INTEGER NOT NULL,
      FOREIGN KEY (team_id) REFERENCES teams(id),
      FOREIGN KEY (runner_id) REFERENCES runners(id),
      PRIMARY KEY (team_id, runner_id)
    )
  )");

  exec(R"(
    CREATE TABLE IF NOT EXISTS results (
      id INTEGER PRIMARY KEY,
      runner_id INTEGER NOT NULL,
      class_id INTEGER NOT NULL,
      position INTEGER,
      total_time INTEGER,
      status TEXT NOT NULL,
      FOREIGN KEY (runner_id) REFERENCES runners(id),
      FOREIGN KEY (class_id) REFERENCES classes(id)
    )
  )");

  exec(R"(
    CREATE TABLE IF NOT EXISTS splits (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      result_id INTEGER NOT NULL,
      control_id INTEGER NOT NULL,
      time INTEGER NOT NULL,
      FOREIGN KEY (result_id) REFERENCES results(id),
      FOREIGN KEY (control_id) REFERENCES controls(id)
    )
  )");

  exec(R"(
    CREATE TABLE IF NOT EXISTS start_list (
      id INTEGER PRIMARY KEY,
      runner_id INTEGER NOT NULL,
      class_id INTEGER NOT NULL,
      start_time TEXT NOT NULL,
      bib INTEGER,
      FOREIGN KEY (runner_id) REFERENCES runners(id),
      FOREIGN KEY (class_id) REFERENCES classes(id)
    )
  )");
}

// ---- Migration system -------------------------------------------------------

void Database::applyMigrations(const std::vector<Migration>& migrations) {
  exec(R"(
    CREATE TABLE IF NOT EXISTS _migrations (
      version     INTEGER PRIMARY KEY,
      description TEXT    NOT NULL,
      applied_at  TEXT    NOT NULL
    )
  )");

  int current = schemaVersion();

  for (const auto& m : migrations) {
    if (m.version <= current) continue;

    exec("BEGIN TRANSACTION");
    try {
      exec(m.sql);

      // Record the applied migration using a prepared statement to safely
      // bind string parameters.
      sqlite3_stmt* raw = nullptr;
      int rc = sqlite3_prepare_v2(
          db_,
          "INSERT INTO _migrations (version, description, applied_at) "
          "VALUES (?, ?, datetime('now'))",
          -1, &raw, nullptr);
      if (rc != SQLITE_OK) {
        exec("ROLLBACK");
        throw std::runtime_error(std::string("Prepare failed: ") +
                                 sqlite3_errmsg(db_));
      }
      sqlite3_bind_int(raw, 1, m.version);
      sqlite3_bind_text(raw, 2, m.description.c_str(), -1, SQLITE_TRANSIENT);
      rc = sqlite3_step(raw);
      sqlite3_finalize(raw);
      if (rc != SQLITE_DONE) {
        exec("ROLLBACK");
        throw std::runtime_error("Failed to record migration " +
                                 std::to_string(m.version));
      }
      exec("COMMIT");
    } catch (...) {
      // exec("ROLLBACK") may itself throw if the transaction was already
      // aborted; ignore that secondary error.
      char* errMsg = nullptr;
      sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, &errMsg);
      sqlite3_free(errMsg);
      throw;
    }
  }
}

int Database::schemaVersion() const {
  // Return 0 if the tracking table does not yet exist.
  sqlite3_stmt* check = nullptr;
  int rc = sqlite3_prepare_v2(
      db_,
      "SELECT name FROM sqlite_master WHERE type='table' AND name='_migrations'",
      -1, &check, nullptr);
  if (rc != SQLITE_OK) return 0;
  bool exists = sqlite3_step(check) == SQLITE_ROW;
  sqlite3_finalize(check);
  if (!exists) return 0;

  sqlite3_stmt* stmt = nullptr;
  rc = sqlite3_prepare_v2(db_, "SELECT MAX(version) FROM _migrations", -1,
                           &stmt, nullptr);
  if (rc != SQLITE_OK) return 0;
  int ver = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW &&
      sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
    ver = sqlite3_column_int(stmt, 0);
  }
  sqlite3_finalize(stmt);
  return ver;
}

// V1: core tables required to store runners and clubs.
std::vector<Migration> Database::v1Migrations() {
  return {
      {1, "Initial schema: runners and clubs",
       R"(
         CREATE TABLE IF NOT EXISTS clubs (
           id      INTEGER PRIMARY KEY,
           name    TEXT    NOT NULL,
           country TEXT
         );
         CREATE TABLE IF NOT EXISTS runners (
           id          INTEGER PRIMARY KEY,
           name        TEXT    NOT NULL,
           club_id     INTEGER,
           class_id    INTEGER,
           start_time  TEXT,
           card_number INTEGER,
           status      TEXT,
           FOREIGN KEY (club_id) REFERENCES clubs(id)
         );
       )"}};
}

// V2: controls and courses. Courses store their control sequence as a
// semicolon-separated list of control IDs in the `controls` TEXT column.
// Controls carry an `odata` BLOB column for DataContainer persistence.
std::vector<Migration> Database::v2Migrations() {
  return {
      {2, "Schema V2: controls and courses",
       R"(
         CREATE TABLE IF NOT EXISTS controls (
           id          INTEGER PRIMARY KEY,
           code        INTEGER NOT NULL,
           description TEXT,
           type        TEXT,
           odata       BLOB
         );
         CREATE TABLE IF NOT EXISTS courses (
           id       INTEGER PRIMARY KEY,
           name     TEXT    NOT NULL,
           length   INTEGER,
           controls TEXT
         );
       )"}};
}

// Helper for sqlite3_prepare/step/finalize
namespace {

struct StmtDeleter {
  void operator()(sqlite3_stmt* s) const { sqlite3_finalize(s); }
};
using StmtPtr = std::unique_ptr<sqlite3_stmt, StmtDeleter>;

StmtPtr prepare(sqlite3* db, const std::string& sql) {
  sqlite3_stmt* raw = nullptr;
  int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &raw, nullptr);
  if (rc != SQLITE_OK) {
    throw std::runtime_error(std::string("Prepare failed: ") + sqlite3_errmsg(db));
  }
  return StmtPtr(raw);
}

std::optional<std::string> optText(sqlite3_stmt* s, int col) {
  if (sqlite3_column_type(s, col) == SQLITE_NULL) return std::nullopt;
  return std::string(reinterpret_cast<const char*>(sqlite3_column_text(s, col)));
}

std::optional<int> optInt(sqlite3_stmt* s, int col) {
  if (sqlite3_column_type(s, col) == SQLITE_NULL) return std::nullopt;
  return sqlite3_column_int(s, col);
}

// Serialize/deserialize course control IDs as semicolon-separated string
// (matches legacy MeOS storage format).
std::string joinControls(const std::vector<int>& ids) {
  std::string result;
  for (size_t i = 0; i < ids.size(); ++i) {
    if (i > 0) result += ';';
    result += std::to_string(ids[i]);
  }
  return result;
}

std::vector<int> parseControls(const std::string& s) {
  std::vector<int> result;
  if (s.empty()) return result;
  std::istringstream ss(s);
  std::string token;
  while (std::getline(ss, token, ';')) {
    if (!token.empty()) result.push_back(std::stoi(token));
  }
  return result;
}

// Bind helpers shared by all CRUD write methods.
void bindOptFk(sqlite3_stmt* s, int col, std::optional<int> val) {
  if (!val.has_value() || *val == 0)
    sqlite3_bind_null(s, col);
  else
    sqlite3_bind_int(s, col, *val);
}

void bindOptInt(sqlite3_stmt* s, int col, std::optional<int> val) {
  if (!val.has_value())
    sqlite3_bind_null(s, col);
  else
    sqlite3_bind_int(s, col, *val);
}

void bindOptText(sqlite3_stmt* s, int col, std::optional<std::string> val) {
  if (!val.has_value())
    sqlite3_bind_null(s, col);
  else
    sqlite3_bind_text(s, col, val->c_str(), -1, SQLITE_TRANSIENT);
}

}  // namespace

// --- Clubs ---

std::vector<domain::Club> Database::getAllClubs() {
  auto stmt = prepare(db_, "SELECT id, name, country FROM clubs");
  std::vector<domain::Club> result;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Club c;
    c.id = sqlite3_column_int(stmt.get(), 0);
    c.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
    c.country = optText(stmt.get(), 2);
    result.push_back(std::move(c));
  }
  return result;
}

std::optional<domain::Club> Database::getClubById(int id) {
  auto stmt = prepare(db_, "SELECT id, name, country FROM clubs WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, id);
  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Club c;
    c.id = sqlite3_column_int(stmt.get(), 0);
    c.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
    c.country = optText(stmt.get(), 2);
    return c;
  }
  return std::nullopt;
}

// --- Controls ---

std::vector<domain::Control> Database::getAllControls() {
  auto stmt = prepare(db_, "SELECT id, code, description, type FROM controls");
  std::vector<domain::Control> result;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Control c;
    c.id = sqlite3_column_int(stmt.get(), 0);
    c.code = sqlite3_column_int(stmt.get(), 1);
    c.description = optText(stmt.get(), 2);
    c.type = optText(stmt.get(), 3);
    result.push_back(std::move(c));
  }
  return result;
}

std::optional<domain::Control> Database::getControlById(int id) {
  auto stmt = prepare(db_, "SELECT id, code, description, type FROM controls WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, id);
  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Control c;
    c.id = sqlite3_column_int(stmt.get(), 0);
    c.code = sqlite3_column_int(stmt.get(), 1);
    c.description = optText(stmt.get(), 2);
    c.type = optText(stmt.get(), 3);
    return c;
  }
  return std::nullopt;
}

// --- Courses ---

// Read the controls column as semicolon-separated text (V2 migration schema).
// Falls back to the course_controls join table (legacy createTables() schema)
// when the `controls` TEXT column is absent from the row result.
static std::vector<int> readCourseControls(sqlite3* db, int courseId,
                                            sqlite3_stmt* row, int col) {
  if (sqlite3_column_type(row, col) != SQLITE_NULL) {
    const char* raw =
        reinterpret_cast<const char*>(sqlite3_column_text(row, col));
    return parseControls(raw ? raw : "");
  }
  // Fallback: join-table schema from createTables()
  std::vector<int> ids;
  sqlite3_stmt* craw = nullptr;
  int rc = sqlite3_prepare_v2(
      db,
      "SELECT control_id FROM course_controls WHERE course_id=? ORDER BY sort_order",
      -1, &craw, nullptr);
  if (rc != SQLITE_OK) return ids;
  StmtPtr cstmt(craw);
  sqlite3_bind_int(cstmt.get(), 1, courseId);
  while (sqlite3_step(cstmt.get()) == SQLITE_ROW) {
    ids.push_back(sqlite3_column_int(cstmt.get(), 0));
  }
  return ids;
}

std::vector<domain::Course> Database::getAllCourses() {
  auto stmt = prepare(db_, "SELECT id, name, length, controls FROM courses");
  std::vector<domain::Course> courses;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Course c;
    c.id = sqlite3_column_int(stmt.get(), 0);
    c.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
    c.length = optInt(stmt.get(), 2);
    c.controls = readCourseControls(db_, c.id, stmt.get(), 3);
    courses.push_back(std::move(c));
  }
  return courses;
}

std::optional<domain::Course> Database::getCourseById(int id) {
  auto stmt =
      prepare(db_, "SELECT id, name, length, controls FROM courses WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, id);
  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Course c;
    c.id = sqlite3_column_int(stmt.get(), 0);
    c.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
    c.length = optInt(stmt.get(), 2);
    c.controls = readCourseControls(db_, c.id, stmt.get(), 3);
    return c;
  }
  return std::nullopt;
}

// --- Classes ---

std::vector<domain::Class> Database::getAllClasses() {
  auto stmt = prepare(db_, "SELECT id, name, course_id, start_method FROM classes");
  std::vector<domain::Class> result;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Class c;
    c.id = sqlite3_column_int(stmt.get(), 0);
    c.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
    c.courseId = optInt(stmt.get(), 2);
    c.startMethod = optText(stmt.get(), 3);
    result.push_back(std::move(c));
  }
  return result;
}

std::optional<domain::Class> Database::getClassById(int id) {
  auto stmt = prepare(db_, "SELECT id, name, course_id, start_method FROM classes WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, id);
  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Class c;
    c.id = sqlite3_column_int(stmt.get(), 0);
    c.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
    c.courseId = optInt(stmt.get(), 2);
    c.startMethod = optText(stmt.get(), 3);
    return c;
  }
  return std::nullopt;
}

// --- Runners ---

std::vector<domain::Runner> Database::getAllRunners() {
  auto stmt = prepare(db_,
    "SELECT id, name, club_id, class_id, start_time, card_number, status FROM runners");
  std::vector<domain::Runner> result;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Runner r;
    r.id = sqlite3_column_int(stmt.get(), 0);
    r.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
    r.clubId = optInt(stmt.get(), 2);
    r.classId = optInt(stmt.get(), 3);
    r.startTime = optText(stmt.get(), 4);
    r.cardNumber = optInt(stmt.get(), 5);
    r.status = optText(stmt.get(), 6);
    result.push_back(std::move(r));
  }
  return result;
}

std::optional<domain::Runner> Database::getRunnerById(int id) {
  auto stmt = prepare(db_,
    "SELECT id, name, club_id, class_id, start_time, card_number, status FROM runners WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, id);
  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Runner r;
    r.id = sqlite3_column_int(stmt.get(), 0);
    r.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
    r.clubId = optInt(stmt.get(), 2);
    r.classId = optInt(stmt.get(), 3);
    r.startTime = optText(stmt.get(), 4);
    r.cardNumber = optInt(stmt.get(), 5);
    r.status = optText(stmt.get(), 6);
    return r;
  }
  return std::nullopt;
}

// --- Teams ---

std::vector<domain::Team> Database::getAllTeams() {
  auto stmt = prepare(db_, "SELECT id, name, club_id, class_id FROM teams");
  std::vector<domain::Team> teams;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Team t;
    t.id = sqlite3_column_int(stmt.get(), 0);
    t.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
    t.clubId = optInt(stmt.get(), 2);
    t.classId = optInt(stmt.get(), 3);
    teams.push_back(std::move(t));
  }
  for (auto& team : teams) {
    auto mstmt = prepare(db_,
      "SELECT runner_id FROM team_members WHERE team_id=?");
    sqlite3_bind_int(mstmt.get(), 1, team.id);
    while (sqlite3_step(mstmt.get()) == SQLITE_ROW) {
      team.members.push_back(sqlite3_column_int(mstmt.get(), 0));
    }
  }
  return teams;
}

std::optional<domain::Team> Database::getTeamById(int id) {
  auto stmt = prepare(db_, "SELECT id, name, club_id, class_id FROM teams WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, id);
  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Team t;
    t.id = sqlite3_column_int(stmt.get(), 0);
    t.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
    t.clubId = optInt(stmt.get(), 2);
    t.classId = optInt(stmt.get(), 3);
    auto mstmt = prepare(db_,
      "SELECT runner_id FROM team_members WHERE team_id=?");
    sqlite3_bind_int(mstmt.get(), 1, t.id);
    while (sqlite3_step(mstmt.get()) == SQLITE_ROW) {
      t.members.push_back(sqlite3_column_int(mstmt.get(), 0));
    }
    return t;
  }
  return std::nullopt;
}

// --- Event (single-row upsert) -----------------------------------------------

void Database::saveEvent(const domain::Event& e) {
  auto stmt = prepare(db_,
    "INSERT OR REPLACE INTO events (id, name, date, zero_time, properties)"
    " VALUES (1, ?, ?, ?, ?)");
  sqlite3_bind_text(stmt.get(), 1, e.name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 2, e.date.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt.get(), 3, e.zeroTime);
  if (e.properties)
    sqlite3_bind_text(stmt.get(), 4, e.properties->c_str(), -1, SQLITE_TRANSIENT);
  else
    sqlite3_bind_null(stmt.get(), 4);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("saveEvent failed");
}

std::optional<domain::Event> Database::getEvent() const {
  sqlite3_stmt* raw = nullptr;
  int rc = sqlite3_prepare_v2(
      db_,
      "SELECT name, date, zero_time, properties FROM events WHERE id=1",
      -1, &raw, nullptr);
  if (rc != SQLITE_OK) return std::nullopt;
  StmtPtr stmt(raw);
  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Event e;
    e.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
    e.date = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
    e.zeroTime = sqlite3_column_int(stmt.get(), 2);
    e.properties = optText(stmt.get(), 3);
    return e;
  }
  return std::nullopt;
}

// ---- CRUD — Teams -----------------------------------------------------------

int Database::insertTeam(const domain::Team& t) {
  auto stmt = prepare(db_,
    "INSERT INTO teams (name, club_id, class_id) VALUES (?, ?, ?)");
  sqlite3_bind_text(stmt.get(), 1, t.name.c_str(), -1, SQLITE_TRANSIENT);
  bindOptFk(stmt.get(), 2, t.clubId);
  bindOptFk(stmt.get(), 3, t.classId);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("insertTeam failed");
  int teamId = static_cast<int>(sqlite3_last_insert_rowid(db_));
  for (int runnerId : t.members) {
    auto mstmt = prepare(db_,
      "INSERT INTO team_members (team_id, runner_id) VALUES (?, ?)");
    sqlite3_bind_int(mstmt.get(), 1, teamId);
    sqlite3_bind_int(mstmt.get(), 2, runnerId);
    if (sqlite3_step(mstmt.get()) != SQLITE_DONE)
      throw std::runtime_error("insertTeam: team_members insert failed");
  }
  return teamId;
}

void Database::updateTeam(const domain::Team& t) {
  auto stmt = prepare(db_,
    "UPDATE teams SET name=?, club_id=?, class_id=? WHERE id=?");
  sqlite3_bind_text(stmt.get(), 1, t.name.c_str(), -1, SQLITE_TRANSIENT);
  bindOptFk(stmt.get(), 2, t.clubId);
  bindOptFk(stmt.get(), 3, t.classId);
  sqlite3_bind_int(stmt.get(), 4, t.id);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("updateTeam failed");
  // Replace membership rows
  auto del = prepare(db_, "DELETE FROM team_members WHERE team_id=?");
  sqlite3_bind_int(del.get(), 1, t.id);
  sqlite3_step(del.get());
  for (int runnerId : t.members) {
    auto mstmt = prepare(db_,
      "INSERT INTO team_members (team_id, runner_id) VALUES (?, ?)");
    sqlite3_bind_int(mstmt.get(), 1, t.id);
    sqlite3_bind_int(mstmt.get(), 2, runnerId);
    if (sqlite3_step(mstmt.get()) != SQLITE_DONE)
      throw std::runtime_error("updateTeam: team_members insert failed");
  }
}

void Database::deleteTeam(int id) {
  auto del = prepare(db_, "DELETE FROM team_members WHERE team_id=?");
  sqlite3_bind_int(del.get(), 1, id);
  sqlite3_step(del.get());
  auto stmt = prepare(db_, "DELETE FROM teams WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, id);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("deleteTeam failed");
}

// --- Competitions ---

void Database::upsertCompetition(const domain::Competition& c) {
  auto stmt = prepare(db_,
    "INSERT OR REPLACE INTO competitions (id, name, date, organizer, location, description)"
    " VALUES (?, ?, ?, ?, ?, ?)");
  sqlite3_bind_int(stmt.get(), 1, c.id > 0 ? c.id : 1);
  sqlite3_bind_text(stmt.get(), 2, c.name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 3, c.date.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 4, c.organizer.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt.get(), 5, c.location.c_str(), -1, SQLITE_TRANSIENT);
  if (c.description)
    sqlite3_bind_text(stmt.get(), 6, c.description->c_str(), -1, SQLITE_TRANSIENT);
  else
    sqlite3_bind_null(stmt.get(), 6);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("upsertCompetition failed");
}

std::vector<domain::Competition> Database::getAllCompetitions() {
  auto stmt = prepare(db_,
    "SELECT id, name, date, organizer, location, description FROM competitions");
  std::vector<domain::Competition> result;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Competition c;
    c.id = sqlite3_column_int(stmt.get(), 0);
    c.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
    c.date = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
    c.organizer = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
    c.location = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 4));
    c.description = optText(stmt.get(), 5);
    result.push_back(std::move(c));
  }
  return result;
}

std::optional<domain::Competition> Database::getCompetitionById(int id) {
  auto stmt = prepare(db_,
    "SELECT id, name, date, organizer, location, description FROM competitions WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, id);
  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Competition c;
    c.id = sqlite3_column_int(stmt.get(), 0);
    c.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
    c.date = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
    c.organizer = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
    c.location = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 4));
    c.description = optText(stmt.get(), 5);
    return c;
  }
  return std::nullopt;
}

// --- Results ---

std::vector<domain::Result> Database::getAllResults() {
  auto stmt = prepare(db_,
    "SELECT id, runner_id, class_id, position, total_time, status FROM results");
  std::vector<domain::Result> results;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Result r;
    r.id = sqlite3_column_int(stmt.get(), 0);
    r.runnerId = sqlite3_column_int(stmt.get(), 1);
    r.classId = sqlite3_column_int(stmt.get(), 2);
    r.position = optInt(stmt.get(), 3);
    r.totalTime = optInt(stmt.get(), 4);
    r.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 5));
    results.push_back(std::move(r));
  }
  for (auto& res : results) {
    auto sstmt = prepare(db_,
      "SELECT control_id, time FROM splits WHERE result_id=?");
    sqlite3_bind_int(sstmt.get(), 1, res.id);
    while (sqlite3_step(sstmt.get()) == SQLITE_ROW) {
      domain::SplitTime st;
      st.controlId = sqlite3_column_int(sstmt.get(), 0);
      st.time = sqlite3_column_int(sstmt.get(), 1);
      res.splits.push_back(st);
    }
  }
  return results;
}

std::optional<domain::Result> Database::getResultById(int id) {
  auto stmt = prepare(db_,
    "SELECT id, runner_id, class_id, position, total_time, status FROM results WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, id);
  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Result r;
    r.id = sqlite3_column_int(stmt.get(), 0);
    r.runnerId = sqlite3_column_int(stmt.get(), 1);
    r.classId = sqlite3_column_int(stmt.get(), 2);
    r.position = optInt(stmt.get(), 3);
    r.totalTime = optInt(stmt.get(), 4);
    r.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 5));
    auto sstmt = prepare(db_,
      "SELECT control_id, time FROM splits WHERE result_id=?");
    sqlite3_bind_int(sstmt.get(), 1, r.id);
    while (sqlite3_step(sstmt.get()) == SQLITE_ROW) {
      domain::SplitTime st;
      st.controlId = sqlite3_column_int(sstmt.get(), 0);
      st.time = sqlite3_column_int(sstmt.get(), 1);
      r.splits.push_back(st);
    }
    return r;
  }
  return std::nullopt;
}

// --- Start List ---

std::vector<domain::StartListEntry> Database::getAllStartList() {
  auto stmt = prepare(db_,
    "SELECT id, runner_id, class_id, start_time, bib FROM start_list");
  std::vector<domain::StartListEntry> result;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::StartListEntry e;
    e.id = sqlite3_column_int(stmt.get(), 0);
    e.runnerId = sqlite3_column_int(stmt.get(), 1);
    e.classId = sqlite3_column_int(stmt.get(), 2);
    e.startTime = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
    e.bib = optInt(stmt.get(), 4);
    result.push_back(std::move(e));
  }
  return result;
}

std::optional<domain::StartListEntry> Database::getStartListEntryById(int id) {
  auto stmt = prepare(db_,
    "SELECT id, runner_id, class_id, start_time, bib FROM start_list WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, id);
  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::StartListEntry e;
    e.id = sqlite3_column_int(stmt.get(), 0);
    e.runnerId = sqlite3_column_int(stmt.get(), 1);
    e.classId = sqlite3_column_int(stmt.get(), 2);
    e.startTime = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
    e.bib = optInt(stmt.get(), 4);
    return e;
  }
  return std::nullopt;
}

// ---- CRUD — Clubs -----------------------------------------------------------

int Database::insertClub(const domain::Club& c) {
  auto stmt = prepare(db_,
    "INSERT INTO clubs (name, country) VALUES (?, ?)");
  sqlite3_bind_text(stmt.get(), 1, c.name.c_str(), -1, SQLITE_TRANSIENT);
  if (c.country)
    sqlite3_bind_text(stmt.get(), 2, c.country->c_str(), -1, SQLITE_TRANSIENT);
  else
    sqlite3_bind_null(stmt.get(), 2);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("insertClub failed");
  return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void Database::updateClub(const domain::Club& c) {
  auto stmt = prepare(db_,
    "UPDATE clubs SET name=?, country=? WHERE id=?");
  sqlite3_bind_text(stmt.get(), 1, c.name.c_str(), -1, SQLITE_TRANSIENT);
  if (c.country)
    sqlite3_bind_text(stmt.get(), 2, c.country->c_str(), -1, SQLITE_TRANSIENT);
  else
    sqlite3_bind_null(stmt.get(), 2);
  sqlite3_bind_int(stmt.get(), 3, c.id);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("updateClub failed");
}

void Database::deleteClub(int id) {
  auto stmt = prepare(db_, "DELETE FROM clubs WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, id);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("deleteClub failed");
}

// ---- CRUD — Controls --------------------------------------------------------

int Database::insertControl(const domain::Control& c) {
  auto stmt = prepare(db_,
    "INSERT INTO controls (code, description, type) VALUES (?, ?, ?)");
  sqlite3_bind_int(stmt.get(), 1, c.code);
  if (c.description)
    sqlite3_bind_text(stmt.get(), 2, c.description->c_str(), -1,
                      SQLITE_TRANSIENT);
  else
    sqlite3_bind_null(stmt.get(), 2);
  if (c.type)
    sqlite3_bind_text(stmt.get(), 3, c.type->c_str(), -1, SQLITE_TRANSIENT);
  else
    sqlite3_bind_null(stmt.get(), 3);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("insertControl failed");
  return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void Database::updateControl(const domain::Control& c) {
  auto stmt = prepare(db_,
    "UPDATE controls SET code=?, description=?, type=? WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, c.code);
  if (c.description)
    sqlite3_bind_text(stmt.get(), 2, c.description->c_str(), -1,
                      SQLITE_TRANSIENT);
  else
    sqlite3_bind_null(stmt.get(), 2);
  if (c.type)
    sqlite3_bind_text(stmt.get(), 3, c.type->c_str(), -1, SQLITE_TRANSIENT);
  else
    sqlite3_bind_null(stmt.get(), 3);
  sqlite3_bind_int(stmt.get(), 4, c.id);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("updateControl failed");
}

void Database::deleteControl(int id) {
  auto stmt = prepare(db_, "DELETE FROM controls WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, id);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("deleteControl failed");
}

// ---- CRUD — Courses ---------------------------------------------------------

int Database::insertCourse(const domain::Course& c) {
  auto stmt = prepare(db_,
    "INSERT INTO courses (name, length, controls) VALUES (?, ?, ?)");
  sqlite3_bind_text(stmt.get(), 1, c.name.c_str(), -1, SQLITE_TRANSIENT);
  if (c.length)
    sqlite3_bind_int(stmt.get(), 2, *c.length);
  else
    sqlite3_bind_null(stmt.get(), 2);
  std::string ctrlText = joinControls(c.controls);
  sqlite3_bind_text(stmt.get(), 3, ctrlText.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("insertCourse failed");
  return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void Database::updateCourse(const domain::Course& c) {
  auto stmt = prepare(db_,
    "UPDATE courses SET name=?, length=?, controls=? WHERE id=?");
  sqlite3_bind_text(stmt.get(), 1, c.name.c_str(), -1, SQLITE_TRANSIENT);
  if (c.length)
    sqlite3_bind_int(stmt.get(), 2, *c.length);
  else
    sqlite3_bind_null(stmt.get(), 2);
  std::string ctrlText = joinControls(c.controls);
  sqlite3_bind_text(stmt.get(), 3, ctrlText.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt.get(), 4, c.id);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("updateCourse failed");
}

void Database::deleteCourse(int id) {
  auto stmt = prepare(db_, "DELETE FROM courses WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, id);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("deleteCourse failed");
}

// ---- V3 Migration -----------------------------------------------------------

// V3: classes, cards, free_punches. Cards store punches as a TEXT punch_string.
std::vector<Migration> Database::v3Migrations() {
  return {
      {3, "Schema V3: classes, cards, and free_punches",
       R"(
         CREATE TABLE IF NOT EXISTS classes (
           id           INTEGER PRIMARY KEY,
           name         TEXT    NOT NULL,
           course_id    INTEGER,
           start_method TEXT,
           FOREIGN KEY (course_id) REFERENCES courses(id)
         );
         CREATE TABLE IF NOT EXISTS cards (
           id           INTEGER PRIMARY KEY,
           runner_id    INTEGER,
           card_number  INTEGER NOT NULL,
           punch_string TEXT,
           FOREIGN KEY (runner_id) REFERENCES runners(id)
         );
         CREATE TABLE IF NOT EXISTS free_punches (
           id          INTEGER PRIMARY KEY,
           code        INTEGER NOT NULL,
           punch_time  INTEGER NOT NULL,
           runner_id   INTEGER,
           card_number INTEGER,
           FOREIGN KEY (runner_id) REFERENCES runners(id)
         );
       )"}};
}

// V4: events (single-row competition metadata) and teams with member join table.
std::vector<Migration> Database::v4Migrations() {
  return {
      {4, "Schema V4: events and teams",
       R"(
        CREATE TABLE IF NOT EXISTS events (
          id         INTEGER PRIMARY KEY DEFAULT 1,
          name       TEXT    NOT NULL DEFAULT '',
          date       TEXT    NOT NULL DEFAULT '',
          zero_time  INTEGER NOT NULL DEFAULT 0,
          properties TEXT
        );
        CREATE TABLE IF NOT EXISTS teams (
          id       INTEGER PRIMARY KEY,
          name     TEXT    NOT NULL,
          club_id  INTEGER,
          class_id INTEGER,
          FOREIGN KEY (club_id)  REFERENCES clubs(id),
          FOREIGN KEY (class_id) REFERENCES classes(id)
        );
        CREATE TABLE IF NOT EXISTS team_members (
          team_id   INTEGER NOT NULL,
          runner_id INTEGER NOT NULL,
          leg       INTEGER NOT NULL DEFAULT 0,
          FOREIGN KEY (team_id)   REFERENCES teams(id),
          FOREIGN KEY (runner_id) REFERENCES runners(id),
          PRIMARY KEY (team_id, runner_id)
        );
       )"}};
}

// ---- CRUD — Classes ---------------------------------------------------------

int Database::insertClass(const domain::Class& c) {
  auto stmt = prepare(db_,
    "INSERT INTO classes (name, course_id, start_method) VALUES (?, ?, ?)");
  sqlite3_bind_text(stmt.get(), 1, c.name.c_str(), -1, SQLITE_TRANSIENT);
  bindOptFk(stmt.get(), 2, c.courseId);
  bindOptText(stmt.get(), 3, c.startMethod);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("insertClass failed");
  return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void Database::updateClass(const domain::Class& c) {
  auto stmt = prepare(db_,
    "UPDATE classes SET name=?, course_id=?, start_method=? WHERE id=?");
  sqlite3_bind_text(stmt.get(), 1, c.name.c_str(), -1, SQLITE_TRANSIENT);
  bindOptFk(stmt.get(), 2, c.courseId);
  bindOptText(stmt.get(), 3, c.startMethod);
  sqlite3_bind_int(stmt.get(), 4, c.id);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("updateClass failed");
}

void Database::deleteClass(int id) {
  auto stmt = prepare(db_, "DELETE FROM classes WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, id);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("deleteClass failed");
}

// ---- CRUD — Runners (insert/update/delete — read already existed) -----------

int Database::insertRunner(const domain::Runner& r) {
  auto stmt = prepare(db_,
    "INSERT INTO runners (name, club_id, class_id, start_time, card_number, status)"
    " VALUES (?, ?, ?, ?, ?, ?)");
  sqlite3_bind_text(stmt.get(), 1, r.name.c_str(), -1, SQLITE_TRANSIENT);
  bindOptFk(stmt.get(), 2, r.clubId);
  bindOptFk(stmt.get(), 3, r.classId);
  bindOptText(stmt.get(), 4, r.startTime);
  bindOptInt(stmt.get(), 5, r.cardNumber);
  bindOptText(stmt.get(), 6, r.status);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("insertRunner failed");
  return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void Database::updateRunner(const domain::Runner& r) {
  auto stmt = prepare(db_,
    "UPDATE runners SET name=?, club_id=?, class_id=?, start_time=?,"
    " card_number=?, status=? WHERE id=?");
  sqlite3_bind_text(stmt.get(), 1, r.name.c_str(), -1, SQLITE_TRANSIENT);
  bindOptFk(stmt.get(), 2, r.clubId);
  bindOptFk(stmt.get(), 3, r.classId);
  bindOptText(stmt.get(), 4, r.startTime);
  bindOptInt(stmt.get(), 5, r.cardNumber);
  bindOptText(stmt.get(), 6, r.status);
  sqlite3_bind_int(stmt.get(), 7, r.id);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("updateRunner failed");
}

void Database::deleteRunner(int id) {
  auto stmt = prepare(db_, "DELETE FROM runners WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, id);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("deleteRunner failed");
}

// ---- CRUD — Cards -----------------------------------------------------------

int Database::insertCard(const domain::Card& c) {
  auto stmt = prepare(db_,
    "INSERT INTO cards (runner_id, card_number, punch_string) VALUES (?, ?, ?)");
  bindOptFk(stmt.get(), 1, c.runnerId);
  sqlite3_bind_int(stmt.get(), 2, c.cardNumber);
  sqlite3_bind_text(stmt.get(), 3, c.punchString.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("insertCard failed");
  return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void Database::updateCard(const domain::Card& c) {
  auto stmt = prepare(db_,
    "UPDATE cards SET runner_id=?, card_number=?, punch_string=? WHERE id=?");
  bindOptFk(stmt.get(), 1, c.runnerId);
  sqlite3_bind_int(stmt.get(), 2, c.cardNumber);
  sqlite3_bind_text(stmt.get(), 3, c.punchString.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt.get(), 4, c.id);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("updateCard failed");
}

void Database::deleteCard(int id) {
  auto stmt = prepare(db_, "DELETE FROM cards WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, id);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("deleteCard failed");
}

std::vector<domain::Card> Database::getAllCards() {
  auto stmt = prepare(db_,
    "SELECT id, runner_id, card_number, punch_string FROM cards");
  std::vector<domain::Card> result;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Card c;
    c.id = sqlite3_column_int(stmt.get(), 0);
    c.runnerId = optInt(stmt.get(), 1);
    c.cardNumber = sqlite3_column_int(stmt.get(), 2);
    const char* ps = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
    c.punchString = ps ? ps : "";
    result.push_back(std::move(c));
  }
  return result;
}

std::optional<domain::Card> Database::getCardById(int id) {
  auto stmt = prepare(db_,
    "SELECT id, runner_id, card_number, punch_string FROM cards WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, id);
  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Card c;
    c.id = sqlite3_column_int(stmt.get(), 0);
    c.runnerId = optInt(stmt.get(), 1);
    c.cardNumber = sqlite3_column_int(stmt.get(), 2);
    const char* ps = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
    c.punchString = ps ? ps : "";
    return c;
  }
  return std::nullopt;
}

// ---- CRUD — Free Punches ----------------------------------------------------

int Database::insertFreePunch(const domain::FreePunch& fp) {
  auto stmt = prepare(db_,
    "INSERT INTO free_punches (code, punch_time, runner_id, card_number)"
    " VALUES (?, ?, ?, ?)");
  sqlite3_bind_int(stmt.get(), 1, fp.code);
  sqlite3_bind_int(stmt.get(), 2, fp.punchTime);
  bindOptFk(stmt.get(), 3, fp.runnerId);
  bindOptInt(stmt.get(), 4, fp.cardNumber);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("insertFreePunch failed");
  return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void Database::updateFreePunch(const domain::FreePunch& fp) {
  auto stmt = prepare(db_,
    "UPDATE free_punches SET code=?, punch_time=?, runner_id=?, card_number=?"
    " WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, fp.code);
  sqlite3_bind_int(stmt.get(), 2, fp.punchTime);
  bindOptFk(stmt.get(), 3, fp.runnerId);
  bindOptInt(stmt.get(), 4, fp.cardNumber);
  sqlite3_bind_int(stmt.get(), 5, fp.id);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("updateFreePunch failed");
}

void Database::deleteFreePunch(int id) {
  auto stmt = prepare(db_, "DELETE FROM free_punches WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, id);
  if (sqlite3_step(stmt.get()) != SQLITE_DONE)
    throw std::runtime_error("deleteFreePunch failed");
}

std::vector<domain::FreePunch> Database::getAllFreePunches() {
  auto stmt = prepare(db_,
    "SELECT id, code, punch_time, runner_id, card_number FROM free_punches");
  std::vector<domain::FreePunch> result;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::FreePunch fp;
    fp.id = sqlite3_column_int(stmt.get(), 0);
    fp.code = sqlite3_column_int(stmt.get(), 1);
    fp.punchTime = sqlite3_column_int(stmt.get(), 2);
    fp.runnerId = optInt(stmt.get(), 3);
    fp.cardNumber = optInt(stmt.get(), 4);
    result.push_back(std::move(fp));
  }
  return result;
}

std::optional<domain::FreePunch> Database::getFreePunchById(int id) {
  auto stmt = prepare(db_,
    "SELECT id, code, punch_time, runner_id, card_number FROM free_punches WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, id);
  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::FreePunch fp;
    fp.id = sqlite3_column_int(stmt.get(), 0);
    fp.code = sqlite3_column_int(stmt.get(), 1);
    fp.punchTime = sqlite3_column_int(stmt.get(), 2);
    fp.runnerId = optInt(stmt.get(), 3);
    fp.cardNumber = optInt(stmt.get(), 4);
    return fp;
  }
  return std::nullopt;
}

}  // namespace meos::db
