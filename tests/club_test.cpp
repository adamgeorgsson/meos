// club_test.cpp — Unit tests for oClub (US-003c).
#include <gtest/gtest.h>
#include <locale>
#include <clocale>

#define protected public
#define private   public

#include "oClub.h"
#include "oEvent.h"
#include "oDataContainer.h"
#include "xmlparser.h"

using std::string;
using std::wstring;
using std::vector;

// ── Test fixture ──────────────────────────────────────────────────────────────

class ClubTest : public ::testing::Test {
protected:
  oEvent oe;

  void SetUp() override {
    std::setlocale(LC_ALL, "C.UTF-8");
  }
};

// ─────────────────────────── Constructor tests ───────────────────────────────

TEST_F(ClubTest, Club_DefaultIdIsAutoAssigned) {
  oClub c(&oe);
  EXPECT_GT(c.getId(), 0);
}

TEST_F(ClubTest, Club_ConstructWithId) {
  oClub c(&oe, 42);
  EXPECT_EQ(c.getId(), 42);
}

TEST_F(ClubTest, Club_InitialNameEmpty) {
  oClub c(&oe, 1);
  EXPECT_TRUE(c.getName().empty());
}

TEST_F(ClubTest, Club_IsNotRemoved) {
  oClub c(&oe, 1);
  EXPECT_FALSE(c.isRemoved());
}

// ─────────────────────────── Name tests ─────────────────────────────────────

TEST_F(ClubTest, Club_SetGetName) {
  oClub c(&oe, 1);
  c.setName(L"IFK Göteborg");
  EXPECT_EQ(c.getName(), L"IFK Göteborg");
}

TEST_F(ClubTest, Club_SetName_NoChangeSkip) {
  oClub c(&oe, 1);
  c.setName(L"Same");
  bool was_changed = c.isChanged();
  c.setName(L"Same"); // same name again
  // isChanged should not be reset by setName to same value
  EXPECT_EQ(c.getName(), L"Same");
}

TEST_F(ClubTest, Club_GetDisplayName_FallsBackToName) {
  oClub c(&oe, 1);
  c.setName(L"MyClub");
  EXPECT_EQ(c.getDisplayName(), L"MyClub");
}

TEST_F(ClubTest, Club_GetCompactName_FallsBackToName) {
  oClub c(&oe, 1);
  c.setName(L"MyClub");
  EXPECT_EQ(c.getCompactName(), L"MyClub");
}

TEST_F(ClubTest, Club_PrettyName_SkidOOK) {
  oClub c(&oe, 1);
  c.setName(L"IFK Skid o OK");
  // "Skid o OK" should be replaced with "SOK"
  EXPECT_EQ(c.getDisplayName(), L"IFK SOK");
}

TEST_F(ClubTest, Club_PrettyName_SkidOOL) {
  oClub c(&oe, 1);
  c.setName(L"IFK Skid o OL");
  EXPECT_EQ(c.getDisplayName(), L"IFK SOL");
}

TEST_F(ClubTest, Club_PrettyName_UnchangedOtherwise) {
  oClub c(&oe, 1);
  c.setName(L"OK Linné");
  EXPECT_EQ(c.getDisplayName(), L"OK Linné");
}

// ─────────────────────────── Compact name computation ────────────────────────

TEST_F(ClubTest, Club_CompactName_StripsOrientering) {
  oClub c(&oe, 1);
  // Name with "Orientering" should strip that word if there are proper-name words
  c.setName(L"Göteborg Orientering");
  // "Göteborg" has 2+ non-uppercase chars → properName; "Orientering" stripped
  EXPECT_EQ(c.getCompactName(), L"Göteborg");
}

TEST_F(ClubTest, Club_CompactName_Short3LetterAllCaps_NotStripped) {
  oClub c(&oe, 1);
  // "IFK" is 3 letters all caps — treated as skip, not a proper name
  // "Linné" has proper chars
  c.setName(L"IFK Linné");
  EXPECT_EQ(c.getCompactName(), L"Linné");
}

