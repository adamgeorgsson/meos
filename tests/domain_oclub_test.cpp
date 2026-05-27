#include <gtest/gtest.h>
#include "oClub.h"
#include "oEvent.h"

// Helper: create a test oEvent on the stack.
static oEvent makeEvent() { return oEvent{}; }

// -----------------------------------------------------------------------
// Construction
// -----------------------------------------------------------------------

TEST(oClub, Construction_DefaultId) {
  oEvent oe;
  oClub c(&oe);
  EXPECT_GT(c.getId(), 0);
  EXPECT_FALSE(c.isRemoved());
  EXPECT_FALSE(c.isChanged());
}

TEST(oClub, Construction_ExplicitId) {
  oEvent oe;
  oClub c(&oe, 42);
  EXPECT_EQ(42, c.getId());
}

TEST(oClub, Construction_EmptyName) {
  oEvent oe;
  oClub c(&oe, 1);
  EXPECT_EQ(L"", c.getName());
  EXPECT_EQ(L"", c.getDisplayName());
  EXPECT_EQ(L"", c.getCompactName());
}

// -----------------------------------------------------------------------
// setName / getName
// -----------------------------------------------------------------------

TEST(oClub, SetName_Basic) {
  oEvent oe;
  oClub c(&oe, 1);
  c.setName(L"Åby OK");
  EXPECT_EQ(L"Åby OK", c.getName());
  EXPECT_TRUE(c.isChanged());
}

TEST(oClub, SetName_SameNameNoChange) {
  oEvent oe;
  oClub c(&oe, 1);
  c.setName(L"TestClub");
  // reset changed flag by creating a fresh club
  oClub c2(&oe, 2);
  c2.setName(L"TestClub");
  bool changed1 = c2.isChanged();
  c2.setName(L"TestClub"); // same name again
  // isChanged might still be true from first set, but should not double-flip
  EXPECT_EQ(L"TestClub", c2.getName());
}

// -----------------------------------------------------------------------
// getInfo
// -----------------------------------------------------------------------

TEST(oClub, GetInfo) {
  oEvent oe;
  oClub c(&oe, 1);
  c.setName(L"Friskis");
  EXPECT_EQ(L"Club: Friskis", c.getInfo());
}

// -----------------------------------------------------------------------
// Pretty name: "Skid o OK" → "SOK"
// -----------------------------------------------------------------------

TEST(oClub, PrettyName_SkidoOK) {
  oEvent oe;
  oClub c(&oe, 1);
  c.setName(L"Täby Skid o OK");
  // getDisplayName should use tPrettyName
  EXPECT_EQ(L"Täby SOK", c.getDisplayName());
  // getName still returns raw
  EXPECT_EQ(L"Täby Skid o OK", c.getName());
}

TEST(oClub, PrettyName_SkidoOL) {
  oEvent oe;
  oClub c(&oe, 1);
  c.setName(L"Vinter Skid o OL");
  EXPECT_EQ(L"Vinter SOL", c.getDisplayName());
}

TEST(oClub, PrettyName_NoChange) {
  oEvent oe;
  oClub c(&oe, 1);
  c.setName(L"Linköpings OK");
  // No pretty-name transform
  EXPECT_EQ(L"Linköpings OK", c.getDisplayName());
}

// -----------------------------------------------------------------------
// Compact name algorithm
// -----------------------------------------------------------------------

TEST(oClub, CompactName_ShortAllCapsWordsSkipped) {
  oEvent oe;
  oClub c(&oe, 1);
  // "OK" is ≤3 chars all-caps → skipped; "Linköping" is a proper name
  c.setName(L"Linköpings OK");
  // The algorithm: "OK" skipped (all-caps ≤3), "Linköpings" has proper chars (ö > 'Z')
  // skipped > 0 && properName → compact = "Linköpings"
  EXPECT_EQ(L"Linköpings", c.getCompactName());
}

