// Unit tests for ClassConfigInfo and QualificationFinal (US-003e2)

#include <gtest/gtest.h>
#include "classconfiginfo.h"
#include "qualification_final.h"
#include "domain_header.h"

// ── ClassConfigInfo tests ─────────────────────────────────────────────────────

TEST(ClassConfigInfoTest, ClearAndEmpty) {
  ClassConfigInfo ci;
  EXPECT_TRUE(ci.empty());
  ci.individual.push_back(1);
  EXPECT_FALSE(ci.empty());
  ci.clear();
  EXPECT_TRUE(ci.empty());
}

TEST(ClassConfigInfoTest, HasPredicates) {
  ClassConfigInfo ci;
  EXPECT_FALSE(ci.hasIndividual());
  EXPECT_FALSE(ci.hasRelay());
  EXPECT_FALSE(ci.hasPatrol());
  EXPECT_FALSE(ci.hasRogaining());
  EXPECT_FALSE(ci.hasTeamClass());
  EXPECT_FALSE(ci.hasQualificationFinal());

  ci.individual.push_back(1);
  EXPECT_TRUE(ci.hasIndividual());
  ci.relay.push_back(2);
  EXPECT_TRUE(ci.hasRelay());
  EXPECT_TRUE(ci.hasTeamClass());
  ci.patrol.push_back(3);
  EXPECT_TRUE(ci.hasPatrol());
  ci.rogainingClasses.push_back(4);
  EXPECT_TRUE(ci.hasRogaining());
  ci.knockout.push_back(5);
  EXPECT_TRUE(ci.hasQualificationFinal());
}

TEST(ClassConfigInfoTest, GetIndividual) {
  ClassConfigInfo ci;
  ci.individual = {1, 2, 3};
  ci.rogainingClasses = {10, 11};

  std::set<int> sel;
  ci.getIndividual(sel, false);
  EXPECT_EQ((std::set<int>{1, 2, 3}), sel);

  sel.clear();
  ci.getIndividual(sel, true);
  EXPECT_EQ((std::set<int>{1, 2, 3, 10, 11}), sel);
}

TEST(ClassConfigInfoTest, GetTeamClass) {
  ClassConfigInfo ci;
  ci.relay = {10};
  ci.patrol = {20};
  std::set<int> sel;
  ci.getTeamClass(sel);
  EXPECT_EQ((std::set<int>{10, 20}), sel);
}

TEST(ClassConfigInfoTest, GetRaceNStart_OutOfBounds) {
  ClassConfigInfo ci;
  std::set<int> sel;
  sel.insert(99);
  ci.getRaceNStart(0, sel);
  EXPECT_TRUE(sel.empty());
}

TEST(ClassConfigInfoTest, GetRaceNStart_InBounds) {
  ClassConfigInfo ci;
  ci.raceNStart = {{1, 2}, {3, 4}};
  std::set<int> sel;
  ci.getRaceNStart(1, sel);
  EXPECT_EQ((std::set<int>{3, 4}), sel);
}

TEST(ClassConfigInfoTest, GetLegNRes) {
  ClassConfigInfo ci;
  ci.legResult[2] = {5, 6, 7};
  std::set<int> sel;
  ci.getLegNRes(2, sel);
  EXPECT_EQ((std::set<int>{5, 6, 7}), sel);

  sel.insert(99);
  ci.getLegNRes(99, sel);
  EXPECT_TRUE(sel.empty());
}

TEST(ClassConfigInfoTest, GetTimeStart) {
  ClassConfigInfo ci;
  ci.timeStart = {{1}, {2, 3}};
  std::set<int> sel;
  ci.getTimeStart(0, sel);
  EXPECT_EQ((std::set<int>{1}), sel);
  sel.clear();
  ci.getTimeStart(1, sel);
  EXPECT_EQ((std::set<int>{2, 3}), sel);
}

TEST(ClassConfigInfoTest, ResultsStarttimesFlags) {
  ClassConfigInfo ci;
  EXPECT_FALSE(ci.hasResults());
  EXPECT_FALSE(ci.hasStartTimes());
  ci.results_ = true;
  ci.starttimes_ = true;
  EXPECT_TRUE(ci.hasResults());
  EXPECT_TRUE(ci.hasStartTimes());
}

// ── QualificationFinal tests ─────────────────────────────────────────────────

TEST(QualificationFinalTest, DefaultConstruct) {
  QualificationFinal qf(1000000, 1);
  EXPECT_EQ(0, qf.getNumClasses());
}

TEST(QualificationFinalTest, ValidNameChar) {
  EXPECT_TRUE(QualificationFinal::isValidNameChar(L'A'));
  EXPECT_TRUE(QualificationFinal::isValidNameChar(L'z'));
  EXPECT_FALSE(QualificationFinal::isValidNameChar(L'|'));
  EXPECT_FALSE(QualificationFinal::isValidNameChar(L'@'));
  EXPECT_FALSE(QualificationFinal::isValidNameChar(L'\0'));
}

