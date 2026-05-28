#include "meos_util.h"

#include <cstdint>

namespace meos::util {

// ---------------------------------------------------------------------------
// CP-1252 decoding table for bytes 0x80-0x9F.
// Values outside this range map directly to their Unicode code point
// (0x00-0x7F are ASCII; 0xA0-0xFF are identical to Unicode Latin-1 Supplement).
// ---------------------------------------------------------------------------
static const uint32_t cp1252_80_9F[32] = {
    0x20AC, // 0x80  €
    0x0081, // 0x81  undefined → keep byte value
    0x201A, // 0x82  ‚
    0x0192, // 0x83  ƒ
    0x201E, // 0x84  „
    0x2026, // 0x85  …
    0x2020, // 0x86  †
    0x2021, // 0x87  ‡
    0x02C6, // 0x88  ˆ
    0x2030, // 0x89  ‰
    0x0160, // 0x8A  Š
    0x2039, // 0x8B  ‹
    0x0152, // 0x8C  Œ
    0x008D, // 0x8D  undefined → keep byte value
    0x017D, // 0x8E  Ž
    0x008F, // 0x8F  undefined → keep byte value
    0x0090, // 0x90  undefined → keep byte value
    0x2018, // 0x91  '
    0x2019, // 0x92  '
    0x201C, // 0x93  "
    0x201D, // 0x94  "
    0x2022, // 0x95  •
    0x2013, // 0x96  –
    0x2014, // 0x97  —
    0x02DC, // 0x98  ˜
    0x2122, // 0x99  ™
    0x0161, // 0x9A  š
    0x203A, // 0x9B  ›
    0x0153, // 0x9C  œ
    0x009D, // 0x9D  undefined → keep byte value
    0x017E, // 0x9E  ž
    0x0178, // 0x9F  Ÿ
};

// Decode a single CP-1252 byte to its Unicode code point.
static inline uint32_t cp1252ToUnicode(unsigned char c) {
    if (c >= 0x80 && c <= 0x9F)
        return cp1252_80_9F[c - 0x80];
    return static_cast<uint32_t>(c);
}

// ---------------------------------------------------------------------------
// Manual UTF-8 encoder: encodes a single Unicode code point.
// ---------------------------------------------------------------------------
static void encodeUtf8(uint32_t cp, std::string& out) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void string2Wide(const std::string& in, std::wstring& out) {
    out.clear();
    out.reserve(in.size());
    for (unsigned char c : in)
        out.push_back(static_cast<wchar_t>(cp1252ToUnicode(c)));
}

void wide2String(const std::wstring& in, std::string& out) {
    out.clear();
    out.reserve(in.size());
    for (wchar_t wc : in) {
        if (static_cast<uint32_t>(wc) > 0xFF)
            out.push_back('?');
        else
            out.push_back(static_cast<char>(wc & 0xFF));
    }
}

const std::wstring& widen(const std::string& input) {
    thread_local std::wstring buf;
    string2Wide(input, buf);
    return buf;
}

const std::string& narrow(const std::wstring& input) {
    thread_local std::string buf;
    wide2String(input, buf);
    return buf;
}

const std::string& toUTF8(const std::wstring& input) {
    thread_local std::string buf;
    buf.clear();
    buf.reserve(input.size() * 2);
    for (wchar_t wc : input)
        encodeUtf8(static_cast<uint32_t>(wc), buf);
    return buf;
}

const std::wstring& fromUTF8(const std::string& input) {
    thread_local std::wstring buf;
    buf.clear();
    buf.reserve(input.size());
    size_t i = 0;
    const size_t n = input.size();
    while (i < n) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        uint32_t cp = 0;
        if (c <= 0x7F) {
            cp = c;
            ++i;
        } else if ((c & 0xE0) == 0xC0 && i + 1 < n) {
            cp = (c & 0x1F) << 6;
            cp |= static_cast<unsigned char>(input[i + 1]) & 0x3F;
            i += 2;
        } else if ((c & 0xF0) == 0xE0 && i + 2 < n) {
            cp = (c & 0x0F) << 12;
            cp |= (static_cast<unsigned char>(input[i + 1]) & 0x3F) << 6;
            cp |= static_cast<unsigned char>(input[i + 2]) & 0x3F;
            i += 3;
        } else if ((c & 0xF8) == 0xF0 && i + 3 < n) {
            cp = (c & 0x07) << 18;
            cp |= (static_cast<unsigned char>(input[i + 1]) & 0x3F) << 12;
            cp |= (static_cast<unsigned char>(input[i + 2]) & 0x3F) << 6;
            cp |= static_cast<unsigned char>(input[i + 3]) & 0x3F;
            i += 4;
        } else {
            // Invalid byte — skip it
            ++i;
            continue;
        }
        buf.push_back(static_cast<wchar_t>(cp));
    }
    return buf;
}

}  // namespace meos::util