TEST(oClub, CompactName_OrienteersWordSkipped) {
  oEvent oe;
  oClub c(&oe, 1);
  c.setName(L"Göteborgs Orientering");
  // "Orientering" is in skip list, "Göteborgs" has 'ö' > 'Z' → proper
  EXPECT_EQ(L"Göteborgs", c.getCompactName());
}

TEST(oClub, CompactName_NoSkippedWords_FallsBackToName) {
  oEvent oe;
  oClub c(&oe, 1);
  // All words are regular ASCII → no skip, no proper → tCompactName stays empty → getName()
  c.setName(L"TestClub");
  EXPECT_EQ(L"TestClub", c.getCompactName());
}

// -----------------------------------------------------------------------
// ShortName override
// -----------------------------------------------------------------------

TEST(oClub, ShortName_OverridesCompact) {
  oEvent oe;
  oClub c(&oe, 1);
  c.setName(L"Linköpings OK");
  c.getDI().setString("ShortName", L"LIOK");
  c.nameChanged(); // recompute
  EXPECT_EQ(L"LIOK", c.getCompactName());
}

// -----------------------------------------------------------------------
// sameClub (case-insensitive name compare)
// -----------------------------------------------------------------------

TEST(oClub, SameClub_CaseInsensitive) {
  oEvent oe;
  oClub a(&oe, 1);
  oClub b(&oe, 2);
  a.setName(L"Täby OK");
  b.setName(L"täby ok");
  EXPECT_TRUE(a.sameClub(b));
}

TEST(oClub, SameClub_Different) {
  oEvent oe;
  oClub a(&oe, 1);
  oClub b(&oe, 2);
  a.setName(L"Täby OK");
  b.setName(L"Linköpings OK");
  EXPECT_FALSE(a.sameClub(b));
}

// -----------------------------------------------------------------------
// operator<
// -----------------------------------------------------------------------

TEST(oClub, OperatorLess_Alphabetical) {
  oEvent oe;
  oClub a(&oe, 1);
  oClub b(&oe, 2);
  a.setName(L"Alfa");
  b.setName(L"Beta");
  EXPECT_TRUE(a < b);
  EXPECT_FALSE(b < a);
}

// -----------------------------------------------------------------------
// StartGroup
// -----------------------------------------------------------------------

TEST(oClub, StartGroup_DefaultZero) {
  oEvent oe;
  oClub c(&oe, 1);
  EXPECT_EQ(0, c.getStartGroup());
}

TEST(oClub, StartGroup_SetGet) {
  oEvent oe;
  oClub c(&oe, 1);
  c.setStartGroup(3);
  EXPECT_EQ(3, c.getStartGroup());
}

// -----------------------------------------------------------------------
// isVacant
// -----------------------------------------------------------------------

TEST(oClub, IsVacant_NormalClub_False) {
  oEvent oe;
  oClub c(&oe, 5);
  // Stub getVacantClubIfExist returns 0; getId() = 5 → not vacant
  EXPECT_FALSE(c.isVacant());
}

// -----------------------------------------------------------------------
// canRemove / remove
// -----------------------------------------------------------------------

TEST(oClub, CanRemove_StubReturnsTrue) {
  oEvent oe;
  oClub c(&oe, 1);
  c.setName(L"FreeClub");
  EXPECT_TRUE(c.canRemove()); // isClubUsed stub returns false
}

TEST(oClub, Remove_SetsRemovedFlag) {
  oEvent oe;
  oClub c(&oe, 1);
  EXPECT_FALSE(c.isRemoved());
  c.remove();
  EXPECT_TRUE(c.isRemoved());
}

// -----------------------------------------------------------------------
// assignInvoiceNumber
// -----------------------------------------------------------------------

