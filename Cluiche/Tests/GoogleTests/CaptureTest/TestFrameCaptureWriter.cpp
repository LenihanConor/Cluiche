// TestFrameCaptureWriter.cpp - Google Test unit tests for FrameCaptureWriter
//
// Tests the async and sync PNG write helper in DiaBgfx.

#include <gtest/gtest.h>
#include <DiaBgfx/Capture/FrameCaptureWriter.h>

#include <cstdio>
#include <direct.h>
#include <thread>
#include <chrono>

using Dia::Bgfx::FrameCaptureWriter;
using Dia::Graphics::FrameCaptureResult;

// ==============================================================================
// WriteSync — invalid status
// ==============================================================================

TEST(FrameCaptureWriter, WriteSync_InvalidStatus_ReturnsFalse)
{
    FrameCaptureResult result;
    result.status = FrameCaptureResult::Status::kPending;
    result.data   = nullptr;
    result.width  = 0;
    result.height = 0;
    result.pitch  = 0;

    const bool ret = FrameCaptureWriter::WriteSync(result, "C:\\Temp\\should_not_exist.png");

    EXPECT_FALSE(ret);
}

// ==============================================================================
// WriteSync — writes valid PNG
// ==============================================================================

TEST(FrameCaptureWriter, WriteSync_WritesFile)
{
    // 4x4 RGBA8 image, all red pixels: R=255 G=0 B=0 A=255
    static const unsigned int kWidth  = 4;
    static const unsigned int kHeight = 4;
    static const unsigned int kComp   = 4; // RGBA
    static const unsigned int kPitch  = kWidth * kComp;

    unsigned char pixels[kWidth * kHeight * kComp];
    for (unsigned int i = 0; i < sizeof(pixels); i += kComp)
    {
        pixels[i + 0] = 255; // R
        pixels[i + 1] = 0;   // G
        pixels[i + 2] = 0;   // B
        pixels[i + 3] = 255; // A
    }

    FrameCaptureResult result;
    result.status = FrameCaptureResult::Status::kReady;
    result.data   = pixels;
    result.width  = kWidth;
    result.height = kHeight;
    result.pitch  = kPitch;

    const char* kPath = "C:\\Temp\\test_capture.png";

    // Ensure C:\Temp exists
    _mkdir("C:\\Temp");

    // Remove any prior file
    std::remove(kPath);

    const bool ret = FrameCaptureWriter::WriteSync(result, kPath);

    EXPECT_TRUE(ret);

    // Verify file was created and is non-empty
    FILE* f = std::fopen(kPath, "rb");
    ASSERT_NE(f, nullptr) << "PNG file was not created at " << kPath;
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fclose(f);
    EXPECT_GT(size, 0L);

    // Cleanup
    std::remove(kPath);
}

// ==============================================================================
// WriteAsync — invalid status does nothing
// ==============================================================================

TEST(FrameCaptureWriter, WriteAsync_InvalidStatus_DoesNothing)
{
    FrameCaptureResult result;
    result.status = FrameCaptureResult::Status::kPending;
    result.data   = nullptr;
    result.width  = 0;
    result.height = 0;
    result.pitch  = 0;

    const char* kPath = "C:\\Temp\\async_should_not_exist.png";

    // Ensure any pre-existing file is gone
    std::remove(kPath);

    FrameCaptureWriter::WriteAsync(result, kPath);

    // Give any (unwanted) thread time to run
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    FILE* f = std::fopen(kPath, "rb");
    EXPECT_EQ(f, nullptr) << "File should not have been written for kPending status";
    if (f) std::fclose(f);
}
