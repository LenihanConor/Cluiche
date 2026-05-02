#include <gtest/gtest.h>
#include <DiaMaths/Core/MathsDefines.h>
#include <DiaMaths/Core/CoreMaths.h>
#include <DiaMaths/Core/Interpolation.h>

using namespace Dia::Maths;

static const float kEps = 1e-4f;

//=============================================================================
// 1. Lerp golden values
//=============================================================================
TEST(DiaMaths_Interpolation_Golden, Lerp_Basic)
{
	EXPECT_NEAR(Lerp(0.0f, 100.0f, 0.0f), 0.0f, kEps);
	EXPECT_NEAR(Lerp(0.0f, 100.0f, 0.5f), 50.0f, kEps);
	EXPECT_NEAR(Lerp(0.0f, 100.0f, 1.0f), 100.0f, kEps);
}

TEST(DiaMaths_Interpolation_Golden, Lerp_Clamped)
{
	EXPECT_NEAR(Lerp(0.0f, 100.0f, -0.5f), 0.0f, kEps);
	EXPECT_NEAR(Lerp(0.0f, 100.0f, 1.5f), 100.0f, kEps);
}

TEST(DiaMaths_Interpolation_Golden, Lerp_NegativeRange)
{
	EXPECT_NEAR(Lerp(-50.0f, 50.0f, 0.5f), 0.0f, kEps);
}

//=============================================================================
// 2. LerpUnclamped golden values
//=============================================================================
TEST(DiaMaths_Interpolation_Golden, LerpUnclamped_Basic)
{
	EXPECT_NEAR(LerpUnclamped(0.0f, 100.0f, 0.0f), 0.0f, kEps);
	EXPECT_NEAR(LerpUnclamped(0.0f, 100.0f, 0.5f), 50.0f, kEps);
	EXPECT_NEAR(LerpUnclamped(0.0f, 100.0f, 1.0f), 100.0f, kEps);
}

TEST(DiaMaths_Interpolation_Golden, LerpUnclamped_Extrapolation)
{
	EXPECT_NEAR(LerpUnclamped(0.0f, 100.0f, 1.5f), 150.0f, kEps);
	EXPECT_NEAR(LerpUnclamped(0.0f, 100.0f, -0.5f), -50.0f, kEps);
}

//=============================================================================
// 3. InverseLerp golden values
//=============================================================================
TEST(DiaMaths_Interpolation_Golden, InverseLerp_Basic)
{
	EXPECT_NEAR(InverseLerp(0.0f, 100.0f, 25.0f), 0.25f, kEps);
	EXPECT_NEAR(InverseLerp(0.0f, 100.0f, 0.0f), 0.0f, kEps);
	EXPECT_NEAR(InverseLerp(0.0f, 100.0f, 100.0f), 1.0f, kEps);
}

TEST(DiaMaths_Interpolation_Golden, InverseLerp_DivisionByZeroGuard)
{
	EXPECT_NEAR(InverseLerp(50.0f, 50.0f, 50.0f), 0.0f, kEps);
}

//=============================================================================
// 4. SmoothStep golden values
//=============================================================================
TEST(DiaMaths_Interpolation_Golden, SmoothStep_Basic)
{
	EXPECT_NEAR(SmoothStep(0.0f), 0.0f, kEps);
	EXPECT_NEAR(SmoothStep(1.0f), 1.0f, kEps);
	EXPECT_NEAR(SmoothStep(0.5f), 0.5f, kEps);
}

TEST(DiaMaths_Interpolation_Golden, SmoothStep_Quarter)
{
	// 0.25^2 * (3 - 2*0.25) = 0.0625 * 2.5 = 0.15625
	EXPECT_NEAR(SmoothStep(0.25f), 0.15625f, kEps);
}

TEST(DiaMaths_Interpolation_Golden, SmoothStep_Clamped)
{
	EXPECT_NEAR(SmoothStep(-1.0f), 0.0f, kEps);
	EXPECT_NEAR(SmoothStep(2.0f), 1.0f, kEps);
}

//=============================================================================
// 5. SmootherStep golden values
//=============================================================================
TEST(DiaMaths_Interpolation_Golden, SmootherStep_Basic)
{
	EXPECT_NEAR(SmootherStep(0.0f), 0.0f, kEps);
	EXPECT_NEAR(SmootherStep(1.0f), 1.0f, kEps);
	EXPECT_NEAR(SmootherStep(0.5f), 0.5f, kEps);
}

