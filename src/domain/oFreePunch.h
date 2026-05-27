#pragma once

#include "oPunch.h"
#include "oControl.h"

class oRunner;
class oFreePunch;
using pFreePunch = oFreePunch*;

class oFreePunch final : public oPunch {
protected:
  int CardNo    = 0;
  int iHashType = 0;  // Index type used for hash-based lookup
  int tRunnerId = 0;  // Id of runner this punch is classified to

  // Comparator to sort free punches by punch time.
  class FreePunchComp {
  public:
    bool operator()(pFreePunch a, pFreePunch b) const {
      return a->getTimeInt() < b->getTimeInt();
    }
  };

  void changedObject() override;

public:
  // Hash a course-control id and race number into a lookup key.
  static int getControlHash(int courseControlId, int race) {
    return courseControlId + race * 100000000;
  }

  // Recover controlId or courseControlId from a hash key.
  static int getControlIdFromHash(int hash, bool courseControlId) {
    int r = hash % 100000000;
    if (courseControlId) return r;
    return oControl::getIdIndexFromCourseControlId(r).first;
  }

  int getControlId() const override { return getControlIdFromHash(iHashType, false); }
  int getCourseControlId() const    { return getControlIdFromHash(iHashType, true); }
  int getIHashType() const          { return iHashType; }

  // Returns the runner currently tied to this punch (nullptr when no match).
  oRunner* getTiedRunner() const;

  void remove();
  bool canRemove() const;

  int  getCardNo() const { return CardNo; }
  bool setCardNo(int cardNo, bool databaseUpdate = false);
  bool setType(const std::wstring& t, bool databaseUpdate = false);
  void setTimeInt(int newTime, bool databaseUpdate) final;

  static void rehashPunches(oEvent& oe, int cardNo, pFreePunch newPunch);
  static bool disableHashing;

  void merge(const oBase& input, const oBase* base) final;

  oFreePunch(oEvent* poe, int card, int time, int type, int unit);
  explicit oFreePunch(oEvent* poe, int id);
  ~oFreePunch() override;

  friend class oEvent;
  friend class oRunner;
};
