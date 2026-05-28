#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <cstring>

#include "time_util.h"
#include "file_util.h"
#include "hls.h"
#include "meos_version.h"

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// time_util tests
// ---------------------------------------------------------------------------

TEST(TimeFormatTest, FormatZero) {
    EXPECT_EQ(meos::util::formatTimeHMS(0), "00:00:00");
}

TEST(TimeFormatTest, FormatOneHour) {
    EXPECT_EQ(meos::util::formatTimeHMS(3600), "01:00:00");
}

TEST(TimeFormatTest, FormatHMS) {
    EXPECT_EQ(meos::util::formatTimeHMS(3661), "01:01:01");
}

TEST(TimeFormatTest, FormatNegative) {
    EXPECT_EQ(meos::util::formatTimeHMS(-1), "-00:00:01");
}

TEST(TimeFormatTest, ParseRoundTrip) {
    EXPECT_EQ(meos::util::parseTimeHMS("01:01:01"), 3661);
    EXPECT_EQ(meos::util::parseTimeHMS("00:00:00"), 0);
    EXPECT_EQ(meos::util::parseTimeHMS("10:30:15"), 37815);
}

TEST(TimeFormatTest, ParseInvalidThrows) {
    EXPECT_THROW(meos::util::parseTimeHMS("invalid"), std::invalid_argument);
    EXPECT_THROW(meos::util::parseTimeHMS("01:01"), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// file_util tests
// ---------------------------------------------------------------------------

TEST(FileUtilTest, FileExistsFalseForMissing) {
    EXPECT_FALSE(fileExists(L"/tmp/meos_test_nonexistent_xyz_12345.txt"));
}

TEST(FileUtilTest, FileExistsTrueForRealFile) {
    // Create a temp file and verify detection.
    std::string tmp = "/tmp/meos_fileutil_test_exists.txt";
    {
        std::ofstream f(tmp);
        f << "test";
    }
    EXPECT_TRUE(fileExists(std::wstring(tmp.begin(), tmp.end())));
    fs::remove(tmp);
}

TEST(FileUtilTest, MoveFileRenamesFile) {
    std::string src = "/tmp/meos_move_src.txt";
    std::string dst = "/tmp/meos_move_dst.txt";
    {
        std::ofstream f(src);
        f << "hello";
    }
    moveFile(std::wstring(src.begin(), src.end()),
             std::wstring(dst.begin(), dst.end()));
    EXPECT_FALSE(fs::exists(src));
    EXPECT_TRUE(fs::exists(dst));
    fs::remove(dst);
}

TEST(FileUtilTest, ExpandDirectoryFindsFiles) {
    // Create a temp dir with some files.
    std::string tmpDir = "/tmp/meos_expand_test_dir";
    fs::create_directories(tmpDir);
    std::ofstream(tmpDir + "/a.xml") << "x";
    std::ofstream(tmpDir + "/b.xml") << "x";
    std::ofstream(tmpDir + "/c.txt") << "x";

    std::vector<std::wstring> found;
    std::wstring dir = std::wstring(tmpDir.begin(), tmpDir.end());
    bool ok = expandDirectory(dir.c_str(), L"*.xml", found);
    EXPECT_TRUE(ok);
    EXPECT_EQ(found.size(), 2u);
    fs::remove_all(tmpDir);
}

TEST(FileUtilTest, ExpandDirectoryEmptyForMissing) {
    std::vector<std::wstring> found;
    bool ok = expandDirectory(L"/tmp/meos_missing_dir_xyz_12345", L"*.xml", found);
    EXPECT_FALSE(ok);
    EXPECT_TRUE(found.empty());
}

// ---------------------------------------------------------------------------
// HLS color tests
// ---------------------------------------------------------------------------

TEST(HLSTest, GreyRoundTrip) {
    // Pure grey: R=G=B=128 → saturation=0
    uint32_t grey = makeRGB(128, 128, 128);
    HLS h;
    h.RGBtoHLS(grey);
    EXPECT_EQ(h.saturation, 0);
    uint32_t back = h.HLStoRGB();
    // Grey should round-trip close to the original (within rounding)
    EXPECT_NEAR(static_cast<int>(getRValue(back)), 128, 2);
    EXPECT_NEAR(static_cast<int>(getGValue(back)), 128, 2);
    EXPECT_NEAR(static_cast<int>(getBValue(back)), 128, 2);
}

TEST(HLSTest, BlackRoundTrip) {
    uint32_t black = makeRGB(0, 0, 0);
    HLS h;
    h.RGBtoHLS(black);
    EXPECT_EQ(h.lightness, 0);
    uint32_t back = h.HLStoRGB();
    EXPECT_EQ(getRValue(back), 0u);
    EXPECT_EQ(getGValue(back), 0u);
    EXPECT_EQ(getBValue(back), 0u);
}

TEST(HLSTest, WhiteRoundTrip) {
    uint32_t white = makeRGB(255, 255, 255);
    HLS h;
    h.RGBtoHLS(white);
    uint32_t back = h.HLStoRGB();
    EXPECT_NEAR(static_cast<int>(getRValue(back)), 255, 2);
    EXPECT_NEAR(static_cast<int>(getGValue(back)), 255, 2);
    EXPECT_NEAR(static_cast<int>(getBValue(back)), 255, 2);
}

TEST(HLSTest, MakeRGBExtractors) {
    uint32_t c = makeRGB(10, 20, 30);
    EXPECT_EQ(getRValue(c), 10u);
    EXPECT_EQ(getGValue(c), 20u);
    EXPECT_EQ(getBValue(c), 30u);
}

TEST(HLSTest, LightenReduces) {
    HLS h(42, 100, 80);
    h.lighten(0.5);
    EXPECT_EQ(h.lightness, 50);
}

TEST(HLSTest, SaturateReduces) {
    HLS h(42, 100, 80);
    h.saturate(0.5);
    EXPECT_EQ(h.saturation, 40);
}

// ---------------------------------------------------------------------------
// Version function tests
// ---------------------------------------------------------------------------

TEST(VersionTest, GetMeosBuildPositive) {
    EXPECT_GT(getMeosBuild(), 0);
}

TEST(VersionTest, GetMeosDateFormat) {
    std::wstring d = getMeosDate();
    ASSERT_EQ(d.size(), 10u);
    EXPECT_EQ(d[4], L'-');
    EXPECT_EQ(d[7], L'-');
}

TEST(VersionTest, GetMeosFullVersionContainsVersion) {
    std::wstring v = getMeosFullVersion();
    EXPECT_NE(v.find(L"Version"), std::wstring::npos);
}

TEST(VersionTest, GetMajorVersion) {
    EXPECT_FALSE(getMajorVersion().empty());
}

TEST(VersionTest, GetCompectVersionNonEmpty) {
    EXPECT_FALSE(getMeosCompectVersion().empty());
}

TEST(VersionTest, GetSupportersNonEmpty) {
    std::vector<std::wstring> supp, dev;
    getSupporters(supp, dev);
    EXPECT_GT(supp.size(), 0u);
    EXPECT_GT(dev.size(), 0u);
}

// ---------------------------------------------------------------------------
// meos_memicmp portable wrapper
// ---------------------------------------------------------------------------

TEST(MemicmpTest, CaseInsensitiveMatch) {
    EXPECT_EQ(meos_memicmp("UTF-8", "utf-8", 5), 0);
    EXPECT_EQ(meos_memicmp("UTF-8", "UTF-8", 5), 0);
}

TEST(MemicmpTest, CaseInsensitiveMismatch) {
    EXPECT_NE(meos_memicmp("UTF-8", "utf-9", 5), 0);
}
