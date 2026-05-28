#pragma once

#include <cstdint>
#include <algorithm>

// HLS (Hue-Lightness-Saturation) color class.
// Replaces Win32 WORD/BYTE/RGB macros with portable uint16_t/uint8_t/inline helpers.
class HLS {
    short HueToRGB(short n1, short n2, short hue) const;
public:
    HLS(short H, short L, short S) : hue(H), lightness(L), saturation(S) {}
    HLS() : hue(0), lightness(0), saturation(1) {}
    short hue;
    short lightness;
    short saturation;
    void lighten(double f);
    void saturate(double s);
    void colorDegree(double d);
    HLS& RGBtoHLS(uint32_t lRGBColor);
    uint32_t HLStoRGB() const;
};

// Portable replacements for Win32 RGB/GetRValue/GetGValue/GetBValue macros.
inline uint32_t makeRGB(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint32_t>(r) |
           (static_cast<uint32_t>(g) << 8) |
           (static_cast<uint32_t>(b) << 16);
}
inline uint8_t getRValue(uint32_t c) { return static_cast<uint8_t>(c & 0xFF); }
inline uint8_t getGValue(uint32_t c) { return static_cast<uint8_t>((c >> 8) & 0xFF); }
inline uint8_t getBValue(uint32_t c) { return static_cast<uint8_t>((c >> 16) & 0xFF); }
