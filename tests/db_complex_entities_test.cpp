#include <gtest/gtest.h>
#include <sqlite3.h>

#include "database.h"

using namespace meos::db;
using namespace meos::domain;

// Apply V1 + V2 + V3 migrations for the full complex-entity schema.
static void applyAllMigrations(Database& db) {
  auto m = Database::v1Migrations();
  auto m2 = Database::v2Migrations();
  auto m3 = Database::v3Migrations();
  m.insert(m.end(), m2.begin(), m2.end());
  m.insert(m.end(), m3.begin(), m3.end());
  db.applyMigrations(m);
}

class ComplexEntitiesTest : public ::testing::Test {
 protected:
  Database db_{":memory:"};
  void SetUp() override { applyAllMigrations(db_); }
};

// ---- V3 Migration -----------------------------------------------------------

TEST_F(ComplexEntitiesTest, V3SchemaVersionIs3) {
  EXPECT_EQ(db_.schemaVersion(), 3);
}

TEST_F(ComplexEntitiesTest, V3MigrationCreatesClassesTable) {
  auto classes = db_.getAllClasses();
  EXPECT_TRUE(classes.empty());
}

TEST_F(ComplexEntitiesTest, V3MigrationCreatesCardsTable) {
  auto cards = db_.getAllCards();
  EXPECT_TRUE(cards.empty());
}

TEST_F(ComplexEntitiesTest, V3MigrationCreatesFreePunchesTable) {
  auto fps = db_.getAllFreePunches();
  EXPECT_TRUE(fps.empty());
}

// ---- Classes ----------------------------------------------------------------

TEST_F(ComplexEntitiesTest, InsertClass_CanReadBack) {
  Class c{0, "H21", std::nullopt, std::nullopt};
  int id = db_.insertClass(c);
  EXPECT_GT(id, 0);

  auto opt = db_.getClassById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->name, "H21");
  EXPECT_FALSE(opt->courseId.has_value());
  EXPECT_FALSE(opt->startMethod.has_value());
}

TEST_F(ComplexEntitiesTest, InsertClass_WithCourseId) {
  // Insert a course first so the FK is satisfied.
  Course course{0, "Lång", 5400, {31, 32, 33}};
  int courseId = db_.insertCourse(course);

  Class c{0, "H21", courseId, "individual"};
  int id = db_.insertClass(c);

  auto opt = db_.getClassById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->courseId, courseId);
  EXPECT_EQ(opt->startMethod, "individual");
}

TEST_F(ComplexEntitiesTest, InsertClass_ZeroCourseIdStoredAsNull) {
  // courseId == 0 means "not assigned" → should be stored as NULL
  Class c{0, "H21", 0, std::nullopt};
  int id = db_.insertClass(c);
  auto opt = db_.getClassById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_FALSE(opt->courseId.has_value());
}

TEST_F(ComplexEntitiesTest, UpdateClass) {
  Class c{0, "H21", std::nullopt, std::nullopt};
  int id = db_.insertClass(c);

  Course course{0, "Kort", 2800, {}};
  int courseId = db_.insertCourse(course);

  Class updated{id, "H21E", courseId, "mass"};
  db_.updateClass(updated);

  auto opt = db_.getClassById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->name, "H21E");
  EXPECT_EQ(opt->courseId, courseId);
  EXPECT_EQ(opt->startMethod, "mass");
}

TEST_F(ComplexEntitiesTest, DeleteClass) {
  Class c{0, "H21", std::nullopt, std::nullopt};
  int id = db_.insertClass(c);
  db_.deleteClass(id);
  EXPECT_FALSE(db_.getClassById(id).has_value());
}

TEST_F(ComplexEntitiesTest, GetAllClasses_ReturnsMultiple) {
  db_.insertClass({0, "H21", std::nullopt, std::nullopt});
  db_.insertClass({0, "D21", std::nullopt, std::nullopt});
  db_.insertClass({0, "H35", std::nullopt, std::nullopt});
  auto classes = db_.getAllClasses();
  EXPECT_EQ(classes.size(), 3u);
}

// ---- Runners ----------------------------------------------------------------

TEST_F(ComplexEntitiesTest, InsertRunner_CanReadBack) {
  Runner r{0, "Anna Svensson", std::nullopt, std::nullopt,
           std::nullopt, std::nullopt, std::nullopt};
  int id = db_.insertRunner(r);
  EXPECT_GT(id, 0);

  auto opt = db_.getRunnerById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->name, "Anna Svensson");
  EXPECT_FALSE(opt->clubId.has_value());
  EXPECT_FALSE(opt->classId.has_value());
}

