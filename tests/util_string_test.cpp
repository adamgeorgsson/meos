#include <gtest/gtest.h>
#include "meos_util.h"
#include "meosexception.h"

using namespace meos::util;

// ---------------------------------------------------------------------------
// toUTF8 / fromUTF8 round-trip
// ---------------------------------------------------------------------------

TEST(StringUtil, ToUtf8_AsciiRoundTrip) {
    std::wstring ws = L"Hello, world!";
    auto utf8 = toUTF8(ws);
    EXPECT_EQ(utf8, "Hello, world!");
}

TEST(StringUtil, FromUtf8_AsciiRoundTrip) {
    std::string s = "Hello, world!";
    auto ws = fromUTF8(s);
    EXPECT_EQ(ws, L"Hello, world!");
}

TEST(StringUtil, ToUtf8_FromUtf8_WideRoundTrip) {
    std::wstring original = L"caf\u00E9 na\u00EFve r\u00F4le";  // café naïve rôle
    std::string utf8 = toUTF8(original);
    const std::wstring& back = fromUTF8(utf8);
    EXPECT_EQ(back, original);
}

TEST(StringUtil, ToUtf8_EuroSign) {
    std::wstring ws = L"\u20AC";  // €
    auto s = toUTF8(ws);
    // U+20AC encodes to E2 82 AC
    ASSERT_EQ(s.size(), 3u);
    EXPECT_EQ(static_cast<unsigned char>(s[0]), 0xE2);
    EXPECT_EQ(static_cast<unsigned char>(s[1]), 0x82);
    EXPECT_EQ(static_cast<unsigned char>(s[2]), 0xAC);
}

TEST(StringUtil, FromUtf8_EuroSign) {
    std::string s = "\xE2\x82\xAC";  // UTF-8 for €
    const std::wstring& ws = fromUTF8(s);
    ASSERT_EQ(ws.size(), 1u);
    EXPECT_EQ(static_cast<uint32_t>(ws[0]), 0x20ACu);
}

TEST(StringUtil, ToUtf8_EmptyString) {
    EXPECT_EQ(toUTF8(L""), "");
}

TEST(StringUtil, FromUtf8_EmptyString) {
    EXPECT_EQ(fromUTF8(""), L"");
}

// ---------------------------------------------------------------------------
// widen (CP-1252 → Unicode)
// ---------------------------------------------------------------------------

TEST(StringUtil, Widen_AsciiUnchanged) {
    std::string s = "ABC 123";
    const std::wstring& ws = widen(s);
    EXPECT_EQ(ws, L"ABC 123");
}

TEST(StringUtil, Widen_Latin1Range) {
    // 0xE9 = é in Latin-1 and CP-1252 → U+00E9
    std::string s = "\xE9";
    const std::wstring& ws = widen(s);
    ASSERT_EQ(ws.size(), 1u);
    EXPECT_EQ(static_cast<uint32_t>(ws[0]), 0x00E9u);
}

TEST(StringUtil, Widen_Cp1252_EuroSign) {
    // 0x80 in CP-1252 = € = U+20AC
    std::string s = "\x80";
    const std::wstring& ws = widen(s);
    ASSERT_EQ(ws.size(), 1u);
    EXPECT_EQ(static_cast<uint32_t>(ws[0]), 0x20ACu);
}

TEST(StringUtil, Widen_Cp1252_SmartQuotes) {
    // 0x91 = ' (U+2018), 0x92 = ' (U+2019)
    std::string s = "\x91Hello\x92";
    const std::wstring& ws = widen(s);
    ASSERT_EQ(ws.size(), 7u);
    EXPECT_EQ(static_cast<uint32_t>(ws[0]), 0x2018u);
    EXPECT_EQ(static_cast<uint32_t>(ws[6]), 0x2019u);
}

TEST(StringUtil, Widen_EmptyString) {
    EXPECT_EQ(widen(""), L"");
}

// ---------------------------------------------------------------------------
// narrow (Unicode → Latin-1, '?' for out-of-range)
// ---------------------------------------------------------------------------

TEST(StringUtil, Narrow_AsciiUnchanged) {
    std::wstring ws = L"ABC 123";
    const std::string& s = narrow(ws);
    EXPECT_EQ(s, "ABC 123");
}

TEST(StringUtil, Narrow_Latin1Range) {
    std::wstring ws = L"\u00E9";  // é
    const std::string& s = narrow(ws);
    ASSERT_EQ(s.size(), 1u);
    EXPECT_EQ(static_cast<unsigned char>(s[0]), 0xE9u);
}

TEST(StringUtil, Narrow_OutOfRangeReplaced) {
    // U+20AC (€) > 0xFF → replaced with '?'
    std::wstring ws = L"\u20AC";
    const std::string& s = narrow(ws);
    ASSERT_EQ(s.size(), 1u);
    EXPECT_EQ(s[0], '?');
}

TEST(StringUtil, Narrow_EmptyString) {
    EXPECT_EQ(narrow(L""), "");
}

// ---------------------------------------------------------------------------
// string2Wide / wide2String
// ---------------------------------------------------------------------------

TEST(StringUtil, String2Wide_AsciiRoundTrip) {
    std::wstring out;
    string2Wide("Hello", out);
    EXPECT_EQ(out, L"Hello");
}

TEST(StringUtil, String2Wide_Cp1252_Euro) {
    std::wstring out;
    string2Wide("\x80", out);
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(static_cast<uint32_t>(out[0]), 0x20ACu);
}

TEST(StringUtil, Wide2String_AsciiRoundTrip) {
    std::string out;
    wide2String(L"Hello", out);
    EXPECT_EQ(out, "Hello");
}

TEST(StringUtil, Wide2String_Latin1Preserved) {
    std::string out;
    wide2String(L"\u00E9", out);  // é
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(static_cast<unsigned char>(out[0]), 0xE9u);
}

TEST(StringUtil, Wide2String_OutOfRange_Replaced) {
    std::string out;
    wide2String(L"\u20AC", out);  // €
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0], '?');
}

// ---------------------------------------------------------------------------
// meosException
// ---------------------------------------------------------------------------

TEST(MeosException, WstringCtor) {
    meosException ex(std::wstring(L"Wide error"));
    EXPECT_EQ(ex.wwhat(), L"Wide error");
    EXPECT_NE(std::string(ex.what()).find("Wide error"), std::string::npos);
}

TEST(MeosException, StringCtor) {
    meosException ex(std::string("narrow error"));
    EXPECT_EQ(std::string(ex.what()), "narrow error");
    EXPECT_EQ(ex.wwhat(), L"narrow error");
}

TEST(MeosException, CharPtrCtor) {
    meosException ex("char* error");
    EXPECT_EQ(std::string(ex.what()), "char* error");
    EXPECT_EQ(ex.wwhat(), L"char* error");
}

TEST(MeosException, DefaultCtor) {
    meosException ex;
    EXPECT_EQ(std::string(ex.what()), "");
}

TEST(MeosException, IsDerivedFromRuntimeError) {
    meosException ex("test");
    std::runtime_error* base = &ex;
    EXPECT_EQ(std::string(base->what()), "test");
}

TEST(MeosCancel, IsDerivedFromMeosException) {
    meosCancel c("cancel");
    meosException* base = &c;
    EXPECT_EQ(std::string(base->what()), "cancel");
}
