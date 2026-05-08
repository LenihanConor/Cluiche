// TestRGBA.cpp - Google Test unit tests for RGBA
//
// Tests RGBA color representation from DiaGraphics

#include <gtest/gtest.h>
#include <DiaGraphics/Misc/RGBA.h>

using namespace Dia::Graphics;

// ==============================================================================
// RGBA Color Constants Tests
// ==============================================================================

TEST(RGBA, Green_MatchesComponents)
{
    RGBA green = RGBA::Green;

    EXPECT_EQ(green, RGBA(0, 255, 0, 255));
}

TEST(RGBA, Blue_MatchesComponents)
{
    RGBA blue = RGBA::Blue;

    EXPECT_EQ(blue, RGBA(0, 0, 255, 255));
}

TEST(RGBA, Red_MatchesComponents)
{
    RGBA red = RGBA::Red;

    EXPECT_EQ(red, RGBA(255, 0, 0, 255));
}

TEST(RGBA, White_MatchesComponents)
{
    RGBA white = RGBA::White;

    EXPECT_EQ(white, RGBA(255, 255, 255, 255));
}

TEST(RGBA, Black_MatchesComponents)
{
    RGBA black = RGBA::Black;

    EXPECT_EQ(black, RGBA(0, 0, 0, 255));
}
