#include "database.h"

#include <sqlite3.h>
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
      length INTEGER
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

std::vector<domain::Course> Database::getAllCourses() {
  auto stmt = prepare(db_, "SELECT id, name, length FROM courses");
  std::vector<domain::Course> courses;
  while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Course c;
    c.id = sqlite3_column_int(stmt.get(), 0);
    c.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
    c.length = optInt(stmt.get(), 2);
    courses.push_back(std::move(c));
  }
  for (auto& course : courses) {
    auto cstmt = prepare(db_,
      "SELECT control_id FROM course_controls WHERE course_id=? ORDER BY sort_order");
    sqlite3_bind_int(cstmt.get(), 1, course.id);
    while (sqlite3_step(cstmt.get()) == SQLITE_ROW) {
      course.controls.push_back(sqlite3_column_int(cstmt.get(), 0));
    }
  }
  return courses;
}

std::optional<domain::Course> Database::getCourseById(int id) {
  auto stmt = prepare(db_, "SELECT id, name, length FROM courses WHERE id=?");
  sqlite3_bind_int(stmt.get(), 1, id);
  if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
    domain::Course c;
    c.id = sqlite3_column_int(stmt.get(), 0);
    c.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
    c.length = optInt(stmt.get(), 2);
    auto cstmt = prepare(db_,
      "SELECT control_id FROM course_controls WHERE course_id=? ORDER BY sort_order");
    sqlite3_bind_int(cstmt.get(), 1, c.id);
    while (sqlite3_step(cstmt.get()) == SQLITE_ROW) {
      c.controls.push_back(sqlite3_column_int(cstmt.get(), 0));
    }
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

// --- Competitions ---

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

}  // namespace meos::db
