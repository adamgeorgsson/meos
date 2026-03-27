/************************************************************************
    MeOS - Orienteering Software
    Copyright (C) 2009-2026 Melin Software HB

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Melin Software HB - software@melin.nu - www.melin.nu
    Eksoppsvägen 16, SE-75646 UPPSALA, Sverige

************************************************************************/
#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <chrono>
#include <ctime>

// Platform-independent empty string constants
extern const std::wstring _EmptyWString;
extern const std::string  _EmptyString;
extern const std::string  _VacantName;

// Thread-local circular buffer for returning string/wstring references from utility functions.
class StringCache {
private:
  std::vector<std::string>  cache;
  size_t ix = 0;
  std::vector<std::wstring> wcache;
  size_t wix = 0;
public:
  /// Returns the thread-local instance (no Win32 GetCurrentThreadId needed).
  static StringCache &getInstance();

  void init()  { cache.resize(256); wcache.resize(256); }
  void clear() { cache.clear(); wcache.clear(); }

  std::string &get() {
    if (++ix >= cache.size()) ix = 0;
    return cache[ix];
  }

  std::wstring &wget() {
    if (++wix >= wcache.size()) wix = 0;
    return wcache[wix];
  }
};

std::string  convertSystemTimeN(const std::tm &st);
std::string  getLocalTimeN();

bool checkValidDate(const std::wstring &date);

std::wstring convertSystemTime(const std::tm &st);
std::wstring convertSystemTimeOnly(const std::tm &st);
std::wstring convertSystemDate(const std::tm &st);
std::wstring getLocalTime();
std::wstring getLocalDate();
std::wstring getLocalTimeOnly();
/// Returns time in seconds after midnight
int getLocalAbsTime();

/// Get a day number after a fixed day some time ago...
int getRelativeDay();

/// Get time and date in a format that forms a part of a filename
std::wstring getLocalTimeFileName();

enum class SubSecond { Off, On, Auto };

int parseRelativeTime(const char *data);
int parseRelativeTime(const wchar_t *data);

const std::wstring &codeRelativeTimeW(int rt);
const std::string  &codeRelativeTime(int rt);

/// Format time MM:SS.t (force2digit=true) or M:SS.t (force2digit=false)
const std::wstring &formatTimeMS(int m, bool force2digit, SubSecond mode = SubSecond::Auto);
const std::wstring &formatTime(int rt, SubSecond mode = SubSecond::Auto);
const std::wstring &formatTimeHMS(int rt, SubSecond mode = SubSecond::Auto);

std::wstring formatTimeIOF(int rt, int zeroTime);

int convertDateYMD(const std::string &m, bool checkValid);
int convertDateYMD(const std::string &m, std::tm &st, bool checkValid);

int convertDateYMD(const std::wstring &m, bool checkValid);
int convertDateYMD(const std::wstring &m, std::tm &st, bool checkValid);

/// Convert a "general" time string to a MeOS compatible time string
void processGeneralTime(const std::wstring &generalTime, std::wstring &meosTime, std::wstring &meosDate);

/// Format number date 20160421 -> 2016-04-21
std::wstring formatDate(int m, bool useIsoFormat);

int64_t SystemTimeToInt64TenthSecond(const std::tm &st);
std::tm Int64TenthSecondToSystemTime(int64_t time);

#define NOTIME 0x7FFFFFFF

/// Returns a time converted from +/-MM:SS or NOTIME, in MeOS time unit
int convertAbsoluteTimeMS(const std::wstring &m);
int convertAbsoluteTimeMS(const std::string  &m);

/// Parses a time on format HH:MM:SS+01:00Z or HHMMSS+0100Z (ignores time zone)
int convertAbsoluteTimeISO(const std::wstring &m);

/** Returns a time converted from HH:MM:SS or -1, in MeOS time unit
   @param m time to convert
   @param daysZeroTime -1 do not support days syntax, positive interpret days w.r.t the specified zero time.
*/
int convertAbsoluteTimeHMS(const std::string  &m, int daysZeroTime);
int convertAbsoluteTimeHMS(const std::wstring &m, int daysZeroTime);

