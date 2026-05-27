#include <gtest/gtest.h>

#include "database.h"
#include "seed.h"

using namespace meos::db;
using namespace meos::domain;

class SeedTest : public ::testing::Test {
 protected:
  void SetUp() override {
    db_ = std::make_unique<Database>(":memory:");
    db_->createTables();
  }
  std::unique_ptr<Database> db_;
};

TEST_F(SeedTest, SeedsAllEntities) {
  seedIfEmpty(*db_);

  auto comps = db_->getAllCompetitions();
  ASSERT_EQ(comps.size(), 1u);
  EXPECT_EQ(comps[0].name, "Spring Cup 2026");
  EXPECT_EQ(comps[0].date, "2026-05-15");
  EXPECT_EQ(comps[0].organizer, "IF Berget");

  auto clubs = db_->getAllClubs();
  ASSERT_EQ(clubs.size(), 5u);
  EXPECT_EQ(clubs[0].name, "IF Berget");
  EXPECT_EQ(clubs[0].country.value(), "SE");

  auto controls = db_->getAllControls();
  ASSERT_EQ(controls.size(), 7u);

  auto courses = db_->getAllCourses();
  ASSERT_EQ(courses.size(), 5u);
  // Course 1 "Long" should have controls [6, 1, 2, 3, 4, 5, 7]
  EXPECT_EQ(courses[0].name, "Long");
  EXPECT_EQ(courses[0].controls, (std::vector<int>{6, 1, 2, 3, 4, 5, 7}));

  auto classes = db_->getAllClasses();
  ASSERT_EQ(classes.size(), 5u);
  EXPECT_EQ(classes[0].name, "H21E");

  auto runners = db_->getAllRunners();
  ASSERT_EQ(runners.size(), 6u);
  EXPECT_EQ(runners[0].name, "Anna Lindstr\xc3\xb6m");
  EXPECT_EQ(runners[0].cardNumber.value(), 2001234);

  auto teams = db_->getAllTeams();
  ASSERT_EQ(teams.size(), 5u);
  EXPECT_EQ(teams[0].name, "Berget Red");
  EXPECT_EQ(teams[0].members, (std::vector<int>{2}));

  auto results = db_->getAllResults();
  ASSERT_EQ(results.size(), 6u);
  EXPECT_EQ(results[0].totalTime.value(), 4512);
  ASSERT_EQ(results[0].splits.size(), 2u);
  EXPECT_EQ(results[0].splits[0].controlId, 1);
  EXPECT_EQ(results[0].splits[0].time, 1234);
  // DNS result should have no position/totalTime
  EXPECT_FALSE(results[3].position.has_value());
  EXPECT_FALSE(results[3].totalTime.has_value());

  auto startList = db_->getAllStartList();
  ASSERT_EQ(startList.size(), 6u);
  EXPECT_EQ(startList[0].startTime, "10:00:00");
  EXPECT_EQ(startList[0].bib.value(), 1);
}

TEST_F(SeedTest, SkipsWhenDataExists) {
  seedIfEmpty(*db_);
  // Seed again — should not throw or duplicate data
  seedIfEmpty(*db_);
  auto clubs = db_->getAllClubs();
  EXPECT_EQ(clubs.size(), 5u);
}
