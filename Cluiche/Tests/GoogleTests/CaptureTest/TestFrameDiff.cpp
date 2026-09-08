// TestFrameDiff.cpp - Google Test unit tests for Dia::CaptureTest::FrameDiff

#include <gtest/gtest.h>
#include <DiaCaptureTest/FrameDiff.h>

using Dia::CaptureTest::FrameDiff;
using Dia::CaptureTest::FrameDiffResult;

// ==============================================================================
// FrameDiff_IdenticalBuffers_Pass
// Identical 4x4 black buffers with threshold=4 -> pass=true, differingPixels=0
// ==============================================================================

TEST(FrameDiff, FrameDiff_IdenticalBuffers_Pass)
{
    static const unsigned int kWidth  = 4;
    static const unsigned int kHeight = 4;

    unsigned char ref[kWidth * kHeight * 4] = {};
    unsigned char run[kWidth * kHeight * 4] = {};

    FrameDiffResult result = FrameDiff::Diff(ref, run, kWidth, kHeight, 4);

    EXPECT_TRUE(result.pass);
    EXPECT_EQ(result.differingPixels, 0u);
}

// ==============================================================================
// FrameDiff_SinglePixelDiff_Fail
// 4x4 black ref vs run with pixel[0,0] R=20 (delta=20 > threshold=4)
// -> pass=false, differingPixels=1
// ==============================================================================

TEST(FrameDiff, FrameDiff_SinglePixelDiff_Fail)
{
    static const unsigned int kWidth  = 4;
    static const unsigned int kHeight = 4;

    unsigned char ref[kWidth * kHeight * 4] = {};
    unsigned char run[kWidth * kHeight * 4] = {};

    // Set pixel [0,0] R channel to 20 in run buffer
    run[0] = 20; // R at pixel (0,0)

    FrameDiffResult result = FrameDiff::Diff(ref, run, kWidth, kHeight, 4);

    EXPECT_FALSE(result.pass);
    EXPECT_EQ(result.differingPixels, 1u);
}

// ==============================================================================
// FrameDiff_RegionStats_Populated
// Same single-pixel diff as above -> regionCount=16 (4x4 grid),
// region[0] (top-left) has differingPct > 0
// ==============================================================================

TEST(FrameDiff, FrameDiff_RegionStats_Populated)
{
    static const unsigned int kWidth  = 4;
    static const unsigned int kHeight = 4;

    unsigned char ref[kWidth * kHeight * 4] = {};
    unsigned char run[kWidth * kHeight * 4] = {};

    // Set pixel [0,0] R channel to 20 in run buffer
    run[0] = 20;

    FrameDiffResult result = FrameDiff::Diff(ref, run, kWidth, kHeight, 4);

    EXPECT_EQ(result.regionCount, 16u);

    // The top-left region (row=0, col=0) should have differingPct > 0
    bool foundDifferingRegion = false;
    for (unsigned int i = 0; i < result.regionCount; ++i)
    {
        if (result.regions[i].row == 0 && result.regions[i].col == 0)
        {
            EXPECT_GT(result.regions[i].differingPct, 0.0f)
                << "Region [0,0] should have differingPct > 0";
            foundDifferingRegion = true;
            break;
        }
    }
    EXPECT_TRUE(foundDifferingRegion) << "Region [0,0] not found in regions array";
}

// ==============================================================================
// FrameDiff_AllSame_Zero_maxDelta
// Identical buffers -> maxDelta=0
// ==============================================================================

TEST(FrameDiff, FrameDiff_AllSame_Zero_maxDelta)
{
    static const unsigned int kWidth  = 4;
    static const unsigned int kHeight = 4;

    unsigned char ref[kWidth * kHeight * 4] = {};
    unsigned char run[kWidth * kHeight * 4] = {};

    FrameDiffResult result = FrameDiff::Diff(ref, run, kWidth, kHeight, 4);

    EXPECT_EQ(result.maxDelta, 0u);
}