/// Add or subtract a number of days from a date in Y-M-D format
std::wstring addOrSubtractDays(const std::wstring& m, int days);

const std::vector<std::string>  &split(const std::string  &line, const std::string  &separators, std::vector<std::string>  &split_vector);
const std::vector<std::wstring> &split(const std::wstring &line, const std::wstring &separators, std::vector<std::wstring> &split_vector);

template<typename T>
const T& unsplit(const std::vector<T>& split_vector, const T& separators, T& line);

/// Compare two strings, ignore case. 0 = equal.
int compareStringIgnoreCase(const std::wstring &a, const std::wstring &b);
const std::wstring &limitText(const std::wstring& tIn, size_t numChar);
std::wstring ensureEndingColon(const std::wstring &text);

const std::wstring &makeDash(const std::wstring &t);
const std::wstring &makeDash(const wchar_t *t);

std::wstring formatRank(int rank);
const std::string  &itos(int i);
std::string itos(unsigned int i);
std::string itos(unsigned long i);
std::string itos(int64_t i);
std::string itos(uint64_t i);

const std::wstring &itow(int i);
std::wstring itow(unsigned int i);
std::wstring itow(unsigned long i);
std::wstring itow(int64_t i);
std::wstring itow(uint64_t i);

/// Lower case match (filt_lc must be lower-cased and stripped of accents)
bool filterMatchString(const std::wstring &c, const wchar_t *filt_lc, int &score);

/// To lower case and strip accents
void prepareMatchString(wchar_t* data_c, int size);

bool matchNumber(int number, const wchar_t *key);

int  getMeosBuild();
std::wstring getMeosDate();
std::wstring getMeosFullVersion();
std::wstring getMajorVersion();
std::wstring getMeosCompectVersion();

void getSupporters(std::vector<std::wstring> &supp, std::vector<std::wstring> &developSupp);

int countWords(const wchar_t *p);

std::wstring trim(const std::wstring &s);
std::string  trim(const std::string  &s);

bool fileExists(const std::wstring &file);

bool stringMatch(const std::wstring &a, const std::wstring &b);

const char          *decodeXML(const char *in);
const std::string   &decodeXML(const std::string &in);
const std::string   &encodeXML(const std::string &in);
const std::wstring  &encodeXML(const std::wstring &in);
const std::wstring  &encodeHTML(const std::wstring &in);

/// Extend a year from 03 -> 2003, 97 -> 1997 etc
int extendYear(int year);

/// Get current year, e.g., 2010
int getThisYear();

/// Translate a char to lower/stripped of accents etc.
int toLowerStripped(wchar_t c);

/// Canonize a person/club name
const wchar_t *canonizeName(const wchar_t *name);

/// String distance between 0 and 1. 0 is equal.
double stringDistance(const wchar_t *a, const wchar_t *b);

/// Return how close sample is to target. 1.0 means equal.
double stringDistanceAssymetric(const std::wstring &target, const std::wstring &sample);

/// Get a number suffix, Start 1 -> 1. Zero for none.
int getNumberSuffix(const std::string  &str);
int getNumberSuffix(const std::wstring &str);

/// Extract any number from a string and return the number, prefix and suffix
int extractAnyNumber(const std::wstring &str, std::wstring &prefix, std::wstring &suffix);

/// Compare classnames, match H21 Elit with H21E and H21 E
bool compareClassName(const std::wstring &a, const std::wstring &b);

/// Get error message from an error code (platform-specific)
std::wstring getErrorMessage(int code);

/// HLS (Hue-Lightness-Saturation) color class
class HLS {
private:
  uint16_t HueToRGB(uint16_t n1, uint16_t n2, uint16_t hue) const;
public:
  HLS(uint16_t H, uint16_t L, uint16_t S) : hue(H), lightness(L), saturation(S) {}
  HLS() : hue(0), lightness(0), saturation(1) {}
  short hue;
  short lightness;
  short saturation;
  void lighten(double f);
  void saturate(double s);
  void colorDegree(double d);
  HLS &RGBtoHLS(uint32_t lRGBColor);
  uint32_t HLStoRGB() const;
};