TEST_F(ComplexEntitiesTest, InsertRunner_WithClubAndClass) {
  Club club{0, "Friskus OK", "SE"};
  int clubId = db_.insertClub(club);

  Class cls{0, "D21", std::nullopt, std::nullopt};
  int classId = db_.insertClass(cls);

  Runner r{0, "Britta Lindqvist", clubId, classId, "10:00", 123456, "OK"};
  int id = db_.insertRunner(r);

  auto opt = db_.getRunnerById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->clubId, clubId);
  EXPECT_EQ(opt->classId, classId);
  EXPECT_EQ(opt->cardNumber, 123456);
  EXPECT_EQ(opt->status, "OK");
}

TEST_F(ComplexEntitiesTest, InsertRunner_ZeroClubIdStoredAsNull) {
  Runner r{0, "Orphan Runner", 0, 0, std::nullopt, std::nullopt, std::nullopt};
  int id = db_.insertRunner(r);
  auto opt = db_.getRunnerById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_FALSE(opt->clubId.has_value());
  EXPECT_FALSE(opt->classId.has_value());
}

TEST_F(ComplexEntitiesTest, UpdateRunner) {
  Runner r{0, "Old Name", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  int id = db_.insertRunner(r);

  Club club{0, "New Club", "SE"};
  int clubId = db_.insertClub(club);

  Runner updated{id, "New Name", clubId, std::nullopt, "11:00", 999, "DNS"};
  db_.updateRunner(updated);

  auto opt = db_.getRunnerById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->name, "New Name");
  EXPECT_EQ(opt->clubId, clubId);
  EXPECT_EQ(opt->status, "DNS");
}

TEST_F(ComplexEntitiesTest, DeleteRunner) {
  Runner r{0, "Temp Runner", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  int id = db_.insertRunner(r);
  db_.deleteRunner(id);
  EXPECT_FALSE(db_.getRunnerById(id).has_value());
}

// ---- Cards ------------------------------------------------------------------

TEST_F(ComplexEntitiesTest, InsertCard_CanReadBack) {
  Card c{0, std::nullopt, 200001, "31:1200;32:2400;33:3600"};
  int id = db_.insertCard(c);
  EXPECT_GT(id, 0);

  auto opt = db_.getCardById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->cardNumber, 200001);
  EXPECT_FALSE(opt->runnerId.has_value());
  EXPECT_EQ(opt->punchString, "31:1200;32:2400;33:3600");
}

TEST_F(ComplexEntitiesTest, InsertCard_WithRunner) {
  Runner r{0, "Erik Johansson", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  int runnerId = db_.insertRunner(r);

  Card c{0, runnerId, 200002, "31:1000"};
  int id = db_.insertCard(c);

  auto opt = db_.getCardById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->runnerId, runnerId);
}

TEST_F(ComplexEntitiesTest, InsertCard_ZeroRunnerIdStoredAsNull) {
  Card c{0, 0, 300001, ""};
  int id = db_.insertCard(c);
  auto opt = db_.getCardById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_FALSE(opt->runnerId.has_value());
}

TEST_F(ComplexEntitiesTest, InsertCard_EmptyPunchString) {
  Card c{0, std::nullopt, 100001, ""};
  int id = db_.insertCard(c);
  auto opt = db_.getCardById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->punchString, "");
}

TEST_F(ComplexEntitiesTest, UpdateCard) {
  Card c{0, std::nullopt, 200003, ""};
  int id = db_.insertCard(c);

  Runner r{0, "Fina Löpare", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  int runnerId = db_.insertRunner(r);

  Card updated{id, runnerId, 200003, "31:500;32:1000;99:1500"};
  db_.updateCard(updated);

  auto opt = db_.getCardById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->runnerId, runnerId);
  EXPECT_EQ(opt->punchString, "31:500;32:1000;99:1500");
}

TEST_F(ComplexEntitiesTest, DeleteCard) {
  Card c{0, std::nullopt, 500001, ""};
  int id = db_.insertCard(c);
  db_.deleteCard(id);
  EXPECT_FALSE(db_.getCardById(id).has_value());
}

TEST_F(ComplexEntitiesTest, GetAllCards_ReturnsMultiple) {
  db_.insertCard({0, std::nullopt, 1, ""});
  db_.insertCard({0, std::nullopt, 2, ""});
  auto cards = db_.getAllCards();
  EXPECT_EQ(cards.size(), 2u);
}

// ---- Free Punches -----------------------------------------------------------

TEST_F(ComplexEntitiesTest, InsertFreePunch_CanReadBack) {
  FreePunch fp{0, 42, 12000, std::nullopt, std::nullopt};
  int id = db_.insertFreePunch(fp);
  EXPECT_GT(id, 0);

  auto opt = db_.getFreePunchById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->code, 42);
  EXPECT_EQ(opt->punchTime, 12000);
  EXPECT_FALSE(opt->runnerId.has_value());
  EXPECT_FALSE(opt->cardNumber.has_value());
}

