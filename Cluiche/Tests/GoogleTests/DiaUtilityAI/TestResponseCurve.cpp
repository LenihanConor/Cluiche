#include <gtest/gtest.h>

#include <DiaUtilityAI/ResponseCurve.h>
#include <DiaCore/Json/external/json/json.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
    constexpr float kTol = 0.001f;

    Json::Value ParseJson(const char* src)
    {
        Json::Value root;
        Json::Reader reader;
        reader.parse(src, root);
        return root;
    }

    Dia::UtilityAI::ResponseCurve LoadCurve(const char* json)
    {
        Json::Value root = ParseJson(json);
        return Dia::UtilityAI::ResponseCurve::LoadFromJson(root);
    }
}

// ---------------------------------------------------------------------------
// 1. Default constructor
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurve, DefaultConstructor_ShapeIsLinear)
{
    Dia::UtilityAI::ResponseCurve curve;
    EXPECT_EQ(curve.GetShape(), Dia::UtilityAI::CurveShape::kLinear);
}

// ---------------------------------------------------------------------------
// 2. Linear shape
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurve, Linear_Evaluate_Zero)
{
    Dia::UtilityAI::ResponseCurve curve;
    EXPECT_NEAR(curve.Evaluate(0.0f), 0.0f, kTol);
}

TEST(DiaUtilityAI_ResponseCurve, Linear_Evaluate_Half)
{
    Dia::UtilityAI::ResponseCurve curve;
    EXPECT_NEAR(curve.Evaluate(0.5f), 0.5f, kTol);
}

TEST(DiaUtilityAI_ResponseCurve, Linear_Evaluate_One)
{
    Dia::UtilityAI::ResponseCurve curve;
    EXPECT_NEAR(curve.Evaluate(1.0f), 1.0f, kTol);
}

// ---------------------------------------------------------------------------
// 3. Quadratic shape (exponent=2)
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurve, Quadratic_Exp2_Half)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "quadratic", "exponent": 2.0 })");
    EXPECT_NEAR(curve.Evaluate(0.5f), 0.25f, kTol);
}

TEST(DiaUtilityAI_ResponseCurve, Quadratic_Exp2_One)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "quadratic", "exponent": 2.0 })");
    EXPECT_NEAR(curve.Evaluate(1.0f), 1.0f, kTol);
}

// ---------------------------------------------------------------------------
// 4. Exponential shape (k=4)
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurve, Exponential_K4_Zero)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "exponential", "k": 4.0 })");
    EXPECT_NEAR(curve.Evaluate(0.0f), 0.0f, kTol);
}

TEST(DiaUtilityAI_ResponseCurve, Exponential_K4_One)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "exponential", "k": 4.0 })");
    EXPECT_NEAR(curve.Evaluate(1.0f), 1.0f, kTol);
}

TEST(DiaUtilityAI_ResponseCurve, Exponential_K4_Half_InRange)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "exponential", "k": 4.0 })");
    const float v = curve.Evaluate(0.5f);
    EXPECT_GT(v, 0.0f);
    EXPECT_LT(v, 1.0f);
}

// ---------------------------------------------------------------------------
// 5. Sine shape
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurve, Sine_Evaluate_Zero)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "sine" })");
    EXPECT_NEAR(curve.Evaluate(0.0f), 0.0f, kTol);
}

TEST(DiaUtilityAI_ResponseCurve, Sine_Evaluate_One)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "sine" })");
    EXPECT_NEAR(curve.Evaluate(1.0f), 1.0f, kTol);
}

TEST(DiaUtilityAI_ResponseCurve, Sine_Evaluate_Half)
{
    // sin(0.5 * PI/2) = sin(PI/4) ≈ 0.7071
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "sine" })");
    EXPECT_NEAR(curve.Evaluate(0.5f), 0.7071f, 0.002f);
}

