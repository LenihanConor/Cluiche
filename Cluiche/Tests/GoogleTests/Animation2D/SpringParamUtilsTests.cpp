#include <gtest/gtest.h>

#include <DiaAnimation2D/SpringParamUtils.h>

#include <cmath>

// ============================================================
// SpringParamUtils tests
// ============================================================

TEST(SpringParamUtils, StiffnessFormula_KnownValues)
{
    // f=1 Hz, mass=1 kg -> k = (2*pi*f)^2 * m = (2*pi)^2 ≈ 39.4784
    Dia::Animation2D::SpringParams p = Dia::Animation2D::SpringParamsFromFrequency(1.0f, 0.0f, 1.0f);
    EXPECT_NEAR(p.stiffness, 39.4784f, 1e-3f);
}

TEST(SpringParamUtils, DampingFormula_KnownValues)
{
    // f=1, ratio=0.5, m=1 -> d = 2 * ratio * sqrt(k * m) = 2 * 0.5 * 2*pi ≈ 6.2832
    Dia::Animation2D::SpringParams p = Dia::Animation2D::SpringParamsFromFrequency(1.0f, 0.5f, 1.0f);
    EXPECT_NEAR(p.damping, 6.2832f, 1e-3f);
}

TEST(SpringParamUtils, DefaultMassOne_MatchesExplicitMassOne)
{
    Dia::Animation2D::SpringParams pDefault  = Dia::Animation2D::SpringParamsFromFrequency(2.5f, 0.7f);
    Dia::Animation2D::SpringParams pExplicit = Dia::Animation2D::SpringParamsFromFrequency(2.5f, 0.7f, 1.0f);

    EXPECT_NEAR(pDefault.stiffness, pExplicit.stiffness, 1e-5f);
    EXPECT_NEAR(pDefault.damping,   pExplicit.damping,   1e-5f);
}

#ifdef _DEBUG
TEST(SpringParamUtils, InvalidFrequency_Zero_Asserts)
{
    EXPECT_DEATH(Dia::Animation2D::SpringParamsFromFrequency(0.0f, 0.5f, 1.0f), "");
}

TEST(SpringParamUtils, InvalidFrequency_Negative_Asserts)
{
    EXPECT_DEATH(Dia::Animation2D::SpringParamsFromFrequency(-1.0f, 0.5f, 1.0f), "");
}

TEST(SpringParamUtils, InvalidDampingRatio_Negative_Asserts)
{
    EXPECT_DEATH(Dia::Animation2D::SpringParamsFromFrequency(1.0f, -0.1f, 1.0f), "");
}

TEST(SpringParamUtils, InvalidMass_Zero_Asserts)
{
    EXPECT_DEATH(Dia::Animation2D::SpringParamsFromFrequency(1.0f, 0.5f, 0.0f), "");
}
#endif // _DEBUG

TEST(SpringParamUtils, CriticallyDamped_GoldenValues)
{
    // f=3 Hz, ratio=1.0, m=2 kg
    // k = (2*pi*3)^2 * 2 = 4*pi^2*9*2 ≈ 710.6
    // d = 2 * 1.0 * sqrt(k * m) = 2 * sqrt(710.6 * 2) ≈ 75.4
    Dia::Animation2D::SpringParams p = Dia::Animation2D::SpringParamsFromFrequency(3.0f, 1.0f, 2.0f);
    EXPECT_NEAR(p.stiffness, 710.6f, 0.1f);
    EXPECT_NEAR(p.damping,    75.4f, 0.1f);
}
