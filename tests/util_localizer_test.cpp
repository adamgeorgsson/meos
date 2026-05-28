#include <gtest/gtest.h>
#include "localizer.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Helper: write a temp .lng file
// ---------------------------------------------------------------------------
static std::wstring writeTmpLng(const std::string& content) {
    auto tmp = fs::temp_directory_path() / "meos_localizer_test.lng";
    std::ofstream f(tmp);
    f << content;
    f.close();
    return tmp.wstring();
}

// ---------------------------------------------------------------------------
// oWordList stub
// ---------------------------------------------------------------------------
TEST(LocalizerStub, oWordList_AlwaysReturnsFalse) {
    oWordList wl;
    std::wstring result;
    EXPECT_FALSE(wl.lookup(L"Alice", result));
}

// ---------------------------------------------------------------------------
// Basic translation
// ---------------------------------------------------------------------------
TEST(LocalizerImpl, BasicTranslation) {
    Localizer loc;
    loc.init();
    auto path = writeTmpLng("Start = Begin\nFinish = End\n");
    loc.get().addLangResource(L"test", path);
    loc.get().loadLangResource(L"test");
    EXPECT_EQ(loc.tl(L"Start"), L"Begin");
    EXPECT_EQ(loc.tl(L"Finish"), L"End");
    loc.unload();
}

TEST(LocalizerImpl, UnknownKey_ReturnsInput) {
    Localizer loc;
    loc.init();
    auto path = writeTmpLng("Foo = Bar\n");
    loc.get().addLangResource(L"test", path);
    loc.get().loadLangResource(L"test");
    EXPECT_EQ(loc.tl(L"Unknown"), L"Unknown");
    loc.unload();
}

TEST(LocalizerImpl, EmptyString_ReturnsEmpty) {
    Localizer loc;
    loc.init();
    auto path = writeTmpLng("A = B\n");
    loc.get().addLangResource(L"test", path);
    loc.get().loadLangResource(L"test");
    EXPECT_EQ(loc.tl(std::wstring(L"")), L"");
    loc.unload();
}

// ---------------------------------------------------------------------------
// Hash-prefix: '#' strips the '#' and returns rest verbatim
// ---------------------------------------------------------------------------
TEST(LocalizerImpl, HashPrefix_ReturnsRestVerbatim) {
    Localizer loc;
    loc.init();
    auto path = writeTmpLng("A = B\n");
    loc.get().addLangResource(L"test", path);
    loc.get().loadLangResource(L"test");
    EXPECT_EQ(loc.tl(L"#verbatim"), L"verbatim");
    loc.unload();
}

// ---------------------------------------------------------------------------
// At-prefix: '@' strips the '@' and returns rest verbatim
// ---------------------------------------------------------------------------
TEST(LocalizerImpl, AtPrefix_ReturnsRestVerbatim) {
    Localizer loc;
    loc.init();
    auto path = writeTmpLng("A = B\n");
    loc.get().addLangResource(L"test", path);
    loc.get().loadLangResource(L"test");
    EXPECT_EQ(loc.tl(L"@untranslated text"), L"untranslated text");
    loc.unload();
}

// ---------------------------------------------------------------------------
// Embedded newline escape: \n in value becomes real newline
// ---------------------------------------------------------------------------
TEST(LocalizerImpl, EmbeddedNewline) {
    Localizer loc;
    loc.init();
    auto path = writeTmpLng("Help = Line1\\nLine2\n");
    loc.get().addLangResource(L"test", path);
    loc.get().loadLangResource(L"test");
    EXPECT_EQ(loc.tl(L"Help"), L"Line1\nLine2");
    loc.unload();
}

// ---------------------------------------------------------------------------
// Comment lines ('#' and '%') are skipped
// ---------------------------------------------------------------------------
TEST(LocalizerImpl, CommentLines_Skipped) {
    Localizer loc;
    loc.init();
    auto path = writeTmpLng("# comment\n% percent comment\nReal = Translated\n");
    loc.get().addLangResource(L"test", path);
    loc.get().loadLangResource(L"test");
    EXPECT_EQ(loc.tl(L"Real"), L"Translated");
    loc.unload();
}

// ---------------------------------------------------------------------------
// Narrow (string) tl overload
// ---------------------------------------------------------------------------
TEST(LocalizerImpl, NarrowTl_ConvertsKey) {
    Localizer loc;
    loc.init();
    auto path = writeTmpLng("Start = Begin\n");
    loc.get().addLangResource(L"test", path);
    loc.get().loadLangResource(L"test");
    EXPECT_EQ(loc.tl(std::string("Start")), L"Begin");
    loc.unload();
}