// ---------------------------------------------------------------------------
// 6. Step shape (threshold=0.5)
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurve, Step_Threshold05_BelowThreshold)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "step", "threshold": 0.5 })");
    EXPECT_NEAR(curve.Evaluate(0.49f), 0.0f, kTol);
}

TEST(DiaUtilityAI_ResponseCurve, Step_Threshold05_AtThreshold)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "step", "threshold": 0.5 })");
    EXPECT_NEAR(curve.Evaluate(0.5f), 1.0f, kTol);
}

TEST(DiaUtilityAI_ResponseCurve, Step_Threshold05_AtOne)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "step", "threshold": 0.5 })");
    EXPECT_NEAR(curve.Evaluate(1.0f), 1.0f, kTol);
}

// ---------------------------------------------------------------------------
// 7. Logistic shape
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurve, Logistic_NearZeroAtZero)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "logistic" })");
    EXPECT_NEAR(curve.Evaluate(0.0f), 0.0f, 0.01f);
}

TEST(DiaUtilityAI_ResponseCurve, Logistic_NearOneAtOne)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "logistic" })");
    EXPECT_NEAR(curve.Evaluate(1.0f), 1.0f, 0.01f);
}

TEST(DiaUtilityAI_ResponseCurve, Logistic_NearHalfAtHalf)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "logistic" })");
    EXPECT_NEAR(curve.Evaluate(0.5f), 0.5f, 0.01f);
}

// ---------------------------------------------------------------------------
// 8. Invert flag
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurve, Invert_Linear_PointThree)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "linear", "invert": true })");
    EXPECT_NEAR(curve.Evaluate(0.3f), 0.7f, kTol);
}

// ---------------------------------------------------------------------------
// 9. Input clamping
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurve, Clamp_NegativeInput_ReturnsZero)
{
    Dia::UtilityAI::ResponseCurve curve;
    EXPECT_NEAR(curve.Evaluate(-0.5f), 0.0f, kTol);
}

TEST(DiaUtilityAI_ResponseCurve, Clamp_OverOneInput_ReturnsOne)
{
    Dia::UtilityAI::ResponseCurve curve;
    EXPECT_NEAR(curve.Evaluate(1.5f), 1.0f, kTol);
}

// ---------------------------------------------------------------------------
// 10. LoadFromJson — quadratic exponent 3.0
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurve, LoadFromJson_QuadraticExp3_ShapeCorrect)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "quadratic", "exponent": 3.0 })");
    EXPECT_EQ(curve.GetShape(), Dia::UtilityAI::CurveShape::kQuadratic);
}

TEST(DiaUtilityAI_ResponseCurve, LoadFromJson_QuadraticExp3_EvaluateHalf)
{
    // 0.5^3 = 0.125
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "quadratic", "exponent": 3.0 })");
    EXPECT_NEAR(curve.Evaluate(0.5f), 0.125f, kTol);
}

// ---------------------------------------------------------------------------
// 11. LoadFromJson — step with threshold 0.7
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurve, LoadFromJson_StepThreshold07_BelowThreshold)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "step", "threshold": 0.7 })");
    EXPECT_NEAR(curve.Evaluate(0.69f), 0.0f, kTol);
}

TEST(DiaUtilityAI_ResponseCurve, LoadFromJson_StepThreshold07_AboveThreshold)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "step", "threshold": 0.7 })");
    EXPECT_NEAR(curve.Evaluate(0.71f), 1.0f, kTol);
}

// ---------------------------------------------------------------------------
// 12. LoadFromJson — unknown shape defaults to kLinear
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurve, LoadFromJson_UnknownShape_DefaultsToLinear)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "unknown_curve" })");
    EXPECT_EQ(curve.GetShape(), Dia::UtilityAI::CurveShape::kLinear);
}

// ---------------------------------------------------------------------------
// 13. LoadFromJson — invert=true
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurve, LoadFromJson_InvertTrue_LinearAtZeroIsOne)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "linear", "invert": true })");
    EXPECT_NEAR(curve.Evaluate(0.0f), 1.0f, kTol);
}
