#pragma once

#include "oBase.h"
#include "oDataContainer.h"
#include "domain_header.h"

class oEvent;
class oCourse;
class oControl;

class oPunch : public oBase {
public:
  bool isUsed = false;   // Is used in the course
  int  tIndex = -1;      // Control match index in course
  int  tMatchControlId = -1;

  enum SpecialPunch {
    PunchUnused = 0,
    PunchStart  = 1,
    PunchFinish = 2,
    PunchCheck  = 3,
    HiredCard   = 11111
  };

protected:
  int type      = 0;
  int punchTime = 0;
  int punchUnit = 0;
  int origin    = 0;

  int tRogainingIndex         = 0;
  int anyRogainingMatchControlId = -1;
  int tRogainingPoints        = 0;

  // Time adjustments: first = fixed (wrong-time at control), second = dynamic
  pair<int, int> tTimeAdjust = {0, 0};

  int  tCardIndex    = -1;
  bool hasBeenPlayed = false;

  mutable int previousPunchTime = 0;

  oDataContainer& getDataBuffers(pvoid& data, pvoid& olddata,
                                  pvectorstr& strData) const override;
  int getDISize() const final { return 0; }

  void changedObject() override;

public:
  const oControl* getRogainingControl(const oCourse& crs) const;

  int  getPunchUnit() const { return punchUnit; }
  void setPunchUnit(int unit);

  virtual int getControlId() const { return tMatchControlId; }

  bool isUsedInCourse() const { return isUsed; }
  void remove()  override { Removed = true; }
  bool canRemove() const override { return true; }

  static int  computeOrigin(int time, int code);
  bool        isOriginal() const;
  int         getOriginalTime() const;

  std::wstring getInfo() const override;

  bool isHiredCard() const { return type == HiredCard; }
  bool isStart()     const { return type == PunchStart; }
  bool isStart(int startType) const { return type == PunchStart || type == startType; }
  bool isFinish()    const { return type == PunchFinish; }
  bool isFinish(int finishType) const { return type == PunchFinish || type == finishType; }
  bool isCheck()     const { return type == PunchCheck; }
  int  getControlNumber() const { return type >= 30 ? type : 0; }

  const wstring& getType(const oCourse* crs) const;
  static const wstring& getType(int type, const oCourse* crs);

  int          getTypeCode() const { return type; }
  wstring      getString() const;
  wstring      getSimpleString() const;

  wstring getTime(bool adjusted, SubSecond mode) const;

  // Time after punch-unit adjustment (not including control adjustments)
  int  getTimeInt() const;

  bool hasTime() const { return punchTime > 0; }

  // Time after unit AND control/course adjustments
  int  getAdjustedTime() const;

  void setTime(const wstring& t);
  virtual void setTimeInt(int newTime, bool databaseUpdate);

  void clearTimeAdjust()    { tTimeAdjust = {0, 0}; }
  void setTimeAdjust(int t) { tTimeAdjust.first  = t; }
  void adjustTimeAdjust(int t) { tTimeAdjust.second += t; }

  wstring getRunningTime(int startTime) const;

  void        decodeString(const char* s);
  string      codeString() const;
  void        appendCodeString(string& dst) const;

  void merge(const oBase& input, const oBase* base) override;

  explicit oPunch(oEvent* poe);
  virtual ~oPunch();

  friend class oCard;
  friend class oRunner;
  friend class oTeam;
  friend class oEvent;

private:
  mutable DataMap dataMap_;
  mutable DataMap oldDataMap_;
};

using pPunch = oPunch*;