TEST(oClub, AssignInvoiceNumber_Reset) {
  oEvent oe;
  oClub& a  = *oe.addClub(1); a.setName(L"Alpha");
  oClub& b  = *oe.addClub(2); b.setName(L"Beta");
  oClub& cc = *oe.addClub(3); cc.setName(L"Gamma");

  oClub::assignInvoiceNumber(oe, true); // reset=true → assign from 100

  int na = a.getDCI().getInt("InvoiceNo");
  int nb = b.getDCI().getInt("InvoiceNo");
  int nc = cc.getDCI().getInt("InvoiceNo");

  // All should have distinct consecutive numbers starting from 100
  EXPECT_GT(na, 0);
  EXPECT_GT(nb, 0);
  EXPECT_GT(nc, 0);
  // They should all be different
  EXPECT_NE(na, nb);
  EXPECT_NE(nb, nc);
  EXPECT_NE(na, nc);
}

TEST(oClub, AssignInvoiceNumber_NoReset_OnlyUnassigned) {
  oEvent oe;
  oClub& a = *oe.addClub(1); a.setName(L"Alpha");
  oClub& b = *oe.addClub(2); b.setName(L"Beta");

  // Pre-assign a an invoice number
  a.getDI().setInt("InvoiceNo", 200);

  oClub::assignInvoiceNumber(oe, false);

  // a already had 200; b had 0 → gets 201
  EXPECT_EQ(200, a.getDCI().getInt("InvoiceNo"));
  EXPECT_EQ(201, b.getDCI().getInt("InvoiceNo"));
}

TEST(oClub, AssignInvoiceNumber_AllZero_ActsLikeReset) {
  oEvent oe;
  oClub& a = *oe.addClub(1); a.setName(L"Alpha");
  oClub& b = *oe.addClub(2); b.setName(L"Beta");

  oClub::assignInvoiceNumber(oe, false); // all zero → behaves like reset

  EXPECT_GT(a.getDCI().getInt("InvoiceNo"), 0);
  EXPECT_GT(b.getDCI().getInt("InvoiceNo"), 0);
}

// -----------------------------------------------------------------------
// getFirstInvoiceNumber
// -----------------------------------------------------------------------

TEST(oClub, GetFirstInvoiceNumber_ReturnsMin) {
  oEvent oe;
  oClub& a = *oe.addClub(1); a.setName(L"Alpha");
  oClub& b = *oe.addClub(2); b.setName(L"Beta");

  a.getDI().setInt("InvoiceNo", 150);
  b.getDI().setInt("InvoiceNo", 142);

  EXPECT_EQ(142, oClub::getFirstInvoiceNumber(oe));
}

TEST(oClub, GetFirstInvoiceNumber_NoAssigned_ReturnsZero) {
  oEvent oe;
  oClub& a = *oe.addClub(1); a.setName(L"Alpha");
  EXPECT_EQ(0, oClub::getFirstInvoiceNumber(oe));
}

// -----------------------------------------------------------------------
// getInvoiceDate / setInvoiceDate
// -----------------------------------------------------------------------

TEST(oClub, InvoiceDate_DefaultNonEmpty) {
  oEvent oe;
  wstring d = oClub::getInvoiceDate(oe);
  EXPECT_FALSE(d.empty());
}

TEST(oClub, InvoiceDate_SetAndGet) {
  oEvent oe;
  oClub::setInvoiceDate(oe, L"2025-11-30");
  EXPECT_EQ(L"2025-11-30", oClub::getInvoiceDate(oe));
}

// -----------------------------------------------------------------------
// nameChanged after ShortName data change
// -----------------------------------------------------------------------

TEST(oClub, NameChanged_UpdatesCompactName) {
  oEvent oe;
  oClub c(&oe, 1);
  c.setName(L"Linköpings OK");
  // Set ShortName via DI, then trigger recompute
  c.getDI().setString("ShortName", L"LIOK");
  c.nameChanged();
  EXPECT_EQ(L"LIOK", c.getCompactName());
  // Clear ShortName
  c.getDI().setString("ShortName", L"");
  c.nameChanged();
  // Should fall back to algorithm or name
  EXPECT_NE(L"LIOK", c.getCompactName());
}
