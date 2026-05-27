#include "seed.h"

#include <sqlite3.h>
#include <stdexcept>

namespace meos::db {

namespace {

void exec(sqlite3* db, const char* sql) {
  char* errMsg = nullptr;
  int rc = sqlite3_exec(db, sql, nullptr, nullptr, &errMsg);
  if (rc != SQLITE_OK) {
    std::string err = errMsg ? errMsg : "unknown error";
    sqlite3_free(errMsg);
    throw std::runtime_error("Seed SQL error: " + err);
  }
}

bool isEmpty(sqlite3* db) {
  sqlite3_stmt* stmt = nullptr;
  sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM competitions", -1, &stmt, nullptr);
  int count = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    count = sqlite3_column_int(stmt, 0);
  }
  sqlite3_finalize(stmt);
  return count == 0;
}

}  // namespace

void seedIfEmpty(Database& db) {
  sqlite3* handle = db.handle();
  if (!isEmpty(handle)) {
    return;
  }

  exec(handle, "BEGIN TRANSACTION");

  // Competition
  exec(handle, R"(
    INSERT INTO competitions (id, name, date, organizer, location, description) VALUES
    (1, 'Spring Cup 2026', '2026-05-15', 'IF Berget', 'Björnparken, Stockholm', 'Annual spring orienteering competition')
  )");

  // Clubs
  exec(handle, R"(
    INSERT INTO clubs (id, name, country) VALUES
    (1, 'IF Berget', 'SE'),
    (2, 'OK Älgen', 'SE'),
    (3, 'IFK Göteborg SOK', 'SE'),
    (4, 'Sundbybergs IK', 'SE'),
    (5, 'Tullinge SK', 'SE')
  )");

  // Controls
  exec(handle, R"(
    INSERT INTO controls (id, code, description, type) VALUES
    (1, 101, 'Fork junction', 'normal'),
    (2, 102, 'Stone wall corner', 'normal'),
    (3, 103, 'Hill top', 'normal'),
    (4, 104, 'Stream crossing', 'normal'),
    (5, 105, 'Marsh edge', 'normal'),
    (6, 31, 'Start triangle', 'start'),
    (7, 900, 'Finish', 'finish')
  )");

  // Courses
  exec(handle, R"(
    INSERT INTO courses (id, name, length) VALUES
    (1, 'Long', 12500),
    (2, 'Medium', 7800),
    (3, 'Short', 4200),
    (4, 'Very Short', 2100),
    (5, 'Ultra Long', 18000)
  )");

  // Course controls (ordered)
  // Course 1 "Long": [6, 1, 2, 3, 4, 5, 7]
  exec(handle, R"(
    INSERT INTO course_controls (course_id, control_id, sort_order) VALUES
    (1, 6, 0), (1, 1, 1), (1, 2, 2), (1, 3, 3), (1, 4, 4), (1, 5, 5), (1, 7, 6)
  )");
  // Course 2 "Medium": [6, 2, 3, 5, 7]
  exec(handle, R"(
    INSERT INTO course_controls (course_id, control_id, sort_order) VALUES
    (2, 6, 0), (2, 2, 1), (2, 3, 2), (2, 5, 3), (2, 7, 4)
  )");
  // Course 3 "Short": [6, 3, 5, 7]
  exec(handle, R"(
    INSERT INTO course_controls (course_id, control_id, sort_order) VALUES
    (3, 6, 0), (3, 3, 1), (3, 5, 2), (3, 7, 3)
  )");
  // Course 4 "Very Short": [6, 5, 7]
  exec(handle, R"(
    INSERT INTO course_controls (course_id, control_id, sort_order) VALUES
    (4, 6, 0), (4, 5, 1), (4, 7, 2)
  )");
  // Course 5 "Ultra Long": [6, 1, 2, 3, 4, 5, 1, 2, 7]
  exec(handle, R"(
    INSERT INTO course_controls (course_id, control_id, sort_order) VALUES
    (5, 6, 0), (5, 1, 1), (5, 2, 2), (5, 3, 3), (5, 4, 4), (5, 5, 5), (5, 1, 6), (5, 2, 7), (5, 7, 8)
  )");

  // Classes
  exec(handle, R"(
    INSERT INTO classes (id, name, course_id, start_method) VALUES
    (1, 'H21E', 1, 'individual'),
    (2, 'D21E', 1, 'individual'),
    (3, 'H21A', 2, 'individual'),
    (4, 'D21A', 2, 'individual'),
    (5, 'H35', 3, 'individual')
  )");

  // Runners
  exec(handle, R"(
    INSERT INTO runners (id, name, club_id, class_id, start_time, card_number, status) VALUES
    (1, 'Anna Lindström', 1, 2, '10:00:00', 2001234, 'ok'),
    (2, 'Erik Johansson', 2, 1, '10:02:00', 2001235, 'ok'),
    (3, 'Maria Karlsson', 1, 2, '10:04:00', 2001236, 'ok'),
    (4, 'Lars Nilsson', 3, 1, '10:06:00', 2001237, 'dns'),
    (5, 'Maja Björk', 4, 3, '10:08:00', 2001238, 'ok'),
    (6, 'Sven Ek', 5, 3, '10:10:00', 2001239, 'dnf')
  )");

  // Teams
  exec(handle, R"(
    INSERT INTO teams (id, name, club_id, class_id) VALUES
    (1, 'Berget Red', 1, 1),
    (2, 'Älgen Elite', 2, 1),
    (3, 'Göteborg A', 3, 1),
    (4, 'Sundbyberg 1', 4, 3),
    (5, 'Tullinge Masters', 5, 3)
  )");

  // Team members
  exec(handle, R"(
    INSERT INTO team_members (team_id, runner_id) VALUES
    (1, 2),
    (2, 2),
    (3, 4),
    (4, 5),
    (5, 6)
  )");

  // Results
  exec(handle, R"(
    INSERT INTO results (id, runner_id, class_id, position, total_time, status) VALUES
    (1, 1, 2, 1, 4512, 'ok'),
    (2, 2, 1, 1, 5823, 'ok'),
    (3, 3, 2, 2, 4789, 'ok'),
    (4, 4, 1, NULL, NULL, 'dns'),
    (5, 5, 3, 1, 3123, 'ok'),
    (6, 6, 3, NULL, NULL, 'dnf')
  )");

  // Splits
  exec(handle, R"(
    INSERT INTO splits (result_id, control_id, time) VALUES
    (1, 1, 1234),
    (1, 2, 2456),
    (2, 1, 1500),
    (2, 2, 3200),
    (3, 1, 1300),
    (3, 2, 2700),
    (5, 3, 900),
    (6, 3, 850)
  )");

  // Start list
  exec(handle, R"(
    INSERT INTO start_list (id, runner_id, class_id, start_time, bib) VALUES
    (1, 1, 2, '10:00:00', 1),
    (2, 2, 1, '10:02:00', 2),
    (3, 3, 2, '10:04:00', 3),
    (4, 4, 1, '10:06:00', 4),
    (5, 5, 3, '10:08:00', 5),
    (6, 6, 3, '10:10:00', 6)
  )");

  exec(handle, "COMMIT");
}

}  // namespace meos::db
