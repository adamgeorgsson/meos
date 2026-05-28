#include <gtest/gtest.h>
#include <sqlite3.h>

#include "database.h"

using namespace meos::db;
using namespace meos::domain;

// Apply V1 + V2 migrations to get the full simple-entity schema.
static void applyAllMigrations(Database& db) {
  auto m1 = Database::v1Migrations();
  auto m2 = Database::v2Migrations();
  m1.insert(m1.end(), m2.begin(), m2.end());
  db.applyMigrations(m1);
}

class SimpleEntitiesTest : public ::testing::Test {
 protected:
  Database db_{":memory:"};
  void SetUp() override { applyAllMigrations(db_); }
};

// ---- V2 Migration -----------------------------------------------------------

TEST_F(SimpleEntitiesTest, V2MigrationCreatesControlsTable) {
  // controls table should exist after V1+V2
  auto ctrls = db_.getAllControls();
  EXPECT_TRUE(ctrls.empty());  // no rows yet, but table exists
}

TEST_F(SimpleEntitiesTest, V2MigrationCreatesCoursesTable) {
  auto courses = db_.getAllCourses();
  EXPECT_TRUE(courses.empty());
}

TEST_F(SimpleEntitiesTest, V2SchemaVersionIs2) {
  EXPECT_EQ(db_.schemaVersion(), 2);
}

// ---- Clubs ------------------------------------------------------------------

TEST_F(SimpleEntitiesTest, InsertClub_CanReadBack) {
  Club c{0, "IF Berget", "SE"};
  int id = db_.insertClub(c);
  EXPECT_GT(id, 0);

  auto opt = db_.getClubById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->name, "IF Berget");
  EXPECT_EQ(opt->country, "SE");
}

TEST_F(SimpleEntitiesTest, InsertClub_NullCountry) {
  Club c{0, "Unknown Club", std::nullopt};
  int id = db_.insertClub(c);
  auto opt = db_.getClubById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_FALSE(opt->country.has_value());
}

TEST_F(SimpleEntitiesTest, InsertMultipleClubs_AllReadBack) {
  db_.insertClub({0, "Club A", "SE"});
  db_.insertClub({0, "Club B", "NO"});
  db_.insertClub({0, "Club C", std::nullopt});

  auto all = db_.getAllClubs();
  EXPECT_EQ(all.size(), 3u);
}

TEST_F(SimpleEntitiesTest, UpdateClub_ChangesName) {
  int id = db_.insertClub({0, "Old Name", "SE"});
  Club updated{id, "New Name", "NO"};
  db_.updateClub(updated);

  auto opt = db_.getClubById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->name, "New Name");
  EXPECT_EQ(opt->country, "NO");
}

TEST_F(SimpleEntitiesTest, UpdateClub_ClearsCountry) {
  int id = db_.insertClub({0, "Club", "SE"});
  Club updated{id, "Club", std::nullopt};
  db_.updateClub(updated);

  auto opt = db_.getClubById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_FALSE(opt->country.has_value());
}

TEST_F(SimpleEntitiesTest, DeleteClub_RemovesEntry) {
  int id = db_.insertClub({0, "ToDelete", "SE"});
  ASSERT_TRUE(db_.getClubById(id).has_value());

  db_.deleteClub(id);
  EXPECT_FALSE(db_.getClubById(id).has_value());
}

TEST_F(SimpleEntitiesTest, DeleteClub_DoesNotAffectOthers) {
  int id1 = db_.insertClub({0, "Club1", "SE"});
  int id2 = db_.insertClub({0, "Club2", "NO"});
  db_.deleteClub(id1);

  EXPECT_FALSE(db_.getClubById(id1).has_value());
  EXPECT_TRUE(db_.getClubById(id2).has_value());
}

// ---- Controls ---------------------------------------------------------------

TEST_F(SimpleEntitiesTest, InsertControl_CanReadBack) {
  Control c{0, 101, "Fork junction", "normal"};
  int id = db_.insertControl(c);
  EXPECT_GT(id, 0);

  auto opt = db_.getControlById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->code, 101);
  EXPECT_EQ(opt->description, "Fork junction");
  EXPECT_EQ(opt->type, "normal");
}

TEST_F(SimpleEntitiesTest, InsertControl_NullFields) {
  Control c{0, 200, std::nullopt, std::nullopt};
  int id = db_.insertControl(c);
  auto opt = db_.getControlById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->code, 200);
  EXPECT_FALSE(opt->description.has_value());
  EXPECT_FALSE(opt->type.has_value());
}

TEST_F(SimpleEntitiesTest, InsertMultipleControls_AllReadBack) {
  db_.insertControl({0, 101, "C1", "normal"});
  db_.insertControl({0, 102, "C2", "start"});
  db_.insertControl({0, 900, "Finish", "finish"});

  auto all = db_.getAllControls();
  EXPECT_EQ(all.size(), 3u);
}

TEST_F(SimpleEntitiesTest, UpdateControl_ChangesCode) {
  int id = db_.insertControl({0, 100, "Old", "normal"});
  Control updated{id, 199, "Updated", "special"};
  db_.updateControl(updated);

  auto opt = db_.getControlById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->code, 199);
  EXPECT_EQ(opt->description, "Updated");
  EXPECT_EQ(opt->type, "special");
}