/// Stub: unzip/zip not used in core migration
void unzip(const wchar_t *zipfilename, const char *password, std::vector<std::wstring> &extractedFiles);
int  zip(const wchar_t *zipfilename, const char *password, const std::vector<std::wstring> &files);

bool isAscii(const std::wstring &s);
bool isNumber(const std::wstring &s);

bool isAscii(const std::string &s);
bool isNumber(const std::string &s);

int  convertDynamicBase(const std::wstring &s, long long &out);
void convertDynamicBase(long long val, int base, wchar_t out[16]);

/// Find all files in dir matching given file pattern
bool expandDirectory(const wchar_t *dir, const wchar_t *pattern, std::vector<std::wstring> &res);

enum PersonSex { sFemale = 1, sMale, sBoth, sUnknown };

PersonSex    interpretSex(const std::wstring &sex);
std::wstring encodeSex(PersonSex sex);

std::wstring makeValidFileName(const std::wstring &input, bool strict);
std::string  makeValidFileName(const std::string  &input, bool strict);

/// Initial capital letter.
void capitalize(std::wstring &str);

/// Initial capital letter for each word.
void capitalizeWords(std::wstring &str);

std::wstring getTimeZoneString(const std::wstring &date);

/// Return bias in seconds. UTC = local time + bias.
int getTimeZoneInfo(const std::wstring &date);

/// Compare bib numbers (which may contain non-digits, e.g. A-203, or 301a)
bool compareBib(const std::wstring &b1, const std::wstring &b2);

/// Split a name into Given, Family, and return Given.
std::wstring getGivenName(const std::wstring &name);

/// Split a name into Given, Family, and return Family.
std::wstring getFamilyName(const std::wstring &name);

// ---- Platform-independent file lock -----------------------------------------
#ifdef _WIN32
#include <windows.h>
class MeOSFileLock {
  HANDLE lockedFile;
  MeOSFileLock(const MeOSFileLock &) = delete;
  const MeOSFileLock &operator=(const MeOSFileLock &) = delete;
public:
  MeOSFileLock() : lockedFile(INVALID_HANDLE_VALUE) {}
  ~MeOSFileLock() { unlockFile(); }
  void unlockFile();
  void lockFile(const std::wstring &file);
};
#else
class MeOSFileLock {
  int fd;
  MeOSFileLock(const MeOSFileLock &) = delete;
  const MeOSFileLock &operator=(const MeOSFileLock &) = delete;
public:
  MeOSFileLock() : fd(-1) {}
  ~MeOSFileLock() { unlockFile(); }
  void unlockFile();
  void lockFile(const std::wstring &file);
};
#endif

// ---- Namespace utility variables ---------------------------------------------
namespace MeOSUtil {
  extern int useHourFormat;
}

// ---- String encoding utilities -----------------------------------------------
void string2Wide(const std::string &in, std::wstring &out);
void wide2String(const std::wstring &in, std::string &out);

void checkWriteAccess(const std::wstring &file);
void moveFile(const std::wstring& src, const std::wstring& dst);

/// Widen a string from system codepage (CP1252 on Windows, UTF-8 on Linux)
const std::wstring &widen(const std::string &input);
/// Narrow a wstring to string (lossy ASCII truncation)
const std::string  &narrow(const std::wstring &input);
/// Encode wstring to UTF-8
const std::string  &toUTF8(const std::wstring &input);
/// Decode UTF-8 string to wstring
const std::wstring &fromUTF8(const std::string &input);

// Cross-platform replacement for Win32 _wtoi
inline int wtoi(const wchar_t* s) {
  return s ? static_cast<int>(std::wcstol(s, nullptr, 10)) : 0;
}
inline int wtoi(const std::wstring& s) {
  return static_cast<int>(std::wcstol(s.c_str(), nullptr, 10));
}

/// Returns monotonic milliseconds (replaces GetTickCount64)
inline uint64_t meos_steady_clock_ms() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

/// Fill std::tm with current local time (replaces GetLocalTime)
inline void meos_localtime_now(std::tm* out) {
  auto now = std::chrono::system_clock::now();
  auto tt  = std::chrono::system_clock::to_time_t(now);
#ifdef _WIN32
  localtime_s(out, &tt);
#else
  localtime_r(&tt, out);
#endif
}
