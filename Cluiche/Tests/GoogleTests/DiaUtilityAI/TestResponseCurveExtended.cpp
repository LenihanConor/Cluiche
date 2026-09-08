#include <gtest/gtest.h>

#include <DiaUtilityAI/ResponseCurve.h>
#include <DiaUtilityAI/UtilitySet.h>
#include <DiaCondition/Testing/ConditionTestHelpers.h>
#include <DiaRules/RuleActionRegistry.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
    constexpr float kTol  = 0.001f;
    constexpr float kTol2 = 0.003f;  // slightly wider for trig results

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

    Dia::UtilityAI::UtilitySet LoadSet(const char* json)
    {
        Json::Value root = ParseJson(json);
        return Dia::UtilityAI::UtilitySet::LoadFromJson(root);
    }
} // anonymous namespace

// ---------------------------------------------------------------------------
// 1. Linear: 9 interior points all produce output == input
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurveExtended, Linear_AllInteriorPoints_OutputEqualsInput)
{
    Dia::UtilityAI::ResponseCurve curve;
    const float pts[] = { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.9f };
    for (float x : pts)
        EXPECT_NEAR(curve.Evaluate(x), x, kTol) << "at x=" << x;
}

// ---------------------------------------------------------------------------
// 2. Quadratic (exp=2): spot-check four points: x^2
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurveExtended, Quadratic_Exp2_FourPoints)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "quadratic", "exponent": 2.0 })");

    EXPECT_NEAR(curve.Evaluate(0.10f), 0.0100f, kTol);
    EXPECT_NEAR(curve.Evaluate(0.25f), 0.0625f, kTol);
    EXPECT_NEAR(curve.Evaluate(0.75f), 0.5625f, kTol);
    EXPECT_NEAR(curve.Evaluate(0.90f), 0.8100f, kTol);
}

// ---------------------------------------------------------------------------
// 3. Exponential (k=4): curve is strictly increasing over [0, 0.25, 0.5, 0.75, 1]
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurveExtended, Exponential_K4_StrictlyIncreasing)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "exponential", "k": 4.0 })");

    const float pts[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
    float prev = -1.0f;
    for (float x : pts)
    {
        float v = curve.Evaluate(x);
        EXPECT_GT(v, prev) << "not strictly increasing at x=" << x;
        prev = v;
    }
}

// ---------------------------------------------------------------------------
// 4. Sine: value at 0.25 ≈ sin(PI/8) ≈ 0.3827
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurveExtended, Sine_Evaluate_AtQuarter)
{
    // sin(0.25 * PI/2) = sin(PI/8) ≈ 0.38268
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "sine" })");
    EXPECT_NEAR(curve.Evaluate(0.25f), 0.38268f, kTol2);
}

// ---------------------------------------------------------------------------
// 5. Sine: value at 0.75 ≈ sin(3*PI/8) ≈ 0.9239
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurveExtended, Sine_Evaluate_AtThreeQuarters)
{
    // sin(0.75 * PI/2) = sin(3*PI/8) ≈ 0.92388
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "sine" })");
    EXPECT_NEAR(curve.Evaluate(0.75f), 0.92388f, kTol2);
}

// ---------------------------------------------------------------------------
// 6-8. Step (threshold=0.3): below / at / above boundary
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurveExtended, Step_Threshold0_3_JustBelow_ReturnsZero)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "step", "threshold": 0.3 })");
    EXPECT_NEAR(curve.Evaluate(0.29f), 0.0f, kTol);
}

TEST(DiaUtilityAI_ResponseCurveExtended, Step_Threshold0_3_AtThreshold_ReturnsOne)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "step", "threshold": 0.3 })");
    EXPECT_NEAR(curve.Evaluate(0.30f), 1.0f, kTol);
}

TEST(DiaUtilityAI_ResponseCurveExtended, Step_Threshold0_3_JustAbove_ReturnsOne)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "step", "threshold": 0.3 })");
    EXPECT_NEAR(curve.Evaluate(0.31f), 1.0f, kTol);
}

// ---------------------------------------------------------------------------
// 9-11. Logistic (steepness=10): midpoint=0.5, near-0 at 0, near-1 at 1
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurveExtended, Logistic_Steepness10_HalfIsPreciselyHalf)
{
    // 1 / (1 + exp(-10*(0.5-0.5))) = 0.5 exactly
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "logistic", "steepness": 10.0 })");
    EXPECT_NEAR(curve.Evaluate(0.5f), 0.5f, kTol);
}

TEST(DiaUtilityAI_ResponseCurveExtended, Logistic_Steepness10_ZeroIsNearZero)
{
    // 1 / (1 + exp(5)) ≈ 0.0067
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "logistic", "steepness": 10.0 })");
    EXPECT_LT(curve.Evaluate(0.0f), 0.05f);
}