TEST_F(SimpleEntitiesTest, DeleteControl_RemovesEntry) {
  int id = db_.insertControl({0, 101, "ToDelete", "normal"});
  ASSERT_TRUE(db_.getControlById(id).has_value());

  db_.deleteControl(id);
  EXPECT_FALSE(db_.getControlById(id).has_value());
}

TEST_F(SimpleEntitiesTest, DeleteControl_DoesNotAffectOthers) {
  int id1 = db_.insertControl({0, 101, "C1", "normal"});
  int id2 = db_.insertControl({0, 102, "C2", "normal"});
  db_.deleteControl(id1);

  EXPECT_FALSE(db_.getControlById(id1).has_value());
  EXPECT_TRUE(db_.getControlById(id2).has_value());
}

// ---- Courses ----------------------------------------------------------------

TEST_F(SimpleEntitiesTest, InsertCourse_CanReadBack) {
  Course c{0, "Long", 12500, {6, 1, 2, 3, 4, 5, 7}};
  int id = db_.insertCourse(c);
  EXPECT_GT(id, 0);

  auto opt = db_.getCourseById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->name, "Long");
  EXPECT_EQ(opt->length, 12500);
  EXPECT_EQ(opt->controls, (std::vector<int>{6, 1, 2, 3, 4, 5, 7}));
}

TEST_F(SimpleEntitiesTest, InsertCourse_NullLength) {
  Course c{0, "Rogaine", std::nullopt, {}};
  int id = db_.insertCourse(c);
  auto opt = db_.getCourseById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_FALSE(opt->length.has_value());
}

TEST_F(SimpleEntitiesTest, InsertCourse_EmptyControls) {
  Course c{0, "Scratch", 1000, {}};
  int id = db_.insertCourse(c);
  auto opt = db_.getCourseById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_TRUE(opt->controls.empty());
}

TEST_F(SimpleEntitiesTest, InsertCourse_ControlsSemicolonSerialized) {
  // Verify the storage format is semicolon-separated text
  Course c{0, "Short", 4200, {6, 3, 5, 7}};
  int id = db_.insertCourse(c);

  sqlite3_stmt* raw = nullptr;
  sqlite3_prepare_v2(db_.handle(),
    "SELECT controls FROM courses WHERE id=?", -1, &raw, nullptr);
  sqlite3_bind_int(raw, 1, id);
  ASSERT_EQ(sqlite3_step(raw), SQLITE_ROW);
  const char* txt =
      reinterpret_cast<const char*>(sqlite3_column_text(raw, 0));
  EXPECT_STREQ(txt, "6;3;5;7");
  sqlite3_finalize(raw);
}

TEST_F(SimpleEntitiesTest, InsertCourse_DuplicateControlIds) {
  // A course may revisit controls (e.g. Ultra Long)
  Course c{0, "Ultra", 18000, {6, 1, 2, 3, 4, 5, 1, 2, 7}};
  int id = db_.insertCourse(c);
  auto opt = db_.getCourseById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->controls, (std::vector<int>{6, 1, 2, 3, 4, 5, 1, 2, 7}));
}

TEST_F(SimpleEntitiesTest, InsertMultipleCourses_AllReadBack) {
  db_.insertCourse({0, "Long", 12500, {6, 1, 2, 7}});
  db_.insertCourse({0, "Short", 4200, {6, 3, 7}});

  auto all = db_.getAllCourses();
  EXPECT_EQ(all.size(), 2u);
}

TEST_F(SimpleEntitiesTest, UpdateCourse_ChangesNameAndControls) {
  int id = db_.insertCourse({0, "Old", 5000, {1, 2, 3}});
  Course updated{id, "New", 9000, {6, 4, 5, 7}};
  db_.updateCourse(updated);

  auto opt = db_.getCourseById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->name, "New");
  EXPECT_EQ(opt->length, 9000);
  EXPECT_EQ(opt->controls, (std::vector<int>{6, 4, 5, 7}));
}

TEST_F(SimpleEntitiesTest, UpdateCourse_ClearsLength) {
  int id = db_.insertCourse({0, "Course", 5000, {1, 2}});
  Course updated{id, "Course", std::nullopt, {1, 2}};
  db_.updateCourse(updated);

  auto opt = db_.getCourseById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_FALSE(opt->length.has_value());
}

TEST_F(SimpleEntitiesTest, DeleteCourse_RemovesEntry) {
  int id = db_.insertCourse({0, "ToDelete", 1000, {1}});
  ASSERT_TRUE(db_.getCourseById(id).has_value());

  db_.deleteCourse(id);
  EXPECT_FALSE(db_.getCourseById(id).has_value());
}

TEST_F(SimpleEntitiesTest, DeleteCourse_DoesNotAffectOthers) {
  int id1 = db_.insertCourse({0, "C1", 1000, {1}});
  int id2 = db_.insertCourse({0, "C2", 2000, {2}});
  db_.deleteCourse(id1);

  EXPECT_FALSE(db_.getCourseById(id1).has_value());
  EXPECT_TRUE(db_.getCourseById(id2).has_value());
}
