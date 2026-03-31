// domain_test.cpp — Unit tests for oBase and oDataContainer (US-003a).
#include <gtest/gtest.h>
#include <locale>
#include <clocale>

// Access internal state in tests
#define protected public
#define private   public

#include "oDataContainer.h"
#include "oBase.h"
#include "oEvent.h"

using std::string;
using std::wstring;
using std::vector;

// ── Minimal concrete oBase subclass for testing ───────────────────────────────

static constexpr int TestDataSize = 512;

class TestEntity : public oBase {
public:
  uint8_t data[TestDataSize]    = {};
  uint8_t olddata[TestDataSize] = {};
  vector<vector<wstring>> strData;

  static oDataContainer &getDC() {
    static oDataContainer dc(TestDataSize);
    static bool initialized = false;
    if (!initialized) {
      dc.addVariableInt("MyInt",    oDataContainer::oIS32, "My Int");
      dc.addVariableInt("ExtId",    oDataContainer::oIS64, "Ext Id");
      dc.addVariableString("Name",  32, "Name");
      dc.addVariableString("Dyn",   "Dynamic");
      dc.addVariableDate("BirthDate", "Birth Date");
      initialized = true;
    }
    return dc;
  }

  oDataContainer &getDataBuffers(pvoid &d, pvoid &od, pvectorstr &sd) const override {
    d  = const_cast<uint8_t *>(data);
    od = const_cast<uint8_t *>(olddata);
    sd = const_cast<vector<vector<wstring>>*>(&strData);
    return getDC();
  }
  int getDISize() const override { return TestDataSize; }
  wstring getInfo() const override { return L"TestEntity"; }
  void changedObject() override {}
  void merge(const oBase &, const oBase *) override {}
  void remove() override { oBase::remove(); }
  bool canRemove() const override { return true; }

  explicit TestEntity(oEvent *oe) : oBase(oe) {
    getDC().initData(this, TestDataSize);
  }
};

// ── Test fixture ──────────────────────────────────────────────────────────────

class DomainTest : public ::testing::Test {
protected:
  oEvent oe;
  std::unique_ptr<TestEntity> entity;

  void SetUp() override {
    std::setlocale(LC_ALL, "C.UTF-8");
    entity = std::make_unique<TestEntity>(&oe);
  }
};

// ── oBase basics ──────────────────────────────────────────────────────────────

TEST_F(DomainTest, oBase_InitialState) {
  EXPECT_EQ(entity->getId(), 0);
  EXPECT_FALSE(entity->isChanged());
  EXPECT_FALSE(entity->isRemoved());
  EXPECT_EQ(entity->getEvent(), &oe);
  EXPECT_FALSE(entity->isImplicitlyCreated());
  EXPECT_FALSE(entity->isAddedToEvent());
}

TEST_F(DomainTest, oBase_UpdateChanged) {
  EXPECT_FALSE(entity->isChanged());
  entity->updateChanged(oBase::ChangeType::Update);
  EXPECT_TRUE(entity->isChanged());
}

TEST_F(DomainTest, oBase_QuietChangeNotPermanent) {
  entity->updateChanged(oBase::ChangeType::Quiet);
  EXPECT_FALSE(entity->isChanged());  // quiet = transient only
  entity->makeQuietChangePermanent();
  EXPECT_TRUE(entity->isChanged());
}

TEST_F(DomainTest, oBase_CopyConstructor) {
  entity->updateChanged(oBase::ChangeType::Update);
  TestEntity copy(*entity);
  // copy constructor resets changed to false
  EXPECT_FALSE(copy.isChanged());
  EXPECT_EQ(copy.getId(), entity->getId());
  EXPECT_EQ(copy.getEvent(), &oe);
}

TEST_F(DomainTest, oBase_GetReference) {
  auto ref = entity->getReference();
  ASSERT_NE(ref, nullptr);
  EXPECT_EQ(ref->get(), entity.get());
}

