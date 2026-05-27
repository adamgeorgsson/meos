#pragma once
#include "oBase.h"
#include "oDataContainer.h"
#include "oPunch.h"

class oRunner;
class oCourse;
class oControl;
struct SICard;
class RunnerResultTestAccessor;

using oPunchList = std::list<oPunch>;

class oCard : public oBase {
  friend class RunnerResultTestAccessor;
protected:
  oPunchList   punches;
  int          cardNo     = 0;
  int          miliVolt   = 0;  // Measured SIAC voltage (0 = not present)
  int          batteryDate = 0; // Battery replace date (YYYYMMDD)
  unsigned int readId     = 0;  // Hash identifying a specific read-out

  static const int ConstructedFromPunches = 1;

  oRunner* tOwner = nullptr;

  oPunch* getPunch(const oPunch* punch);

  mutable std::string punchString;

  oDataContainer& getDataBuffers(pvoid& data, pvoid& olddata,
                                  pvectorstr& strData) const override;
  int getDISize() const final { return 0; }
  void changedObject() override;

public:
  enum class PunchOrigin {
    Unknown,
    Original,
    Manual,
  };

  enum class BatteryStatus {
    OK,
    Warning,
    Bad,
  };

  int getStartTime(int ptype) const;
  int getStartPunchCode() const;
  int getFinishPunchCode() const;

  void setMeasuredVoltage(int mv) { miliVolt = mv; }
  std::wstring getCardVoltage() const;
  static std::wstring getCardVoltage(int mv);

  BatteryStatus isCriticalCardVoltage() const;
  static BatteryStatus isCriticalCardVoltage(int mv);

  std::wstring getBatteryDate() const;

  /** Returns true if the order of punches is unexpected (wrt time). */
  bool unexpectedOrder(int startTime) const;

  bool isConstructedFromPunches() const { return (int)readId == ConstructedFromPunches; }

  void  remove() override;
  bool  canRemove() const override;

  std::pair<int, int> getTimeRange() const;
  std::wstring getInfo() const override;

  int getSplitTime(int startTime, const oPunch* punch) const;

  oRunner* getOwner() const;
  int getNumPunches() const { return (int)punches.size(); }

  int getNumControlPunches(int startPunchType, int finishPunchType) const;

  bool setPunchTime(const oPunch* punch, const std::wstring& time);

  bool isCardRead(const SICard& card) const;
  void setReadId(const SICard& card);
  void getSICard(SICard& card) const;

  void deletePunch(oPunch* pp);
  void insertPunchAfter(int pos, int type, int time);

  /** Get a (candidate) incorrect punch for the specified missing control. */
  std::pair<int, oControl*> getWrongPunch(const oCourse& crs, const oControl& ctrl);

  static int getUnmatchedPunchId(int punchIx)   { return -(100 + punchIx); }
  static int getUnmatchedPunchIndex(int punchId) { return -punchId - 100; }

  PunchOrigin isOriginalCard() const;

  void    addPunch(int type, int time, int matchControlId, int unit,
                   PunchOrigin origin);
  oPunch* getPunchByType(int type) const;
  oPunch* getPunchById(int courseControlId) const;
  oPunch* getPunchByIndex(int ix) const;

  void getPunches(std::vector<oPunch*>& out) const;
  std::wstring getRogainingSplit(int ix, int startTime) const;

  void adaptTimes(int startTime);

  int getCardNo() const { return cardNo; }
  const std::wstring& getCardNoString() const;
  void setCardNo(int c);

  void importPunches(const std::string& s);
  const std::string& getPunchString() const;

  void merge(const oBase& input, const oBase* base) final;
  std::pair<int, int> getCardHash() const;

  oCard(oEvent* poe);
  oCard(oEvent* poe, int id);
  ~oCard() override;

  friend class oEvent;
  friend class oRunner;

private:
  mutable DataMap dataMap_;
  mutable DataMap oldDataMap_;
};

using pCard = oCard*;

