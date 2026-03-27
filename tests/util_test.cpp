#include <gtest/gtest.h>
#include "meos_util.h"
#include "timeconstants.hpp"
#include <ctime>
#include <string>
#include <vector>

using std::string;
using std::wstring;
using std::vector;

// ---- String encoding --------------------------------------------------------
TEST(StringConversion, toUTF8_fromUTF8_roundtrip) {
  wstring ws = L"Hello, \u00C5\u00E4\u00F6"; // Swedish chars
  const string &utf8 = toUTF8(ws);
  const wstring &back = fromUTF8(utf8);
  EXPECT_EQ(ws, back);
}

TEST(StringConversion, toUTF8_ascii) {
  wstring ws = L"MeOS";
  const string &utf8 = toUTF8(ws);
  EXPECT_EQ(utf8, "MeOS");
}

TEST(StringConversion, fromUTF8_ascii) {
  const wstring &ws = fromUTF8("MeOS");
  EXPECT_EQ(ws, L"MeOS");
}

TEST(StringConversion, widen_narrow_ascii) {
  const wstring &ws = widen("Hello");
  EXPECT_EQ(ws, L"Hello");
  const string &ns = narrow(L"World");
  EXPECT_EQ(ns, "World");
}

TEST(StringConversion, string2Wide_empty) {
  wstring out;
  string2Wide("", out);
  EXPECT_TRUE(out.empty());
}

TEST(StringConversion, string2Wide_ascii) {
  wstring out;
  string2Wide("abc", out);
  EXPECT_EQ(out, L"abc");
}

TEST(StringConversion, wide2String_ascii) {
  string out;
  wide2String(L"abc", out);
  EXPECT_EQ(out, "abc");
}

// ---- Time formatting --------------------------------------------------------
TEST(TimeFormat, formatTime_minutes_seconds) {
  // 90 seconds = 1:30 (150 tenths-of-second)
  int t = 1 * timeConstMinute + 30 * timeConstSecond;
  const wstring &s = formatTime(t);
  EXPECT_EQ(s, L"1:30");
}

TEST(TimeFormat, formatTime_zero_gives_dash) {
  const wstring &s = formatTime(0);
  EXPECT_EQ(s[0], (wchar_t)0x2013);
}

TEST(TimeFormat, formatTimeHMS_basic) {
  int t = 2 * timeConstHour + 3 * timeConstMinute + 4 * timeConstSecond;
  const wstring &s = formatTimeHMS(t);
  EXPECT_EQ(s, L"02:03:04");
}

TEST(TimeFormat, codeRelativeTime_full_seconds) {
  // 10 tenths of a second = 1 second
  const string &s = codeRelativeTime(10);
  EXPECT_EQ(s, "1");
}

// ---- Time parsing -----------------------------------------------------------
TEST(TimeParse, convertAbsoluteTimeHMS_basic) {
  int t = convertAbsoluteTimeHMS("01:02:03", -1);
  EXPECT_EQ(t, 1*timeConstHour + 2*timeConstMinute + 3*timeConstSecond);
}

TEST(TimeParse, convertAbsoluteTimeHMS_negative) {
  int t = convertAbsoluteTimeHMS("-1:00:00", -1);
  EXPECT_EQ(t, -1);
}

TEST(TimeParse, convertAbsoluteTimeMS_plus) {
  int t = convertAbsoluteTimeMS("+05:30");
  EXPECT_EQ(t, 5*timeConstMinute + 30*timeConstSecond);
}

TEST(TimeParse, convertAbsoluteTimeMS_minus) {
  int t = convertAbsoluteTimeMS("-02:00");
  EXPECT_EQ(t, -(2*timeConstMinute));
}

TEST(TimeParse, convertAbsoluteTimeMS_notime) {
  int t = convertAbsoluteTimeMS("");
  EXPECT_EQ(t, NOTIME);
}

// ---- Date parsing -----------------------------------------------------------
TEST(DateParse, convertDateYMD_iso) {
  int d = convertDateYMD(string("2023-06-15"), false);
  EXPECT_EQ(d, 20230615);
}

TEST(DateParse, convertDateYMD_compact) {
  int d = convertDateYMD(string("20231225"), false);
  EXPECT_EQ(d, 20231225);
}

TEST(DateParse, convertDateYMD_wstring) {
  int d = convertDateYMD(wstring(L"2024-01-01"), false);
  EXPECT_EQ(d, 20240101);
}

TEST(DateParse, formatDate_basic) {
  wstring s = formatDate(20231225, true);
  EXPECT_EQ(s, L"2023-12-25");
}

TEST(DateParse, formatDate_invalid) {
  wstring s = formatDate(0, true);
  EXPECT_EQ(s, L"-");
}

// ---- String utilities -------------------------------------------------------
TEST(StringUtil, trim_leading_trailing) {
  EXPECT_EQ(trim(string("  hello  ")), "hello");
  EXPECT_EQ(trim(wstring(L"  world  ")), L"world");
}

TEST(StringUtil, trim_empty) {
  EXPECT_EQ(trim(string("")), "");
  EXPECT_EQ(trim(wstring(L"")), L"");
}

