#include "hls.h"

#include <algorithm>
#include <cstdint>

static constexpr short HLSMAX = 252;
static constexpr short RGBMAX = 255;
static constexpr short UNDEFINED_HUE = (HLSMAX * 2 / 3);

HLS& HLS::RGBtoHLS(uint32_t lRGBColor) {
    uint16_t R = getRValue(lRGBColor);
    uint16_t G = getGValue(lRGBColor);
    uint16_t B = getBValue(lRGBColor);

    auto cMax = static_cast<uint8_t>(std::max({R, G, B}));
    auto cMin = static_cast<uint8_t>(std::min({R, G, B}));

    lightness = static_cast<short>(
        ((static_cast<int>(cMax) + cMin) * HLSMAX + RGBMAX) / (2 * RGBMAX));

    if (cMax == cMin) {
        saturation = 0;
        hue = UNDEFINED_HUE;
    } else {
        if (lightness <= HLSMAX / 2)
            saturation = static_cast<short>(
                ((cMax - cMin) * HLSMAX + (cMax + cMin) / 2) / (cMax + cMin));
        else
            saturation = static_cast<short>(
                ((cMax - cMin) * HLSMAX + (2 * RGBMAX - cMax - cMin) / 2) /
                (2 * RGBMAX - cMax - cMin));

        short Rdelta = static_cast<short>(
            ((cMax - R) * (HLSMAX / 6) + (cMax - cMin) / 2) / (cMax - cMin));
        short Gdelta = static_cast<short>(
            ((cMax - G) * (HLSMAX / 6) + (cMax - cMin) / 2) / (cMax - cMin));
        short Bdelta = static_cast<short>(
            ((cMax - B) * (HLSMAX / 6) + (cMax - cMin) / 2) / (cMax - cMin));

        if (R == cMax)
            hue = static_cast<short>(Bdelta - Gdelta);
        else if (G == cMax)
            hue = static_cast<short>(HLSMAX / 3 + Rdelta - Bdelta);
        else
            hue = static_cast<short>((2 * HLSMAX) / 3 + Gdelta - Rdelta);

        if (hue < 0)     hue = static_cast<short>(hue + HLSMAX);
        if (hue > HLSMAX) hue = static_cast<short>(hue - HLSMAX);
    }
    return *this;
}

short HLS::HueToRGB(short n1, short n2, short hue) const {
    if (hue < 0)      hue = static_cast<short>(hue + HLSMAX);
    if (hue > HLSMAX) hue = static_cast<short>(hue - HLSMAX);

    if (hue < HLSMAX / 6)
        return static_cast<short>(n1 + ((n2 - n1) * hue + HLSMAX / 12) / (HLSMAX / 6));
    if (hue < HLSMAX / 2)
        return n2;
    if (hue < (HLSMAX * 2) / 3)
        return static_cast<short>(
            n1 + ((n2 - n1) * ((HLSMAX * 2 / 3) - hue) + HLSMAX / 12) / (HLSMAX / 6));
    return n1;
}

uint32_t HLS::HLStoRGB() const {
    uint8_t R, G, B;
    if (saturation == 0) {
        R = G = B = static_cast<uint8_t>((lightness * RGBMAX) / HLSMAX);
    } else {
        short Magic2;
        if (lightness <= HLSMAX / 2)
            Magic2 = static_cast<short>((lightness * (HLSMAX + saturation) + HLSMAX / 2) / HLSMAX);
        else
            Magic2 = static_cast<short>(
                lightness + saturation - (lightness * saturation + HLSMAX / 2) / HLSMAX);
        short Magic1 = static_cast<short>(2 * lightness - Magic2);

        R = static_cast<uint8_t>(
            (HueToRGB(Magic1, Magic2, static_cast<short>(hue + HLSMAX / 3)) * RGBMAX +
             HLSMAX / 2) / HLSMAX);
        G = static_cast<uint8_t>(
            (HueToRGB(Magic1, Magic2, hue) * RGBMAX + HLSMAX / 2) / HLSMAX);
        B = static_cast<uint8_t>(
            (HueToRGB(Magic1, Magic2, static_cast<short>(hue - HLSMAX / 3)) * RGBMAX +
             HLSMAX / 2) / HLSMAX);
    }
    return makeRGB(R, G, B);
}

void HLS::lighten(double f) {
    lightness = static_cast<short>(std::min<int>(HLSMAX, static_cast<int>(f * lightness)));
}

void HLS::saturate(double s) {
    saturation = static_cast<short>(std::min<int>(HLSMAX, static_cast<int>(s * saturation)));
}

void HLS::colorDegree(double /*d*/) {
    // Intentionally empty — used as an extension point in legacy UI code.
}
