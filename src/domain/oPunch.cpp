// oPunch.cpp — Migrated from legacy code/oPunch.cpp (US-003b).
// Cross-platform, no Win32 dependencies.

#include "oPunch.h"
#include "oEvent.h"
#include "oCourse.h"
#include "oControl.h"
#include "../util/meos_util.h"
#include "../util/localizer.h"
#include "../util/timeconstants.hpp"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

oPunch::oPunch(oEvent* poe) : oBase(poe) {
  type = 0;
  punchTime = 0;
  isUsed = false;
  hasBeenPlayed = false;
  tMatchControlId = -1;
  tRogainingIndex = 0;
  anyRogainingMatchControlId = -1;
  tIndex = -1;
  previousPunchTime = 0;
  tRogainingPoints = 0;
  tTimeAdjust = {0, 0};
}

oPunch::~oPunch() = default;

wstring oPunch::getInfo() const
{
  return L"Stämpling " + widen(codeString());
}

string oPunch::codeString() const
{
  char bf[32];
  snprintf(bf, 32, "%d-%d;", type, punchTime);
  return bf;
}

void oPunch::appendCodeString(string& dst) const {
  char ubf[16];
  if (punchUnit > 0)
    snprintf(ubf, sizeof(ubf), "@%d", punchUnit);
  else
    ubf[0] = 0;

  char ubo[16];
  if (origin != 0)
    snprintf(ubo, sizeof(ubo), "#%d", origin);
  else
    ubo[0] = 0;

  char bf[48];
  if (timeConstSecond == 10 && punchTime != -1) {
    if (punchTime >= 0)
      snprintf(bf, 32, "%d-%d.%d%s%s;", type, punchTime / timeConstSecond,
               punchTime % timeConstSecond, ubf, ubo);
    else {
      snprintf(bf, 32, "%d--%d.%d%s%s;", type, (-punchTime) / timeConstSecond,
               (-punchTime) % timeConstSecond, ubf, ubo);
    }
  }
  else if (timeConstSecond == 100 && punchTime != -1) {
    if (punchTime >= 0)
      snprintf(bf, 32, "%d-%d.%02d%s%s;", type, punchTime / timeConstSecond,
               punchTime % timeConstSecond, ubf, ubo);
    else {
      snprintf(bf, 32, "%d--%d.%02d%s%s;", type, (-punchTime) / timeConstSecond,
               (-punchTime) % timeConstSecond, ubf, ubo);
    }
  }
  else
    snprintf(bf, 32, "%d-%d%s%s;", type, punchTime, ubf, ubo);

  dst.append(bf);
}

void oPunch::decodeString(const char* s) {
  const char* typeS = s;
  while (*s >= '0' && *s <= '9')
    ++s;

  type = atoi(typeS);

  if (*s == '-') {
    ++s;
    const char* timeS = s;
    while ((*s >= '0' && *s <= '9') || *s == '-')
      ++s;

    int t = atoi(timeS);
    if (timeConstSecond > 1 && *s == '.') {
      ++s;
      int tenth = *s - '0';
      int hundredth = -1;

      if (timeConstSecond > 10)
        tenth *= 10;

      while ((*s >= '0' && *s <= '9')) { // Eat more decimal digits (unused)
        ++s;
        if (hundredth == -1) {
          hundredth = *s - '0';
          if (timeConstSecond >= 100 && hundredth >= 0 && hundredth < 10)
            tenth += hundredth;
        }
      }

      if (tenth > 0 && tenth < timeConstSecond) {
        if (t >= 0 && *timeS != '-')
          punchTime = timeConstSecond * t + tenth;
        else
          punchTime = timeConstSecond * t - tenth;
      }
      else
        punchTime = timeConstSecond * t;
    }
    else if (t == -1)
      punchTime = t;
    else
      punchTime = timeConstSecond * t;
  }
  else
    punchTime = 0;

  if (*s == '@') {
    ++s;
    punchUnit = atoi(s);
  }
  else {
    punchUnit = 0;
  }

  while (*s && *s != '#' && *s != ';') {
    s++;
  }

  if (*s == '#') {
    ++s;
    origin = atoi(s);
  }
  else {
    origin = 0;
  }
}

wstring oPunch::getString() const {
  wchar_t bf[32];

  const wchar_t* ct;
  wstring time(getTime(false, SubSecond::Auto));
  if (!isOriginal() && origin != 0) {
    if (time.length() == 1)
      time = L"\u270E";
    else
      time = L"\u270E" + time;
  }
  ct = time.c_str();

  wstring typeS = getType(nullptr);
  const wchar_t* tp = typeS.c_str();

  if (type == oPunch::PunchStart || type == oPunch::PunchCheck || type == oPunch::PunchFinish) {
    swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"%s\t%s", tp, ct);
  }
  else {
    if (isUsed)
      swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"%d\t%s", type, ct);
    else
      swprintf(bf, sizeof(bf) / sizeof(wchar_t), L"  %d*\t%s", type, ct);
  }

  return bf;
}