// ---------------------------------------------------------------------------
// getLangResource returns registered names
// ---------------------------------------------------------------------------
TEST(LocalizerImpl, GetLangResource_ReturnsNames) {
    Localizer loc;
    loc.init();
    auto path = writeTmpLng("A = B\n");
    loc.get().addLangResource(L"english", path);
    auto names = loc.get().getLangResource();
    ASSERT_EQ(names.size(), 1u);
    EXPECT_EQ(names[0], L"english");
    loc.unload();
}

// ---------------------------------------------------------------------------
// loadLangResource with unknown language throws
// ---------------------------------------------------------------------------
TEST(LocalizerImpl, LoadUnknownLanguage_Throws) {
    Localizer loc;
    loc.init();
    EXPECT_THROW(loc.get().loadLangResource(L"nonexistent"), std::runtime_error);
    loc.unload();
}

// ---------------------------------------------------------------------------
// File not found → empty table (no throw)
// ---------------------------------------------------------------------------
TEST(LocalizerImpl, FileNotFound_EmptyTable) {
    Localizer loc;
    loc.init();
    loc.get().addLangResource(L"ghost", L"/nonexistent/path/ghost.lng");
    EXPECT_NO_THROW(loc.get().loadLangResource(L"ghost"));
    EXPECT_EQ(loc.tl(L"Anything"), L"Anything");
    loc.unload();
}

// ---------------------------------------------------------------------------
// Trailing-punctuation strip: "Start:" translates if "Start" is in table
// ---------------------------------------------------------------------------
TEST(LocalizerImpl, TrailingColon_TranslatesBase) {
    Localizer loc;
    loc.init();
    auto path = writeTmpLng("Start = Begin\n");
    loc.get().addLangResource(L"test", path);
    loc.get().loadLangResource(L"test");
    // "Start:" → look up "Start", append ":"
    EXPECT_EQ(loc.tl(L"Start:"), L"Begin:");
    loc.unload();
}

// ---------------------------------------------------------------------------
// Multiple entries and whitespace trimming
// ---------------------------------------------------------------------------
TEST(LocalizerImpl, WhitespaceTrimming) {
    Localizer loc;
    loc.init();
    auto path = writeTmpLng("Name   =   Alice\nAge = 30\n");
    loc.get().addLangResource(L"test", path);
    loc.get().loadLangResource(L"test");
    EXPECT_EQ(loc.tl(L"Name"), L"Alice");
    EXPECT_EQ(loc.tl(L"Age"), L"30");
    loc.unload();
}

// ---------------------------------------------------------------------------
// capitalizeWords: returns true only when "Lyssna" -> "Listen"
// ---------------------------------------------------------------------------
TEST(LocalizerImpl, CapitalizeWords_English) {
    Localizer loc;
    loc.init();
    // English: Lyssna = Listen
    auto path = writeTmpLng("Lyssna = Listen\n");
    loc.get().addLangResource(L"english", path);
    loc.get().loadLangResource(L"english");
    EXPECT_TRUE(loc.capitalizeWords());
    loc.unload();
}

TEST(LocalizerImpl, CapitalizeWords_Swedish) {
    Localizer loc;
    loc.init();
    // Swedish: Lyssna = Lyssna (same, not "Listen")
    auto path = writeTmpLng("Lyssna = Lyssna\n");
    loc.get().addLangResource(L"swedish", path);
    loc.get().loadLangResource(L"swedish");
    EXPECT_FALSE(loc.capitalizeWords());
    loc.unload();
}

// ---------------------------------------------------------------------------
// Real .lng file smoke test: load english.lng and translate a known key
// ---------------------------------------------------------------------------
TEST(LocalizerReal, EnglishLng_KnownKey) {
    // Locate english.lng relative to executable or skip if absent
    auto lngPath = fs::path(RESOURCES_DIR) / "lang" / "english.lng";
    if (!fs::exists(lngPath)) {
        GTEST_SKIP() << "english.lng not found at " << lngPath;
    }
    Localizer loc;
    loc.init();
    loc.get().addLangResource(L"english", lngPath.wstring());
    loc.get().loadLangResource(L"english");
    // "(kopia)" → "(copy)" per english.lng
    EXPECT_EQ(loc.tl(L"(kopia)"), L"(copy)");
    loc.unload();
}
