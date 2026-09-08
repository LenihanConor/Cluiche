// TestMetricsWriter.cpp - Google Test unit tests for Dia::CaptureTest::MetricsWriter

#include <gtest/gtest.h>
#include <DiaCaptureTest/MetricsWriter.h>

#include <cstdio>
#include <direct.h>

using Dia::CaptureTest::MetricsWriter;
using Dia::CaptureTest::MetricEntry;

// ==============================================================================
// MetricsWriter_Write_CreatesFile
// Write 3 metric entries to C:\Temp\test_metrics.json,
// verify returns true and file is non-empty.
// ==============================================================================

TEST(MetricsWriter, MetricsWriter_Write_CreatesFile)
{
    // Ensure C:\Temp exists
    _mkdir("C:\\Temp");

    const char* kPath = "C:\\Temp\\test_metrics.json";

    // Remove any prior file
    std::remove(kPath);

    MetricEntry entries[3];
    entries[0].key   = "draw_calls";
    entries[0].value = 3.0f;
    entries[1].key   = "gpu_mesh_count";
    entries[1].value = 1.0f;
    entries[2].key   = "frame_time_ms";
    entries[2].value = 16.67f;

    const bool result = MetricsWriter::Write(kPath, "mesh3d_render_system", 60, entries, 3);

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
// MetricsWriter_Write_BadPath_ReturnsFalse
// Write to a non-existent directory path -> returns false.
// ==============================================================================

TEST(MetricsWriter, MetricsWriter_Write_BadPath_ReturnsFalse)
{
    MetricEntry entries[1];
    entries[0].key   = "draw_calls";
    entries[0].value = 1.0f;

    const bool result = MetricsWriter::Write(
        "Z:\\nonexistent\\path\\metrics.json",
        "test",
        1,
        entries,
        1);

    EXPECT_FALSE(result) << "Write to non-existent path should return false";
}

// ==============================================================================
// MetricsWriter_Write_ZeroEntries_Succeeds
// Write 0 entries -> returns true, file exists with valid JSON (empty metrics object).
// ==============================================================================

TEST(MetricsWriter, MetricsWriter_Write_ZeroEntries_Succeeds)
{
    // Ensure C:\Temp exists
    _mkdir("C:\\Temp");

    const char* kPath = "C:\\Temp\\test_metrics_empty.json";

    // Remove any prior file
    std::remove(kPath);

    const bool result = MetricsWriter::Write(kPath, "empty_stage", 0, nullptr, 0);

    EXPECT_TRUE(result) << "Write with zero entries should return true";

    // Verify file exists and is non-empty
    FILE* f = std::fopen(kPath, "rb");
    ASSERT_NE(f, nullptr) << "JSON file was not created at " << kPath;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fclose(f);
    EXPECT_GT(size, 0L) << "JSON file should contain at least the root object";

    // Cleanup
    std::remove(kPath);
}
