#pragma once

#include <cstdint>

namespace design
{
    namespace colour
    {
        constexpr std::uint32_t bgApp         = 0xff0E1015;
        constexpr std::uint32_t bgStage       = 0xff11141B;
        constexpr std::uint32_t surfacePanel  = 0xff171B24;
        constexpr std::uint32_t surfaceRaised = 0xff202533;
        constexpr std::uint32_t surfaceHover  = 0xff293042;
        constexpr std::uint32_t borderSubtle  = 0xff262C3A;
        constexpr std::uint32_t borderStrong  = 0xff363E52;
        constexpr std::uint32_t textPrimary   = 0xffECEEF3;
        constexpr std::uint32_t textSecondary = 0xffA0A7B8;
        constexpr std::uint32_t textMuted     = 0xff636B7E;
        constexpr std::uint32_t textOnAccent  = 0xff06121C;
        constexpr std::uint32_t expected      = 0xff46C2FF;
        constexpr std::uint32_t correct       = 0xff37D483;
        constexpr std::uint32_t wrong         = 0xffFF5B61;
        constexpr std::uint32_t heard         = 0xffF5C451;
        constexpr std::uint32_t noteRight     = 0xff6E8BFF;
        constexpr std::uint32_t noteLeft      = 0xffFFA94D;
        constexpr std::uint32_t noteOther     = 0xff2B3140;
        constexpr std::uint32_t keyWhite      = 0xffF3F4F7;
        constexpr std::uint32_t keyWhiteShade = 0xffC9CDD6;
        constexpr std::uint32_t keyBlack      = 0xff191C24;
        constexpr std::uint32_t keyBlackTop   = 0xff2E3340;
    }

    namespace space  { constexpr int s4 = 4, s8 = 8, s12 = 12, s16 = 16, s24 = 24, s32 = 32; }
    namespace radius { constexpr float sm = 6.0f, md = 10.0f, lg = 14.0f; }

    namespace layout
    {
        constexpr int topBar = 64, progress = 36, bottomBar = 56;
        constexpr int lowestKey = 36, highestKey = 96;
        constexpr float whiteKeyAspect = 5.0f;
        constexpr float blackKeyWidth = 0.6f;
        constexpr float blackKeyHeight = 124.0f / 200.0f;
        constexpr float pixelsPerSecond = 160.0f;
    }

    namespace text
    {
        struct Style { float size; float trackingPercent; };
        constexpr Style titlePiece  { 18.0f, -1.0f };
        constexpr Style titleApp    { 15.0f, -1.0f };
        constexpr Style body        { 14.0f,  0.0f };
        constexpr Style bodyStrong  { 14.0f,  0.0f };
        constexpr Style labelCaps   { 11.0f,  8.0f };
        constexpr Style monoValue   { 22.0f, -2.0f };
        constexpr Style monoSmall   { 11.0f,  0.0f };
        constexpr Style promptLarge { 28.0f, -2.0f };
    }
}