TEST(StringUtil, split_basic) {
  vector<string> sv;
  split(string("a,b,c"), string(","), sv);
  ASSERT_EQ(sv.size(), 3u);
  EXPECT_EQ(sv[0], "a"); EXPECT_EQ(sv[1], "b"); EXPECT_EQ(sv[2], "c");
}

TEST(StringUtil, split_wstring) {
  vector<wstring> sv;
  split(wstring(L"x;y;z"), wstring(L";"), sv);
  ASSERT_EQ(sv.size(), 3u);
  EXPECT_EQ(sv[0], L"x");
}

TEST(StringUtil, unsplit_basic) {
  vector<string> sv = {"a", "b", "c"};
  string result;
  unsplit(sv, string(","), result);
  EXPECT_EQ(result, "a,b,c");
}

TEST(StringUtil, itos_itow) {
  EXPECT_EQ(itos(42), "42");
  EXPECT_EQ(itos(-1), "-1");
  EXPECT_EQ(std::wstring(itow(42)), L"42");
}

TEST(StringUtil, compareStringIgnoreCase) {
  EXPECT_EQ(compareStringIgnoreCase(L"Hello", L"hello"), 0);
  EXPECT_NE(compareStringIgnoreCase(L"abc", L"abd"), 0);
}

TEST(StringUtil, toLowerStripped_ascii) {
  EXPECT_EQ(toLowerStripped(L'A'), (int)'a');
  EXPECT_EQ(toLowerStripped(L'z'), (int)'z');
}

TEST(StringUtil, toLowerStripped_swedish) {
  EXPECT_EQ(toLowerStripped(L'Å'), (int)'a');
  EXPECT_EQ(toLowerStripped(L'Ö'), (int)'o');
}

TEST(StringUtil, isAscii_isNumber) {
  EXPECT_TRUE(isAscii(string("hello")));
  EXPECT_FALSE(isAscii(wstring(L"åäö")));
  EXPECT_TRUE(isNumber(string("12345")));
  EXPECT_FALSE(isNumber(string("12a")));
}

TEST(StringUtil, encodeXML_decodeXML_roundtrip) {
  string src = "a & b < c > d \"";
  const string &enc = encodeXML(src);
  EXPECT_NE(enc, src);
  const string &dec = decodeXML(enc);
  EXPECT_EQ(dec, src);
}

TEST(StringUtil, encodeXML_wstring) {
  wstring src = L"a & b";
  const wstring &enc = encodeXML(src);
  EXPECT_EQ(enc, L"a &amp; b");
}

// ---- Misc -------------------------------------------------------------------
TEST(Misc, formatRank) {
  EXPECT_EQ(formatRank(7), L"(0007)");
}

TEST(Misc, makeDash) {
  const wstring &s = makeDash(L"A-B-C");
  EXPECT_EQ(s[1], (wchar_t)0x2013);
}

TEST(Misc, version_non_empty) {
  EXPECT_FALSE(getMeosFullVersion().empty());
  EXPECT_GT(getMeosBuild(), 0);
}

TEST(Misc, getNumberSuffix) {
  // Scans from right skipping trailing digits+spaces until a non-digit,
  // then returns atoi from that position.
  EXPECT_EQ(getNumberSuffix(string("H21 Start 1")), 1);
  EXPECT_EQ(getNumberSuffix(string("H21 Start")),   0); // ends with letter
  EXPECT_EQ(getNumberSuffix(string("H21")),         21);
}

TEST(Misc, compareBib_length) {
  EXPECT_TRUE(compareBib(L"10", L"100")); // shorter < longer
}

TEST(Misc, getGivenName_getFamilyName) {
  EXPECT_EQ(getGivenName(L"Anna Svensson"),  L"Anna");
  EXPECT_EQ(getFamilyName(L"Anna Svensson"), L"Svensson");
}

TEST(Misc, getGivenName_comma) {
  EXPECT_EQ(getGivenName(L"Svensson, Anna"),  L"Anna");
  EXPECT_EQ(getFamilyName(L"Svensson, Anna"), L"Svensson");
}

TEST(Misc, convertDynamicBase) {
  long long out;
  int base = convertDynamicBase(wstring(L"123"), out);
  EXPECT_EQ(base, 10);
  EXPECT_EQ(out, 123LL);
}

TEST(HLS, RGBtoHLS_black) {
  HLS h;
  h.RGBtoHLS(0x000000);
  EXPECT_EQ(h.lightness, 0);
}

TEST(HLS, RGBtoHLS_white) {
  HLS h;
  h.RGBtoHLS(0xFFFFFF);
  EXPECT_EQ(h.lightness, 252);
}

TEST(HLS, HLStoRGB_roundtrip) {
  uint32_t original = 0x3366AA;
  HLS h;
  h.RGBtoHLS(original);
  uint32_t back = h.HLStoRGB();
  // Allow small rounding error (±2 per channel)
  int dr = abs((int)(original&0xFF)         - (int)(back&0xFF));
  int dg = abs((int)((original>>8)&0xFF)   - (int)((back>>8)&0xFF));
  int db = abs((int)((original>>16)&0xFF)  - (int)((back>>16)&0xFF));
  EXPECT_LE(dr, 2);
  EXPECT_LE(dg, 2);
  EXPECT_LE(db, 2);
}
