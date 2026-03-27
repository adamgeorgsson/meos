#include <gtest/gtest.h>
#include "localizer.h"
#include <fstream>
#include <filesystem>

namespace {

// Helper: write a temp .lng file and return its path
std::filesystem::path writeTempLng(const std::string &content) {
  auto tmp = std::filesystem::temp_directory_path() / "meos_test_lang.lng";
  std::ofstream f(tmp, std::ios::out | std::ios::trunc);
  f << content;
  return tmp;
}

} // namespace

// --- Basic load and translate -------------------------------------------------

TEST(LocalizerTest, LoadAndTranslate) {
  auto path = writeTempLng(
      "Hej = Hello\n"
      "Adjö = Goodbye\n"
      "Lyssna = Listen\n");

  Localizer l;
  l.init();
  l.get().addLangResource(L"English", path.wstring());
  l.get().loadLangResource(L"English");

  EXPECT_EQ(l.tl(L"Hej"),   L"Hello");
  EXPECT_EQ(l.tl(L"Adjö"),  L"Goodbye");
  EXPECT_EQ(l.tl(L"Lyssna"), L"Listen");

  std::filesystem::remove(path);
}

// --- Newline escaping ---------------------------------------------------------

TEST(LocalizerTest, NewlineEscaping) {
  auto path = writeTempLng("Radbrytning = Rad ett\\nRad två\n");

  Localizer l;
  l.init();
  l.get().addLangResource(L"Swedish", path.wstring());
  l.get().loadLangResource(L"Swedish");

  EXPECT_EQ(l.tl(L"Radbrytning"), L"Rad ett\nRad två");

  std::filesystem::remove(path);
}

// --- Comments skipped ---------------------------------------------------------

TEST(LocalizerTest, CommentsSkipped) {
  auto path = writeTempLng(
      "# This is a comment\n"
      "Nyckel = Värde\n");

  Localizer l;
  l.init();
  l.get().addLangResource(L"Test", path.wstring());
  l.get().loadLangResource(L"Test");

  EXPECT_EQ(l.tl(L"Nyckel"), L"Värde");

  std::filesystem::remove(path);
}

// --- Untranslated string returns original ------------------------------------

TEST(LocalizerTest, UntranslatedReturnsOriginal) {
  Localizer l;
  l.init();
  EXPECT_EQ(l.tl(L"NotInDictionary"), L"NotInDictionary");
}

// --- Hash prefix strips the '#' and returns literal --------------------------

TEST(LocalizerTest, HashPrefixLiteral) {
  Localizer l;
  l.init();
  EXPECT_EQ(l.tl(L"#literal string"), L"literal string");
}

// --- @ prefix strips the '@' -------------------------------------------------

TEST(LocalizerTest, AtPrefixLiteral) {
  Localizer l;
  l.init();
  EXPECT_EQ(l.tl(L"@verbatim"), L"verbatim");
}

// --- Empty string returns empty ----------------------------------------------

TEST(LocalizerTest, EmptyString) {
  Localizer l;
  l.init();
  EXPECT_EQ(l.tl(L""), L"");
}

// --- capitalizeWords --------------------------------------------------------

TEST(LocalizerTest, CapitalizeWordsEnglish) {
  auto path = writeTempLng("Lyssna = Listen\n");

  Localizer l;
  l.init();
  l.get().addLangResource(L"English", path.wstring());
  l.get().loadLangResource(L"English");

  EXPECT_TRUE(l.capitalizeWords());

  std::filesystem::remove(path);
}

TEST(LocalizerTest, CapitalizeWordsSwedish) {
  // Without English loaded, "Lyssna" won't translate to "Listen"
  Localizer l;
  l.init();
  EXPECT_FALSE(l.capitalizeWords());
}

// --- Suffix strip (trailing punctuation) -------------------------------------

TEST(LocalizerTest, TrailingSuffixStripped) {
  auto path = writeTempLng("Tid = Time\n");

  Localizer l;
  l.init();
  l.get().addLangResource(L"English", path.wstring());
  l.get().loadLangResource(L"English");

  // "Tid:" should find "Tid" in table and append ":"
  EXPECT_EQ(l.tl(L"Tid:"), L"Time:");

  std::filesystem::remove(path);
}

// --- getLangResource ---------------------------------------------------------

TEST(LocalizerTest, GetLangResource) {
  auto path = writeTempLng("A = B\n");

  Localizer l;
  l.init();
  l.get().addLangResource(L"English", path.wstring());
  l.get().addLangResource(L"Swedish", path.wstring());

  auto names = l.get().getLangResource();
  ASSERT_EQ(names.size(), 2u);
  // Names must contain English and Swedish (order not guaranteed)
  bool hasEn = false, hasSv = false;
  for (const auto &n : names) {
    if (n == L"English") hasEn = true;
    if (n == L"Swedish") hasSv = true;
  }
  EXPECT_TRUE(hasEn);
  EXPECT_TRUE(hasSv);

  std::filesystem::remove(path);
}

// --- oWordList stub -----------------------------------------------------------

TEST(LocalizerTest, WordListStubAlwaysFalse) {
  oWordList wl;
  wl.insert(L"test");
  EXPECT_FALSE(wl.lookup(L"test"));
  EXPECT_FALSE(wl.lookup(L"other"));
}

// --- getGivenNames returns stub -----------------------------------------------

TEST(LocalizerTest, GetGivenNamesReturnsStub) {
  Localizer l;
  l.init();
  const oWordList &wl = l.get().getGivenNames();
  EXPECT_FALSE(wl.lookup(L"Erik"));
}

// --- tl(string) narrow overload -----------------------------------------------

TEST(LocalizerTest, TlNarrowString) {
  auto path = writeTempLng("Start = Start\n");

  Localizer l;
  l.init();
  l.get().addLangResource(L"Test", path.wstring());
  l.get().loadLangResource(L"Test");

  // narrow string overload
  EXPECT_EQ(l.tl(std::string("Start")), L"Start");

  std::filesystem::remove(path);
}

// --- Global lang object accessible -------------------------------------------

TEST(LocalizerTest, GlobalLangExists) {
  // Just verify the global is accessible and survives init/unload cycle
  lang.init();
  EXPECT_EQ(lang.tl(L""), L"");
  lang.unload();
}