TEST_F(DomainTest, oBase_ReferenceNullifiedOnDestroy) {
  std::shared_ptr<oBase::oBaseReference> ref;
  {
    TestEntity e(&oe);
    ref = e.getReference();
    EXPECT_NE(ref->get(), nullptr);
  }
  EXPECT_EQ(ref->get(), nullptr);
}

TEST_F(DomainTest, oBase_idFromExtId_SmallPositive) {
  // values that fit in lower 28 bits are returned as-is
  EXPECT_EQ(oBase::idFromExtId(42), 42);
  EXPECT_EQ(oBase::idFromExtId(0), 0);
}

// ── oDataContainer — integer fields ──────────────────────────────────────────

TEST_F(DomainTest, DataContainer_SetGetInt) {
  auto di = entity->getDI();
  EXPECT_TRUE(di.setInt("MyInt", 42));
  EXPECT_EQ(di.getInt("MyInt"), 42);
}

TEST_F(DomainTest, DataContainer_SetIntNoChangeReturnsFalse) {
  auto di = entity->getDI();
  di.setInt("MyInt", 99);
  EXPECT_FALSE(di.setInt("MyInt", 99));  // same value → not modified
}

TEST_F(DomainTest, DataContainer_SetGetInt64) {
  auto di = entity->getDI();
  int64_t bigVal = 0x1234567890ABCDEFll;
  EXPECT_TRUE(di.setInt64("ExtId", bigVal));
  EXPECT_EQ(di.getInt64("ExtId"), bigVal);
}

// ── oDataContainer — string fields ───────────────────────────────────────────

TEST_F(DomainTest, DataContainer_SetGetFixedString) {
  auto di = entity->getDI();
  EXPECT_TRUE(di.setString("Name", L"Alice"));
  EXPECT_EQ(di.getString("Name"), L"Alice");
}

TEST_F(DomainTest, DataContainer_SetFixedStringSameValueReturnsFalse) {
  auto di = entity->getDI();
  di.setString("Name", L"Bob");
  EXPECT_FALSE(di.setString("Name", L"Bob"));
}

TEST_F(DomainTest, DataContainer_SetGetDynamicString) {
  auto di = entity->getDI();
  EXPECT_TRUE(di.setString("Dyn", L"dynamic value"));
  EXPECT_EQ(di.getString("Dyn"), L"dynamic value");
}

// ── oDataContainer — date field ───────────────────────────────────────────────

TEST_F(DomainTest, DataContainer_SetGetDate) {
  // Store via raw int via setInt (date is stored as YYYYMMDD integer)
  auto di = entity->getDI();
  EXPECT_TRUE(di.setDate("BirthDate", L"1990-06-15"));
  // getDate should return the formatted date
  wstring d = di.getDate("BirthDate");
  EXPECT_EQ(d, L"1990-06-15");
}

// ── oDataContainer — type queries ─────────────────────────────────────────────

TEST_F(DomainTest, DataContainer_IsInt) {
  EXPECT_TRUE(TestEntity::getDC().isInt("MyInt"));
  EXPECT_FALSE(TestEntity::getDC().isInt("Name"));
}

TEST_F(DomainTest, DataContainer_IsString) {
  EXPECT_TRUE(TestEntity::getDC().isString("Name"));
  EXPECT_TRUE(TestEntity::getDC().isString("Dyn"));
  EXPECT_FALSE(TestEntity::getDC().isString("MyInt"));
}

// ── oDataContainer — unknown variable throws ──────────────────────────────────

TEST_F(DomainTest, DataContainer_UnknownVarThrows) {
  auto di = entity->getDI();
  EXPECT_THROW(di.setInt("NoSuchVar", 1), std::runtime_error);
  EXPECT_THROW(di.getInt("NoSuchVar"), std::runtime_error);
}

// ── oDataContainer — SQL generation ──────────────────────────────────────────

TEST_F(DomainTest, DataContainer_GenerateSQLDefinition) {
  string sql = TestEntity::getDC().generateSQLDefinition();
  EXPECT_NE(sql.find("MyInt"), string::npos);
  EXPECT_NE(sql.find("ExtId"), string::npos);
  EXPECT_NE(sql.find("Name"),  string::npos);
}

