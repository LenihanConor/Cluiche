// TestExpectationEvaluator.cpp - Google Test unit tests for Dia::CaptureTest::ExpectationEvaluator

#include <gtest/gtest.h>
#include <DiaCaptureTest/ExpectationEvaluator.h>
#include <DiaGraphics/Interface/FrameCapture.h>

#include <cstdio>
#include <cstring>
#include <direct.h>

using Dia::CaptureTest::ExpectationEvaluator;
using Dia::CaptureTest::EvaluationResult;
using Dia::CaptureTest::FrameDiffResult;
using Dia::Graphics::FrameCaptureResult;

// Helper: write a temp .expectations.json to C:\Temp\<name>
static bool WriteTempExpectations(const char* filename, const char* content)
{
    _mkdir("C:\\Temp");
    FILE* f = std::fopen(filename, "wb");
    if (!f) return false;
    std::fwrite(content, 1, std::strlen(content), f);
    std::fclose(f);
    return true;
}

// Helper: build a solid-colour FrameCaptureResult backed by a static pixel buffer.
// Returns FrameCaptureResult with pointers into the caller-supplied buffer.
static FrameCaptureResult MakeSolidCapture(
    unsigned char* pixels, unsigned int width, unsigned int height,
    unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    for (unsigned int y = 0; y < height; ++y)
        for (unsigned int x = 0; x < width; ++x)
        {
            unsigned int off = (y * width + x) * 4;
            pixels[off + 0] = r;
            pixels[off + 1] = g;
            pixels[off + 2] = b;
            pixels[off + 3] = a;
        }

    FrameCaptureResult cap;
    cap.status = FrameCaptureResult::Status::kReady;
    cap.data   = pixels;
    cap.width  = width;
    cap.height = height;
    cap.pitch  = width * 4;
    return cap;
}

// ==============================================================================
// ExpectationEvaluator_Load_InvalidPath_ReturnsFalse
// ==============================================================================
TEST(ExpectationEvaluator, ExpectationEvaluator_Load_InvalidPath_ReturnsFalse)
{
    ExpectationEvaluator eval;
    const bool result = eval.Load("nonexistent.json");
    EXPECT_FALSE(result);
}

// ==============================================================================
// ExpectationEvaluator_Brightness_PassRule
// Solid grey 4x4 image (R=50,G=50,B=50,A=255).
// brightness > 0.01 should pass (actual ~= 50/255 ~= 0.196).
// ==============================================================================
TEST(ExpectationEvaluator, ExpectationEvaluator_Brightness_PassRule)
{
    const char* kPath = "C:\\Temp\\test_brightness_pass.expectations.json";

    const char* json =
        "{"
        "  \"tag\": \"test\","
        "  \"rules\": ["
        "    { \"id\": \"f\", \"type\": \"brightness\", \"region\": [0,0], \"op\": \">\", \"value\": 0.01 }"
        "  ]"
        "}";

    ASSERT_TRUE(WriteTempExpectations(kPath, json));

    ExpectationEvaluator eval;
    ASSERT_TRUE(eval.Load(kPath));

    static unsigned char pixels[4 * 4 * 4];
    FrameCaptureResult cap = MakeSolidCapture(pixels, 4, 4, 50, 50, 50, 255);

    FrameDiffResult diff;
    std::memset(&diff, 0, sizeof(diff));

    EvaluationResult result = eval.Evaluate(cap, diff);

    EXPECT_TRUE(result.allPass);
    EXPECT_EQ(result.ruleCount, 1u);
    EXPECT_TRUE(result.results[0].pass);
    EXPECT_GT(result.results[0].actualValue, 0.01f);

    std::remove(kPath);
}

// ==============================================================================
// ExpectationEvaluator_Brightness_FailRule
// All-black 4x4 image (R=0,G=0,B=0,A=255).
// brightness > 0.01 should fail (actual = 0.0).
// ==============================================================================
TEST(ExpectationEvaluator, ExpectationEvaluator_Brightness_FailRule)
{
    const char* kPath = "C:\\Temp\\test_brightness_fail.expectations.json";

    const char* json =
        "{"
        "  \"tag\": \"test\","
        "  \"rules\": ["
        "    { \"id\": \"f\", \"type\": \"brightness\", \"region\": [0,0], \"op\": \">\", \"value\": 0.01 }"
        "  ]"
        "}";

    ASSERT_TRUE(WriteTempExpectations(kPath, json));

    ExpectationEvaluator eval;
    ASSERT_TRUE(eval.Load(kPath));

    static unsigned char pixels[4 * 4 * 4];
    FrameCaptureResult cap = MakeSolidCapture(pixels, 4, 4, 0, 0, 0, 255);

    FrameDiffResult diff;
    std::memset(&diff, 0, sizeof(diff));

    EvaluationResult result = eval.Evaluate(cap, diff);

    EXPECT_FALSE(result.allPass);
    EXPECT_EQ(result.ruleCount, 1u);
    EXPECT_FALSE(result.results[0].pass);
    EXPECT_FLOAT_EQ(result.results[0].actualValue, 0.0f);

    std::remove(kPath);
}

// ==============================================================================
// ExpectationEvaluator_Metric_Rule
// metric key draw_calls == 3 with metricJson containing draw_calls:3 -> pass.
// ==============================================================================
TEST(ExpectationEvaluator, ExpectationEvaluator_Metric_Rule)
{
    const char* kPath = "C:\\Temp\\test_metric.expectations.json";

    const char* json =
        "{"
        "  \"tag\": \"test\","
        "  \"rules\": ["
        "    { \"id\": \"dc\", \"type\": \"metric\", \"key\": \"draw_calls\", \"op\": \"==\", \"value\": 3 }"
        "  ]"
        "}";

    ASSERT_TRUE(WriteTempExpectations(kPath, json));

    ExpectationEvaluator eval;
    ASSERT_TRUE(eval.Load(kPath));

    // Minimal capture (not used by metric rules)
    FrameCaptureResult cap;
    cap.status = FrameCaptureResult::Status::kInvalidToken;
    cap.data   = nullptr;
    cap.width  = 0;
    cap.height = 0;
    cap.pitch  = 0;

    FrameDiffResult diff;
    std::memset(&diff, 0, sizeof(diff));

    const char* metricJson = "{\"metrics\":{\"draw_calls\":3}}";

    EvaluationResult result = eval.Evaluate(cap, diff, metricJson);

    EXPECT_TRUE(result.allPass);
    EXPECT_EQ(result.ruleCount, 1u);
    EXPECT_TRUE(result.results[0].pass);
    EXPECT_FLOAT_EQ(result.results[0].actualValue, 3.0f);

    std::remove(kPath);
}
