// TestCaptureReportWriter.cpp - Google Test unit tests for Dia::CaptureTest::CaptureReportWriter

#include <gtest/gtest.h>
#include <DiaCaptureTest/CaptureReport.h>
#include <DiaCaptureTest/FrameDiff.h>

#include <cstdio>
#include <direct.h>

using Dia::CaptureTest::CaptureReportWriter;
using Dia::CaptureTest::CaptureMetadata;
using Dia::CaptureTest::FrameDiffResult;
using Dia::CaptureTest::RegionStat;

// ==============================================================================
// CaptureReportWriter_WritesFile
// Write a FrameDiffResult + CaptureMetadata to C:\Temp\test_report.json,
// verify the file exists and is non-empty.
// ==============================================================================

TEST(CaptureReportWriter, CaptureReportWriter_WritesFile)
{
    // Ensure C:\Temp exists
    _mkdir("C:\\Temp");

    const char* kPath = "C:\\Temp\\test_report.json";

    // Remove any prior file
    std::remove(kPath);

    // Build metadata
    CaptureMetadata meta;
    meta.tag         = "mesh3d_render_system";
    meta.frameNumber = 60;
    meta.backend     = "dx11";
    meta.commitHash  = "856e56ec";

    // Build a minimal FrameDiffResult
    FrameDiffResult diff;
    diff.pass             = false;
    diff.totalPixels      = 921600;
    diff.differingPixels  = 4200;
    diff.differingPct     = 0.46f;
    diff.maxDelta         = 12;
    diff.threshold        = 4;
    diff.regionCount      = 1;
    diff.regions[0].row          = 1;
    diff.regions[0].col          = 2;
    diff.regions[0].differingPct = 3.1f;
    diff.regions[0].maxDelta     = 12;

    const bool result = CaptureReportWriter::Write(kPath, meta, diff);

    EXPECT_TRUE(result) << "Write should return true for a valid path";

    // Verify file exists and is non-empty
    FILE* f = std::fopen(kPath, "rb");
    ASSERT_NE(f, nullptr) << "JSON file was not created at " << kPath;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fclose(f);
    EXPECT_GT(size, 0L) << "JSON file should be non-empty";

    // Cleanup
    std::remove(kPath);
}

// ==============================================================================
// CaptureReportWriter_BadPath_ReturnsFalse
// Write to a non-existent directory path -> returns false
// ==============================================================================

TEST(CaptureReportWriter, CaptureReportWriter_BadPath_ReturnsFalse)
{
    CaptureMetadata meta;
    meta.tag         = "test";
    meta.frameNumber = 1;
    meta.backend     = "dx11";
    meta.commitHash  = "";

    FrameDiffResult diff;
    diff.pass             = true;
    diff.totalPixels      = 16;
    diff.differingPixels  = 0;
    diff.differingPct     = 0.0f;
    diff.maxDelta         = 0;
    diff.threshold        = 4;
    diff.regionCount      = 0;

    const bool result = CaptureReportWriter::Write(
        "Z:\\nonexistent\\path\\report.json", meta, diff);

    EXPECT_FALSE(result) << "Write to non-existent path should return false";
}