TEST_F(DomainTest, DataContainer_GenerateSQLSet_ForceAll) {
  auto di = entity->getDI();
  di.setInt("MyInt", 7);
  di.setString("Name", L"Test");
  di.allDataStored();  // mark as "in DB"
  di.setInt("MyInt", 8);  // change one field

  string sql = di.generateSQLSet(false);
  EXPECT_NE(sql.find("MyInt"), string::npos);
}

// ── formatDouble ──────────────────────────────────────────────────────────────

TEST_F(DomainTest, FormatDouble_Integers) {
  EXPECT_EQ(oDataContainer::formatDouble(0.0),  "0");
  EXPECT_EQ(oDataContainer::formatDouble(1.0),  "1");
  EXPECT_EQ(oDataContainer::formatDouble(-3.0), "-3");
}

TEST_F(DomainTest, FormatDouble_Fraction) {
  string s = oDataContainer::formatDouble(3.14);
  EXPECT_NE(s.find("3.14"), string::npos);
}

// ── External identifier encoding ──────────────────────────────────────────────

TEST_F(DomainTest, ExtId_RoundTripDecimal) {
  auto di = entity->getDI();
  di.setInt64("ExtId", 12345);
  wstring s = entity->getExtIdentifierString();
  EXPECT_EQ(s, L"12345");
}

TEST_F(DomainTest, ExtId_ZeroReturnsEmpty) {
  auto di = entity->getDI();
  di.setInt64("ExtId", 0);
  EXPECT_EQ(entity->getExtIdentifierString(), L"");
}

// ── oDataConstInterface (getDCI) — read-only access ───────────────────────────

TEST_F(DomainTest, DataConstInterface_ReadInt) {
  // Write via mutable interface, read back via const interface
  entity->getDI().setInt("MyInt", 55);
  const TestEntity* ce = entity.get();
  auto dci = ce->getDCI();
  EXPECT_EQ(dci.getInt("MyInt"), 55);
}

TEST_F(DomainTest, DataConstInterface_ReadString) {
  // "Name" field is 32 bytes; on Linux 4-byte wchar_t gives max 7 chars.
  entity->getDI().setString("Name", L"Alice");
  const TestEntity* ce = entity.get();
  auto dci = ce->getDCI();
  EXPECT_EQ(dci.getString("Name"), L"Alice");
}

// ── oDataInterface tracks mutations ──────────────────────────────────────────

TEST_F(DomainTest, DataInterface_MutationMarksChanged) {
  EXPECT_FALSE(entity->isChanged());
  entity->getDI().setInt("MyInt", 123);
  EXPECT_TRUE(entity->isChanged());
}

TEST_F(DomainTest, DataInterface_SameValueNoChange) {
  entity->getDI().setInt("MyInt", 77);
  entity->changed = false;  // reset (private, but tests use #define private public)
  // Setting same value should NOT mark changed
  bool modified = entity->getDI().setInt("MyInt", 77);
  EXPECT_FALSE(modified);
  EXPECT_FALSE(entity->isChanged());
}

// ── DataRevisionCache ─────────────────────────────────────────────────────────

TEST_F(DomainTest, DataRevisionCache_NeedsUpdateInitially) {
  DataRevisionCache<wstring> cache;
  EXPECT_TRUE(cache.needsUpdate(oe));
}

TEST_F(DomainTest, DataRevisionCache_UpdateClearsNeedsUpdate) {
  DataRevisionCache<wstring> cache;
  cache.update(oe, wstring(L"hello"));
  EXPECT_FALSE(cache.needsUpdate(oe));
  EXPECT_EQ(cache.get(), L"hello");
}

TEST_F(DomainTest, DataRevisionCache_RevisionChangeTriggersNeedsUpdate) {
  DataRevisionCache<wstring> cache;
  cache.update(oe, wstring(L"v1"));
  oe.dataRevision++;  // simulate domain change
  EXPECT_TRUE(cache.needsUpdate(oe));
}