wstring oPunch::getSimpleString() const
{
  wstring time(getTime(false, SubSecond::Auto));

  if (type == oPunch::PunchStart)
    return lang.tl(L"starten (X)#" + time);
  else if (type == oPunch::PunchFinish)
    return lang.tl(L"målet (X)#" + time);
  else if (type == oPunch::PunchCheck)
    return lang.tl(L"check (X)#" + time);
  else
    return lang.tl(L"kontroll X (Y)#" + itow(type) + L"#" + time);
}

wstring oPunch::getTime(bool adjust, SubSecond mode) const
{
  if (punchTime >= 0) {
    int at = getTimeInt();
    if (adjust)
      at += tTimeAdjust.second;
    if (at > 0)
      return oe->getAbsTime(at, mode);
  }
  return makeDash(L"-");
}

int oPunch::getTimeInt() const {
  if (punchUnit > 0)
    return punchTime + oe->getUnitAdjustment(type, punchUnit);
  return punchTime + tTimeAdjust.first; // Adjustment "wrong time" at control
}

int oPunch::getAdjustedTime() const
{
  if (punchTime >= 0)
    return getTimeInt() + tTimeAdjust.second;
  else return -1;
}

void oPunch::setTime(const wstring& t)
{
  if (convertAbsoluteTimeHMS(t, -1) <= 0) {
    setTimeInt(-1, false);
    return;
  }

  int tt = oe->getRelativeTime(t) - tTimeAdjust.first;
  if (tt < 0)
    tt = 0;
  else if (punchUnit > 0) {
    tt -= oe->getUnitAdjustment(type, punchUnit);
  }
  setTimeInt(tt, false);
}

void oPunch::setTimeInt(int tt, bool databaseUpdate) {
  if (tt != punchTime) {
    if (origin == 0)
      origin = -1; // Manual change
    punchTime = tt;
    if (!databaseUpdate)
      updateChanged();
  }
}

void oPunch::setPunchUnit(int unit) {
  if (unit != punchUnit) {
    punchUnit = unit;
    updateChanged();
  }
}

oDataContainer& oPunch::getDataBuffers(pvoid& data, pvoid& olddata, pvectorstr& strData) const {
  throw std::runtime_error("Unsupported: oPunch has no oDataContainer buffer");
}

wstring oPunch::getRunningTime(int startTime) const
{
  int t = getAdjustedTime();
  if (startTime > 0 && t > 0 && t > startTime)
    return formatTime(t - startTime);
  else
    return makeDash(L"-");
}

void oPunch::remove()
{
  // Not implemented
}

bool oPunch::canRemove() const
{
  return true;
}

const wstring& oPunch::getType(const oCourse* crs) const {
  return getType(type, crs);
}

const wstring& oPunch::getType(int t, const oCourse* crs) {
  if (t == oPunch::PunchStart || (crs && t == crs->getStartPunchType()))
    return lang.tl("Start");
  else if (t == oPunch::PunchFinish || (crs && t == crs->getFinishPunchType()))
    return lang.tl(L"Mål");
  else if (t == oPunch::PunchCheck)
    return lang.tl("Check");
  else if (t > 10 && t < 10000) {
    return itow(t);
  }
  return _EmptyWString;
}

void oPunch::changedObject() {
  // Does nothing for oPunch
}

namespace {
  constexpr uint64_t origin_key = 1300602071;
}

int oPunch::computeOrigin(int time, int code) {
  if (time <= 0 || code <= 0)
    return 0;
  static_assert(timeConstHour == 36000);
  time = time % (36000 * 24 * 7);
  code = code % 29;
  uint64_t xcode = (static_cast<uint64_t>(time) * 29 + code) * 7;
  assert(xcode > 0 && xcode < 1300000000);
  return static_cast<int>((xcode * 53458ul) % origin_key);
}

bool oPunch::isOriginal() const {
  if (origin <= 0)
    return false;
  return computeOrigin(oe->getZeroTimeNum() + punchTime, type) == origin;
}

int oPunch::getOriginalTime() const {
  if (origin <= 0)
    return 0;

  constexpr uint64_t inv = 91551603;
  int xcode = static_cast<int>((inv * static_cast<uint64_t>(origin)) % origin_key);
  if (xcode % 7 != 0)
    return 0;

  int pt = ((xcode / 7) - type % 29) / 29;

  if (punchUnit > 0)
    return pt + oe->getUnitAdjustment(type, punchUnit);
  return pt + tTimeAdjust.first; // Adjustment "wrong time" at control
}

const oControl* oPunch::getRogainingControl(const oCourse& crs) const {
  return crs.getControl(tRogainingIndex);
}

void oPunch::merge(const oBase& /*input*/, const oBase* /*base*/) {
  // Merge logic deferred to US-003i (full oEvent integration)
}