TEST(QualificationFinalTest, EncodeDecodeRoundtrip) {
  // Build a simple 2-class 2-level scheme: class 1 qualifies by place to class 2
  std::vector<QFClass> classes(2);
  classes[0].name = L"Kval";
  classes[0].level = 0;
  classes[1].name = L"Final";
  classes[1].level = 1;
  classes[1].qualificationMap.emplace_back(1, 1);  // 1st from class 1 → class 2
  classes[1].qualificationMap.emplace_back(1, 2);

  QualificationFinal qf(1000000, 1);
  qf.setClasses(classes);
  EXPECT_EQ(2, qf.getNumClasses());
  EXPECT_EQ(L"Kval",  qf.getInstanceName(1));
  EXPECT_EQ(L"Final", qf.getInstanceName(2));

  // Encode then re-parse
  std::wstring encoded;
  qf.encode(encoded);
  EXPECT_TRUE(qf.matchSerialization(encoded));

  QualificationFinal qf2(1000000, 1);
  qf2.init(encoded);
  std::wstring encoded2;
  qf2.encode(encoded2);
  EXPECT_EQ(encoded, encoded2);
}

TEST(QualificationFinalTest, GetLevel) {
  // 3 classes: two in level 0, one in level 1
  std::vector<QFClass> classes(3);
  classes[0].level = 0;
  classes[1].level = 0;
  classes[2].level = 1;
  classes[2].qualificationMap.emplace_back(1, 1);
  classes[2].qualificationMap.emplace_back(2, 1);

  QualificationFinal qf(1000000, 1);
  qf.setClasses(classes);

  EXPECT_EQ(0, qf.getLevel(1));
  EXPECT_EQ(0, qf.getLevel(2));
  EXPECT_EQ(1, qf.getLevel(3));
  EXPECT_EQ(2, qf.getNumLevels());
}

TEST(QualificationFinalTest, IsFinalClass) {
  std::vector<QFClass> classes(2);
  classes[0].level = 0;
  classes[1].level = 1;
  classes[1].qualificationMap.emplace_back(1, 1);

  QualificationFinal qf(1000000, 1);
  qf.setClasses(classes);
  EXPECT_FALSE(qf.isFinalClass(1));
  EXPECT_TRUE(qf.isFinalClass(2));
}

TEST(QualificationFinalTest, NoQualification) {
  std::vector<QFClass> classes(2);
  classes[0].level = 0;  // no qualMap → base class
  classes[1].level = 1;
  classes[1].qualificationMap.emplace_back(1, 1);

  QualificationFinal qf(1000000, 1);
  qf.setClasses(classes);
  EXPECT_TRUE(qf.noQualification(0));
  EXPECT_FALSE(qf.noQualification(1));
}

TEST(QualificationFinalTest, GetBaseClassInstances) {
  std::vector<QFClass> classes(3);
  classes[0].level = 0;
  classes[1].level = 0;
  classes[2].level = 1;
  classes[2].qualificationMap.emplace_back(1, 1);

  QualificationFinal qf(1000000, 1);
  qf.setClasses(classes);
  std::set<int> base;
  qf.getBaseClassInstances(base);
  EXPECT_EQ((std::set<int>{1, 2}), base);
}

TEST(QualificationFinalTest, HasRemainingClass) {
  std::vector<QFClass> classes(2);
  classes[0].level = 0;
  classes[1].level = 1;

  QualificationFinal qf(1000000, 1);
  qf.setClasses(classes);
  EXPECT_FALSE(qf.hasRemainingClass());

  classes[1].extraQualification = QFClass::ExtraQualType::All;
  qf.setClasses(classes);
  EXPECT_TRUE(qf.hasRemainingClass());
}

TEST(QualificationFinalTest, QFClassSerialExtra) {
  QFClass c;
  c.extraQualification = QFClass::ExtraQualType::None;
  EXPECT_EQ(L"", c.serialExtra());
  c.extraQualification = QFClass::ExtraQualType::All;
  EXPECT_EQ(L"A;", c.serialExtra());
  c.extraQualification = QFClass::ExtraQualType::NBest;
  c.extraQualData = 3;
  EXPECT_EQ(L"B3;", c.serialExtra());
}

TEST(QualificationFinalTest, InitSimpleParse) {
  // Minimal serialized string (as written by encode)
  // Build via setClasses then parse back
  std::vector<QFClass> classes(2);
  classes[0].name = L"Heat";
  classes[0].level = 0;
  classes[1].name = L"Fin";
  classes[1].level = 1;
  classes[1].qualificationMap.emplace_back(1, 1);
  classes[1].qualificationMap.emplace_back(1, 2);

  QualificationFinal qf(1000000, 1);
  qf.setClasses(classes);
  std::wstring ser;
  qf.encode(ser);

  QualificationFinal qf2(1000000, 1);
  qf2.init(ser);
  EXPECT_EQ(2, qf2.getNumClasses());
  EXPECT_EQ(2, (int)qf2.getInstance(1).qualificationMap.size());
}

TEST(QualificationFinalTest, GetNumStagesEqualsNumLevels) {
  std::vector<QFClass> classes(3);
  classes[0].level = 0;
  classes[1].level = 1;
  classes[1].qualificationMap.emplace_back(1, 1);
  classes[2].level = 2;
  classes[2].qualificationMap.emplace_back(2, 1);

  QualificationFinal qf(1000000, 1);
  qf.setClasses(classes);
  EXPECT_EQ(qf.getNumLevels(), qf.getNumStages());
  EXPECT_EQ(3, qf.getNumStages());
}