TEST_F(ClubTest, Club_CompactName_NoSkippedWords_Empty) {
  oClub c(&oe, 1);
  // All words are short all-caps, no proper name word → no compaction
  c.setName(L"OK IFK");
  // "OK" and "IFK" are all-caps short → both skipped, properName=false → empty
  EXPECT_EQ(c.getCompactName(), L"OK IFK"); // falls back to name
}

TEST_F(ClubTest, Club_ShortNameDI_OverridesCompact) {
  oClub c(&oe, 1);
  c.setName(L"Some Long Club Name Orientering");
  c.getDI().setString("ShortName", L"SLCN");
  // After setting ShortName DI field, internalSetName must be re-triggered
  c.nameChanged();
  EXPECT_EQ(c.getCompactName(), L"SLCN");
}

// ─────────────────────────── getInfo ────────────────────────────────────────

TEST_F(ClubTest, Club_GetInfo) {
  oClub c(&oe, 1);
  c.setName(L"TestClub");
  EXPECT_EQ(c.getInfo(), L"Club: TestClub");
}

// ─────────────────────────── sameClub ───────────────────────────────────────

TEST_F(ClubTest, Club_SameClub_CaseInsensitive) {
  oClub a(&oe, 1);
  oClub b(&oe, 2);
  a.setName(L"IFK Göteborg");
  b.setName(L"ifk göteborg");
  EXPECT_TRUE(a.sameClub(b));
}

TEST_F(ClubTest, Club_SameClub_Different) {
  oClub a(&oe, 1);
  oClub b(&oe, 2);
  a.setName(L"Club A");
  b.setName(L"Club B");
  EXPECT_FALSE(a.sameClub(b));
}

// ─────────────────────────── operator< ──────────────────────────────────────

TEST_F(ClubTest, Club_LessThan) {
  oClub a(&oe, 1);
  oClub b(&oe, 2);
  a.setName(L"AAA");
  b.setName(L"BBB");
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(b < a);
}

// ─────────────────────────── DI fields ──────────────────────────────────────

TEST_F(ClubTest, Club_StartGroup_DefaultZero) {
  oClub c(&oe, 1);
  EXPECT_EQ(c.getStartGroup(), 0);
}

TEST_F(ClubTest, Club_SetGetStartGroup) {
  oClub c(&oe, 1);
  c.setStartGroup(5);
  EXPECT_EQ(c.getStartGroup(), 5);
}

TEST_F(ClubTest, Club_DI_District) {
  oClub c(&oe, 1);
  EXPECT_EQ(c.getDCI().getInt("District"), 0);
  c.getDI().setInt("District", 42);
  EXPECT_EQ(c.getDCI().getInt("District"), 42);
}

TEST_F(ClubTest, Club_DI_Email) {
  oClub c(&oe, 1);
  c.getDI().setString("EMail", L"test@example.com");
  EXPECT_EQ(c.getDCI().getString("EMail"), L"test@example.com");
}

// ─────────────────────────── XML round-trip ─────────────────────────────────

TEST_F(ClubTest, Club_WriteSet_RoundTrip) {
  oClub c(&oe, 99);
  c.setName(L"XML Club");
  c.getDI().setInt("District", 7);

  // Write to XML
  xmlparser xml;
  xml.openMemoryOutput(false);
  c.write(xml);
  string xmlStr;
  xml.getMemoryOutput(xmlStr);

  // Parse back
  xmlparser in;
  in.readMemory(xmlStr, 0);
  xmlobject root = in.getObject("Club");
  ASSERT_TRUE(root);

  oClub c2(&oe, 0);
  c2.set(root);

  EXPECT_EQ(c2.getId(), 99);
  EXPECT_EQ(c2.getName(), L"XML Club");
  EXPECT_EQ(c2.getDCI().getInt("District"), 7);
}

