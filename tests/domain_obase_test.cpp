#include <gtest/gtest.h>

#include "oBase.h"
#include "oDataContainer.h"
#include "oEvent.h"

// -----------------------------------------------------------------------
// Minimal concrete oBase subclass for testing
// -----------------------------------------------------------------------

class TestEntity : public oBase {
public:
  // DataMap storage — one for current data, one for "old" state
  mutable DataMap dataMap_;
  mutable DataMap oldDataMap_;
  mutable pvectorstr strData_ = nullptr;

  // A static container shared by all TestEntity instances (mirrors legacy pattern)
  static oDataContainer& container() {
    static oDataContainer dc(256);
    static bool initialized = false;
    if (!initialized) {
      dc.addVariableInt("Score", oDataContainer::oIS32, "Score");
      dc.addVariableInt64("ExtId", "ExternalIdentifier");
      dc.addVariableString("Name", 64, "Name");
      initialized = true;
    }
    return dc;
  }

  explicit TestEntity(oEvent* poe) : oBase(poe) {
    // Seed the DataMap with default values
    dataMap_["Score"] = 0;
    dataMap_["ExtId"] = int64_t(0);
    dataMap_["Name"] = std::wstring{};
  }

  // oBase interface implementation
  std::wstring getInfo() const override { return L"TestEntity"; }
  void changedObject() override {}
  void remove() override { Removed = true; }
  bool canRemove() const override { return !isChanged(); }
  void merge(const oBase& /*input*/, const oBase* /*base*/) override {}

  oDataContainer& getDataBuffers(pvoid& data, pvoid& olddata,
                                  pvectorstr& strData) const override {
    data = &dataMap_;
    olddata = &oldDataMap_;
    strData = strData_;
    return container();
  }

  int getDISize() const override { return 0; }
};

// -----------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------

TEST(oBaseTest, DefaultConstruction) {
  oEvent ev;
  TestEntity e(&ev);
  EXPECT_EQ(e.getId(), 0);
  EXPECT_FALSE(e.isChanged());
  EXPECT_FALSE(e.isRemoved());
  EXPECT_EQ(e.getEvent(), &ev);
}

TEST(oBaseTest, UpdateChangedSetsChangedFlag) {
  oEvent ev;
  TestEntity e(&ev);
  EXPECT_FALSE(e.isChanged());
  e.updateChanged();
  EXPECT_TRUE(e.isChanged());
}

TEST(oBaseTest, QuietChangeDoesNotSetChangedFlag) {
  oEvent ev;
  TestEntity e(&ev);
  e.updateChanged(oBase::ChangeType::Quiet);
  EXPECT_FALSE(e.isChanged());
}

TEST(oBaseTest, MakeQuietChangePermanent) {
  oEvent ev;
  TestEntity e(&ev);
  e.updateChanged(oBase::ChangeType::Quiet);
  EXPECT_FALSE(e.isChanged());
  e.makeQuietChangePermanent();
  EXPECT_TRUE(e.isChanged());
}

TEST(oBaseTest, GetSetInt) {
  oEvent ev;
  TestEntity e(&ev);
  e.getDI().setInt("Score", 42);
  EXPECT_EQ(e.getDCI().getInt("Score"), 42);
}

TEST(oBaseTest, GetSetInt64) {
  oEvent ev;
  TestEntity e(&ev);
  e.getDI().setInt64("ExtId", 999999999LL);
  EXPECT_EQ(e.getDCI().getInt64("ExtId"), 999999999LL);
}

TEST(oBaseTest, GetSetString) {
  oEvent ev;
  TestEntity e(&ev);
  e.getDI().setString("Name", L"Alice");
  EXPECT_EQ(e.getDCI().getString("Name"), L"Alice");
}

TEST(oBaseTest, SetIntMarksChanged) {
  oEvent ev;
  TestEntity e(&ev);
  bool changed = e.getDI().setInt("Score", 7);
  EXPECT_TRUE(changed);
  EXPECT_TRUE(e.isChanged());
}

TEST(oBaseTest, SetIntNoOpWhenSameValue) {
  oEvent ev;
  TestEntity e(&ev);
  e.getDI().setInt("Score", 5);
  // Clear changed state by synchronizing
  e.synchronize();
  bool changed = e.getDI().setInt("Score", 5);
  EXPECT_FALSE(changed);
}

TEST(oBaseTest, RemoveWorks) {
  oEvent ev;
  TestEntity e(&ev);
  EXPECT_FALSE(e.isRemoved());
  e.remove();
  EXPECT_TRUE(e.isRemoved());
}

TEST(oBaseTest, ExtIdentifierRoundtrip) {
  oEvent ev;
  TestEntity e(&ev);
  e.setExtIdentifier(int64_t(12345));
  EXPECT_EQ(e.getExtIdentifier(), 12345LL);
}

TEST(oBaseTest, ExtIdentifierStringRoundtrip) {
  oEvent ev;
  TestEntity e(&ev);
  e.setExtIdentifier(std::wstring(L"42"));
  EXPECT_EQ(e.getExtIdentifierString(), L"42");
}

TEST(oBaseTest, CopyConstruction) {
  oEvent ev;
  TestEntity e(&ev);
  e.getDI().setInt("Score", 99);
  TestEntity copy(e);
  // Copy starts with changed=false per legacy semantics
  EXPECT_FALSE(copy.isChanged());
  EXPECT_EQ(copy.getId(), e.getId());
}

TEST(oBaseTest, InfoReturnsValue) {
  oEvent ev;
  TestEntity e(&ev);
  EXPECT_EQ(e.getInfo(), L"TestEntity");
}