TEST(DiaUtilityAI_ResponseCurveExtended, Logistic_Steepness10_OneIsNearOne)
{
    // 1 / (1 + exp(-5)) ≈ 0.9933
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "logistic", "steepness": 10.0 })");
    EXPECT_GT(curve.Evaluate(1.0f), 0.95f);
}

// ---------------------------------------------------------------------------
// 12. Invert + quadratic (exp=2): at 0.5, output = 1 - 0.25 = 0.75
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurveExtended, Invert_Quadratic_Exp2_AtHalf)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "quadratic", "exponent": 2.0, "invert": true })");
    EXPECT_NEAR(curve.Evaluate(0.5f), 0.75f, kTol);
}

// ---------------------------------------------------------------------------
// 13-14. Invert + step (threshold=0.4): flips 0→1 and 1→0
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurveExtended, Invert_Step_BelowThreshold_ReturnsOne)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "step", "threshold": 0.4, "invert": true })");
    EXPECT_NEAR(curve.Evaluate(0.39f), 1.0f, kTol);
}

TEST(DiaUtilityAI_ResponseCurveExtended, Invert_Step_AboveThreshold_ReturnsZero)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "step", "threshold": 0.4, "invert": true })");
    EXPECT_NEAR(curve.Evaluate(0.41f), 0.0f, kTol);
}

// ---------------------------------------------------------------------------
// 15. Exponential: higher k produces a steeper (lower at x=0.5) curve
//     k=2: (e^1-1)/(e^2-1) ≈ 0.269; k=8: (e^4-1)/(e^8-1) ≈ 0.018
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurveExtended, Exponential_HigherK_LowerValueAtMidpoint)
{
    Dia::UtilityAI::ResponseCurve curveK2 = LoadCurve(R"({ "shape": "exponential", "k": 2.0 })");
    Dia::UtilityAI::ResponseCurve curveK8 = LoadCurve(R"({ "shape": "exponential", "k": 8.0 })");

    const float vK2 = curveK2.Evaluate(0.5f);
    const float vK8 = curveK8.Evaluate(0.5f);

    // Higher k concentrates growth near x=1 → lower value at midpoint
    EXPECT_LT(vK8, vK2);
    EXPECT_GT(vK2, 0.0f);
    EXPECT_LT(vK8, 0.1f);
}

// ---------------------------------------------------------------------------
// 16. Logistic: higher steepness produces a sharper transition at x=0.25
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurveExtended, Logistic_HigherSteepness_SharperTransitionBelowMid)
{
    // steepness=5 at x=0.25: 1/(1+exp(1.25)) ≈ 0.223
    // steepness=20 at x=0.25: 1/(1+exp(5))   ≈ 0.007
    Dia::UtilityAI::ResponseCurve curveSlow = LoadCurve(R"({ "shape": "logistic", "steepness":  5.0 })");
    Dia::UtilityAI::ResponseCurve curveFast = LoadCurve(R"({ "shape": "logistic", "steepness": 20.0 })");

    EXPECT_LT(curveFast.Evaluate(0.25f), curveSlow.Evaluate(0.25f));
}

// ---------------------------------------------------------------------------
// 17. Quadratic with exponent=1.0 behaves identically to linear
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurveExtended, Quadratic_Exponent1_BehavesLikeLinear)
{
    Dia::UtilityAI::ResponseCurve curve = LoadCurve(R"({ "shape": "quadratic", "exponent": 1.0 })");
    const float pts[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
    for (float x : pts)
        EXPECT_NEAR(curve.Evaluate(x), x, kTol) << "exponent=1 should behave like linear at x=" << x;
}

// ---------------------------------------------------------------------------
// 18. Zero-range normalisation guard: inputMin == inputMax must not divide by
//     zero; normInput clamps to 0 so the linear curve returns 0.0 and the
//     action is not selected.
// ---------------------------------------------------------------------------

TEST(DiaUtilityAI_ResponseCurveExtended, ZeroRangeNormalisation_DoesNotCrash_ActionScoresZero)
{
    // inputMin == inputMax == 50.0 → range = 0. Guard sets normInput = 0.
    // Linear curve(0) = 0.0 → action not eligible → no selection.
    const char* kZeroRangeSet = R"({
        "actions": [
            {
                "id": "Attack",
                "scorers": [
                    { "slot": "health", "field": "value",
                      "input_min": 50.0, "input_max": 50.0,
                      "curve": { "shape": "linear" } }
                ]
            }
        ]
    })";

    Dia::UtilityAI::UtilitySet set = LoadSet(kZeroRangeSet);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("health"), Dia::Core::StringCRC("value"), 50.0f);

    Dia::UtilityAI::UtilitySelection result = set.SelectWinner(ctx, nullptr, nullptr);

    // No crash and no eligible winner (score collapsed to 0)
    EXPECT_EQ(result.actionId, Dia::Core::StringCRC());
    EXPECT_NEAR(result.score, 0.0f, kTol);
}