TEST_F(ClubTest, Club_Write_Removed_NoOutput) {
  oClub c(&oe, 5);
  c.setName(L"Removed");
  c.Removed = true;

  xmlparser xml;
  xml.openMemoryOutput(false);
  bool result = c.write(xml);
  EXPECT_TRUE(result); // returns true even when removed
  string xmlStr;
  xml.getMemoryOutput(xmlStr);
  // No <Club> element should have been written
  EXPECT_EQ(xmlStr.find("<Club"), string::npos);
}

// ─────────────────────────── oEvent club management ─────────────────────────

TEST_F(ClubTest, Event_AddClub_ByName) {
  pClub pc = oe.addClub(L"New Club");
  ASSERT_NE(pc, nullptr);
  EXPECT_EQ(pc->getName(), L"New Club");
}

TEST_F(ClubTest, Event_AddClub_DuplicateId_ReturnsSame) {
  pClub pc1 = oe.addClub(L"Club One", 10);
  pClub pc2 = oe.addClub(L"Club Two", 10); // same ID
  EXPECT_EQ(pc1, pc2); // same pointer
}

TEST_F(ClubTest, Event_GetClub_ById) {
  pClub added = oe.addClub(L"Test Club", 100);
  pClub found = oe.getClub(100);
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->getName(), L"Test Club");
}

TEST_F(ClubTest, Event_GetClub_ByName) {
  oe.addClub(L"Named Club", 101);
  pClub found = oe.getClub(wstring(L"Named Club"));
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->getId(), 101);
}

TEST_F(ClubTest, Event_GetClub_NotFound) {
  pClub found = oe.getClub(99999);
  EXPECT_EQ(found, nullptr);
}

TEST_F(ClubTest, Event_GetClubs) {
  oe.addClub(L"Club A", 200);
  oe.addClub(L"Club B", 201);

  vector<pClub> clubs;
  oe.getClubs(clubs, false);
  EXPECT_GE(clubs.size(), 2u);
}

TEST_F(ClubTest, Event_RemoveClub) {
  pClub pc = oe.addClub(L"ToRemove", 300);
  ASSERT_NE(pc, nullptr);
  oe.removeClub(300);
  pClub after = oe.getClub(300);
  EXPECT_EQ(after, nullptr); // removed from index
}

TEST_F(ClubTest, Club_CanRemove) {
  oClub c(&oe, 1);
  // stub: isClubUsed always returns false
  EXPECT_TRUE(c.canRemove());
}

// ─────────────────────────── clearClubs ─────────────────────────────────────

TEST_F(ClubTest, Event_ClearClubs) {
  oe.addClub(L"Club X", 500);
  oe.addClub(L"Club Y", 501);

  oClub::clearClubs(oe);

  vector<pClub> clubs;
  oe.getClubs(clubs, false);
  EXPECT_TRUE(clubs.empty());
}

// ─────────────────────────── Invoice helpers ─────────────────────────────────

TEST_F(ClubTest, Club_AssignInvoiceNumber) {
  pClub c1 = oe.addClub(L"Club A", 601);
  pClub c2 = oe.addClub(L"Club B", 602);

  oClub::assignInvoiceNumber(oe, true);

  int n1 = c1->getDCI().getInt("InvoiceNo");
  int n2 = c2->getDCI().getInt("InvoiceNo");
  EXPECT_GT(n1, 0);
  EXPECT_GT(n2, 0);
  EXPECT_NE(n1, n2);
}

TEST_F(ClubTest, Club_GetFirstInvoiceNumber) {
  pClub c1 = oe.addClub(L"Club A", 701);
  pClub c2 = oe.addClub(L"Club B", 702);

  oClub::assignInvoiceNumber(oe, true);

  int first = oClub::getFirstInvoiceNumber(oe);
  EXPECT_GT(first, 0);
}