TEST(DiaMaths_Interpolation_Golden, SmootherStep_Quarter)
{
	// t=0.25: t^3*(6t^2 - 15t + 10) = 0.015625*(0.375 - 3.75 + 10) = 0.015625*6.625 = 0.103516
	EXPECT_NEAR(SmootherStep(0.25f), 0.103516f, kEps);
}

//=============================================================================
// 6. SmoothLerp and SmootherLerp
//=============================================================================
TEST(DiaMaths_Interpolation_Golden, SmoothLerp_Midpoint)
{
	// SmoothStep(0.5) = 0.5, so SmoothLerp(0, 100, 0.5) = 50
	EXPECT_NEAR(SmoothLerp(0.0f, 100.0f, 0.5f), 50.0f, kEps);
}

TEST(DiaMaths_Interpolation_Golden, SmootherLerp_Midpoint)
{
	// SmootherStep(0.5) = 0.5, so SmootherLerp(0, 100, 0.5) = 50
	EXPECT_NEAR(SmootherLerp(0.0f, 100.0f, 0.5f), 50.0f, kEps);
}

//=============================================================================
// 7. MoveTowards
//=============================================================================
TEST(DiaMaths_Interpolation_Golden, MoveTowards_PartialStep)
{
	EXPECT_NEAR(MoveTowards(0.0f, 10.0f, 3.0f), 3.0f, kEps);
}

TEST(DiaMaths_Interpolation_Golden, MoveTowards_Overshoot)
{
	// maxDelta > distance: should snap to target
	EXPECT_NEAR(MoveTowards(0.0f, 10.0f, 15.0f), 10.0f, kEps);
}

TEST(DiaMaths_Interpolation_Golden, MoveTowards_AlreadyAtTarget)
{
	EXPECT_NEAR(MoveTowards(5.0f, 5.0f, 1.0f), 5.0f, kEps);
}

TEST(DiaMaths_Interpolation_Golden, MoveTowards_Backward)
{
	EXPECT_NEAR(MoveTowards(10.0f, 0.0f, 3.0f), 7.0f, kEps);
}

//=============================================================================
// 8. Remap golden values
//=============================================================================
TEST(DiaMaths_Interpolation_Golden, Remap_Basic)
{
	EXPECT_NEAR(Remap(0.5f, 0.0f, 1.0f, 10.0f, 20.0f), 15.0f, kEps);
	EXPECT_NEAR(Remap(5.0f, 0.0f, 10.0f, 100.0f, 200.0f), 150.0f, kEps);
}

TEST(DiaMaths_Interpolation_Golden, Remap_Extrapolation)
{
	// Value beyond input range: extrapolates
	EXPECT_NEAR(Remap(15.0f, 0.0f, 10.0f, 100.0f, 200.0f), 250.0f, kEps);
}

//=============================================================================
// 9. RemapClamped golden values
//=============================================================================
TEST(DiaMaths_Interpolation_Golden, RemapClamped_Basic)
{
	EXPECT_NEAR(RemapClamped(0.5f, 0.0f, 1.0f, 10.0f, 20.0f), 15.0f, kEps);
}

TEST(DiaMaths_Interpolation_Golden, RemapClamped_Above)
{
	EXPECT_NEAR(RemapClamped(15.0f, 0.0f, 10.0f, 100.0f, 200.0f), 200.0f, kEps);
}

TEST(DiaMaths_Interpolation_Golden, RemapClamped_Below)
{
	EXPECT_NEAR(RemapClamped(-5.0f, 0.0f, 10.0f, 100.0f, 200.0f), 100.0f, kEps);
}

//=============================================================================
// 10. Invariant -- Lerp/InverseLerp roundtrip
//=============================================================================
TEST(DiaMaths_Interpolation_Golden, LerpInverseLerp_Roundtrip)
{
	const float a = 10.0f;
	const float b = 80.0f;
	const float tValues[] = { 0.0f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 1.0f };

	for (float t : tValues)
	{
		float lerped = Lerp(a, b, t);
		float recovered = InverseLerp(a, b, lerped);
		EXPECT_NEAR(recovered, t, kEps) << "Roundtrip failed for t=" << t;
	}
}