TEST_F(ComplexEntitiesTest, InsertFreePunch_WithRunner) {
  Runner r{0, "Löpare X", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  int runnerId = db_.insertRunner(r);

  FreePunch fp{0, 55, 8000, runnerId, 200099};
  int id = db_.insertFreePunch(fp);

  auto opt = db_.getFreePunchById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->runnerId, runnerId);
  EXPECT_EQ(opt->cardNumber, 200099);
}

TEST_F(ComplexEntitiesTest, InsertFreePunch_ZeroRunnerIdStoredAsNull) {
  FreePunch fp{0, 10, 5000, 0, std::nullopt};
  int id = db_.insertFreePunch(fp);
  auto opt = db_.getFreePunchById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_FALSE(opt->runnerId.has_value());
}

TEST_F(ComplexEntitiesTest, UpdateFreePunch) {
  FreePunch fp{0, 31, 9000, std::nullopt, std::nullopt};
  int id = db_.insertFreePunch(fp);

  Runner r{0, "Runner Y", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  int runnerId = db_.insertRunner(r);

  FreePunch updated{id, 32, 9500, runnerId, 100000};
  db_.updateFreePunch(updated);

  auto opt = db_.getFreePunchById(id);
  ASSERT_TRUE(opt.has_value());
  EXPECT_EQ(opt->code, 32);
  EXPECT_EQ(opt->punchTime, 9500);
  EXPECT_EQ(opt->runnerId, runnerId);
}

TEST_F(ComplexEntitiesTest, DeleteFreePunch) {
  FreePunch fp{0, 99, 1, std::nullopt, std::nullopt};
  int id = db_.insertFreePunch(fp);
  db_.deleteFreePunch(id);
  EXPECT_FALSE(db_.getFreePunchById(id).has_value());
}

TEST_F(ComplexEntitiesTest, GetAllFreePunches_ReturnsMultiple) {
  db_.insertFreePunch({0, 1, 100, std::nullopt, std::nullopt});
  db_.insertFreePunch({0, 2, 200, std::nullopt, std::nullopt});
  db_.insertFreePunch({0, 3, 300, std::nullopt, std::nullopt});
  auto fps = db_.getAllFreePunches();
  EXPECT_EQ(fps.size(), 3u);
}

// ---- FK enforcement ---------------------------------------------------------

TEST_F(ComplexEntitiesTest, DeleteClub_RejectedWhenRunnerReferencesWith_FK_ON) {
  Club club{0, "FK Test Club", "SE"};
  int clubId = db_.insertClub(club);
  db_.insertRunner({0, "Runner", clubId, std::nullopt, std::nullopt, std::nullopt, std::nullopt});

  // Deleting the club while a runner references it must throw (PRAGMA foreign_keys=ON).
  EXPECT_THROW(db_.deleteClub(clubId), std::runtime_error);
}

TEST_F(ComplexEntitiesTest, DeleteCourse_RejectedWhenClassReferencesWith_FK_ON) {
  Course course{0, "Test Course", 3000, {}};
  int courseId = db_.insertCourse(course);
  db_.insertClass({0, "Test Class", courseId, std::nullopt});

  EXPECT_THROW(db_.deleteCourse(courseId), std::runtime_error);
}

// ---- Dependency order -------------------------------------------------------

TEST_F(ComplexEntitiesTest, LoadOrder_ClubsBeforeRunners) {
  // Verify that clubs can be loaded independently before runners are present.
  Club club{0, "Order Club", "SE"};
  int clubId = db_.insertClub(club);
  Runner r{0, "Order Runner", clubId, std::nullopt, std::nullopt, std::nullopt, std::nullopt};
  db_.insertRunner(r);

  auto clubs = db_.getAllClubs();
  EXPECT_EQ(clubs.size(), 1u);
  auto runners = db_.getAllRunners();
  EXPECT_EQ(runners.size(), 1u);
  EXPECT_EQ(runners[0].clubId, clubId);
}

TEST_F(ComplexEntitiesTest, LoadOrder_CoursesBeforeClasses) {
  Course course{0, "Course A", 4000, {}};
  int courseId = db_.insertCourse(course);
  db_.insertClass({0, "Class A", courseId, "individual"});

  auto courses = db_.getAllCourses();
  EXPECT_EQ(courses.size(), 1u);
  auto classes = db_.getAllClasses();
  EXPECT_EQ(classes.size(), 1u);
  EXPECT_EQ(classes[0].courseId, courseId);
}
