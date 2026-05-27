#include <gtest/gtest.h>
#include "oEvent.h"

// -----------------------------------------------------------------------
// getZeroTimeNum — returns ZeroTime member (not hardcoded 0)
// -----------------------------------------------------------------------

TEST(oEvent, GetZeroTimeNum_Default) {
  oEvent oe;
  // Default constructed — ZeroTime == 0.
  EXPECT_EQ(0, oe.getZeroTimeNum());
}

TEST(oEvent, GetZeroTimeNum_AfterSet) {
  oEvent oe;
  oe.ZeroTime = 36000; // 10 hours in tenths-of-second
  EXPECT_EQ(36000, oe.getZeroTimeNum());
}

// -----------------------------------------------------------------------
// newCompetition — clears all collections and resets indices
// -----------------------------------------------------------------------

TEST(oEvent, NewCompetition_SetsName) {
  oEvent oe;
  oe.newCompetition(L"Test Race");
  EXPECT_EQ(L"Test Race", oe.Name);
}

TEST(oEvent, NewCompetition_ClearsRunners) {
  oEvent oe;
  oe.addRunner(1);
  oe.addRunner(2);
  EXPECT_EQ(2u, oe.Runners.size());

  oe.newCompetition(L"Fresh");
  EXPECT_TRUE(oe.Runners.empty());
}

TEST(oEvent, NewCompetition_ClearsControls) {
  oEvent oe;
  oe.getControl(10, true);
  oe.getControl(20, true);
  EXPECT_EQ(2u, oe.Controls.size());

  oe.newCompetition(L"Fresh");
  EXPECT_TRUE(oe.Controls.empty());
  EXPECT_TRUE(oe.controlIndex_.empty());
}

TEST(oEvent, NewCompetition_ClearsCourses) {
  oEvent oe;
  oe.addCourse(1);
  EXPECT_EQ(1u, oe.Courses.size());
  oe.newCompetition(L"Fresh");
  EXPECT_TRUE(oe.Courses.empty());
}

TEST(oEvent, NewCompetition_ClearsClasses) {
  oEvent oe;
  oe.addClass(1);
  EXPECT_EQ(1u, oe.Classes.size());
  oe.newCompetition(L"Fresh");
  EXPECT_TRUE(oe.Classes.empty());
}

TEST(oEvent, NewCompetition_ClearsClubs) {
  oEvent oe;
  oe.addClub(1);
  EXPECT_EQ(1u, oe.Clubs.size());
  oe.newCompetition(L"Fresh");
  EXPECT_TRUE(oe.Clubs.empty());
}

TEST(oEvent, NewCompetition_ClearsTeams) {
  oEvent oe;
  oe.addTeam(1);
  EXPECT_EQ(1u, oe.Teams.size());
  oe.newCompetition(L"Fresh");
  EXPECT_TRUE(oe.Teams.empty());
}

TEST(oEvent, NewCompetition_ClearsCards) {
  oEvent oe;
  oe.addCard(1);
  EXPECT_EQ(1u, oe.Cards.size());
  oe.newCompetition(L"Fresh");
  EXPECT_TRUE(oe.Cards.empty());
}

TEST(oEvent, NewCompetition_ResetsFreeIdCounters) {
  oEvent oe;
  oe.addRunner();
  oe.addRunner();
  oe.newCompetition(L"Fresh");
  // After reset, next allocated runner should get id 1 again.
  oRunner* r = oe.addRunner();
  EXPECT_EQ(1, r->getId());
}

// -----------------------------------------------------------------------
// getControlByNumber
// -----------------------------------------------------------------------

TEST(oEvent, GetControlByNumber_FindsExisting) {
  oEvent oe;
  // getControl(id, create=true) initialises Numbers[0] = id.
  oControl* c = oe.getControl(31, true);
  ASSERT_NE(nullptr, c);

  oControl* found = oe.getControlByNumber(31);
  EXPECT_EQ(c, found);
}

TEST(oEvent, GetControlByNumber_ReturnsNullForMissing) {
  oEvent oe;
  oe.getControl(31, true);
  EXPECT_EQ(nullptr, oe.getControlByNumber(99));
}

TEST(oEvent, GetControlByNumber_MultipleControls) {
  oEvent oe;
  oe.getControl(100, true);
  oe.getControl(200, true);
  oe.getControl(300, true);

  EXPECT_NE(nullptr, oe.getControlByNumber(100));
  EXPECT_NE(nullptr, oe.getControlByNumber(200));
  EXPECT_NE(nullptr, oe.getControlByNumber(300));
  EXPECT_EQ(nullptr, oe.getControlByNumber(150));
}

// -----------------------------------------------------------------------
// addRunner / getRunnerByCardNo
// -----------------------------------------------------------------------

TEST(oEvent, AddRunner_AppearsInCollection) {
  oEvent oe;
  oRunner* r = oe.addRunner(42);
  ASSERT_NE(nullptr, r);
  EXPECT_EQ(42, r->getId());
  EXPECT_EQ(1u, oe.Runners.size());
}

TEST(oEvent, GetRunnerByCardNo_LinearScanFallback) {
  oEvent oe;
  oRunner* r = oe.addRunner(1);
  r->setCardNo(12345, false);

  oRunner* found = oe.getRunnerByCardNo(12345, 0, oEvent::CardLookupProperty::Any);
  EXPECT_EQ(r, found);
}

TEST(oEvent, GetRunnerByCardNo_ReturnsNullWhenNotFound) {
  oEvent oe;
  oe.addRunner(1);

  EXPECT_EQ(nullptr, oe.getRunnerByCardNo(99999, 0, oEvent::CardLookupProperty::Any));
}

// -----------------------------------------------------------------------
// addControl / getControl
// -----------------------------------------------------------------------

TEST(oEvent, GetControl_Create_ReturnsStablePointer) {
  oEvent oe;
  oControl* c1 = oe.getControl(55, true);
  oControl* c2 = oe.getControl(55, false);
  EXPECT_EQ(c1, c2); // same pointer from index
}

TEST(oEvent, GetControl_NoCreate_ReturnsNullWhenMissing) {
  oEvent oe;
  EXPECT_EQ(nullptr, oe.getControl(999, false));
}

// -----------------------------------------------------------------------
// addCourse / getCourseById
// -----------------------------------------------------------------------

TEST(oEvent, AddCourse_LookupById) {
  oEvent oe;
  oCourse* c = oe.addCourse(7);
  EXPECT_EQ(c, oe.getCourseById(7));
  EXPECT_EQ(nullptr, oe.getCourseById(99));
}

// -----------------------------------------------------------------------
// addClass / getClass
// -----------------------------------------------------------------------

TEST(oEvent, AddClass_LookupById) {
  oEvent oe;
  oClass* cl = oe.addClass(3);
  EXPECT_EQ(cl, oe.getClass(3));
  EXPECT_EQ(nullptr, oe.getClass(99));
}

// -----------------------------------------------------------------------
// addClub / getClub
// -----------------------------------------------------------------------

TEST(oEvent, AddClub_LookupById) {
  oEvent oe;
  oClub* c = oe.addClub(5);
  EXPECT_EQ(c, oe.getClub(5));
  EXPECT_EQ(nullptr, oe.getClub(99));
}

// -----------------------------------------------------------------------
// addCard / getCard
// -----------------------------------------------------------------------

TEST(oEvent, AddCard_LookupById) {
  oEvent oe;
  oCard* card = oe.addCard(10);
  EXPECT_EQ(card, oe.getCard(10));
  EXPECT_EQ(nullptr, oe.getCard(99));
}
