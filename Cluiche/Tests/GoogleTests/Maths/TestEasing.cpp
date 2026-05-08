#include <gtest/gtest.h>
#include <DiaMaths/Core/MathsDefines.h>
#include <DiaMaths/Core/CoreMaths.h>
#include <DiaMaths/Core/FloatMaths.h>
#include <DiaMaths/Core/Trigonometry.h>
#include <DiaMaths/Core/Easing.h>
#include <cmath>

using namespace Dia::Maths;

typedef float (*EasingFn)(float);

struct EasingEntry
{
	const char* name;
	EasingFn fn;
};

static const EasingEntry kAllFunctions[] = {
	{ "Linear",           Easing::Linear },
	{ "EaseInQuad",       Easing::EaseInQuad },
	{ "EaseOutQuad",      Easing::EaseOutQuad },
	{ "EaseInOutQuad",    Easing::EaseInOutQuad },
	{ "EaseInCubic",      Easing::EaseInCubic },
	{ "EaseOutCubic",     Easing::EaseOutCubic },
	{ "EaseInOutCubic",   Easing::EaseInOutCubic },
	{ "EaseInQuart",      Easing::EaseInQuart },
	{ "EaseOutQuart",     Easing::EaseOutQuart },
	{ "EaseInOutQuart",   Easing::EaseInOutQuart },
	{ "EaseInQuint",      Easing::EaseInQuint },
	{ "EaseOutQuint",     Easing::EaseOutQuint },
	{ "EaseInOutQuint",   Easing::EaseInOutQuint },
	{ "EaseInSine",       Easing::EaseInSine },
	{ "EaseOutSine",      Easing::EaseOutSine },
	{ "EaseInOutSine",    Easing::EaseInOutSine },
	{ "EaseInExpo",       Easing::EaseInExpo },
	{ "EaseOutExpo",      Easing::EaseOutExpo },
	{ "EaseInOutExpo",    Easing::EaseInOutExpo },
	{ "EaseInCirc",       Easing::EaseInCirc },
	{ "EaseOutCirc",      Easing::EaseOutCirc },
	{ "EaseInOutCirc",    Easing::EaseInOutCirc },
	{ "EaseInBack",       Easing::EaseInBack },
	{ "EaseOutBack",      Easing::EaseOutBack },
	{ "EaseInOutBack",    Easing::EaseInOutBack },
	{ "EaseInElastic",    Easing::EaseInElastic },
	{ "EaseOutElastic",   Easing::EaseOutElastic },
	{ "EaseInOutElastic", Easing::EaseInOutElastic },
	{ "EaseInBounce",     Easing::EaseInBounce },
	{ "EaseOutBounce",    Easing::EaseOutBounce },
	{ "EaseInOutBounce",  Easing::EaseInOutBounce },
};

static const int kNumFunctions = sizeof(kAllFunctions) / sizeof(kAllFunctions[0]);

// ---------------------------------------------------------------------------
// 1. Endpoint tests
// ---------------------------------------------------------------------------

TEST(DiaMaths_Easing_Golden, AllFunctions_AtZero_ReturnZero)
{
	for (int i = 0; i < kNumFunctions; ++i)
	{
		EXPECT_NEAR(kAllFunctions[i].fn(0.0f), 0.0f, 1e-5f) << kAllFunctions[i].name;
	}
}

TEST(DiaMaths_Easing_Golden, AllFunctions_AtOne_ReturnOne)
{
	for (int i = 0; i < kNumFunctions; ++i)
	{
		EXPECT_NEAR(kAllFunctions[i].fn(1.0f), 1.0f, 1e-5f) << kAllFunctions[i].name;
	}
}

// ---------------------------------------------------------------------------
// 2. Midpoint golden values
// ---------------------------------------------------------------------------

TEST(DiaMaths_Easing_Golden, Linear_Midpoint_MatchesAnalytical)
{
	EXPECT_NEAR(Easing::Linear(0.5f), 0.5f, 1e-5f);
}

TEST(DiaMaths_Easing_Golden, Quadratic_Midpoint_MatchesAnalytical)
{
	EXPECT_NEAR(Easing::EaseInQuad(0.5f),    0.25f, 1e-5f);
	EXPECT_NEAR(Easing::EaseOutQuad(0.5f),   0.75f, 1e-5f);
	EXPECT_NEAR(Easing::EaseInOutQuad(0.5f), 0.5f,  1e-5f);
}

TEST(DiaMaths_Easing_Golden, Cubic_Midpoint_MatchesAnalytical)
{
	EXPECT_NEAR(Easing::EaseInCubic(0.5f),    0.125f, 1e-5f);
	EXPECT_NEAR(Easing::EaseOutCubic(0.5f),   0.875f, 1e-5f);
	EXPECT_NEAR(Easing::EaseInOutCubic(0.5f), 0.5f,   1e-5f);
}

TEST(DiaMaths_Easing_Golden, Quartic_Midpoint_MatchesAnalytical)
{
	EXPECT_NEAR(Easing::EaseInQuart(0.5f),    0.0625f,  1e-5f);
	EXPECT_NEAR(Easing::EaseOutQuart(0.5f),   0.9375f,  1e-5f);
	EXPECT_NEAR(Easing::EaseInOutQuart(0.5f), 0.5f,     1e-5f);
}

TEST(DiaMaths_Easing_Golden, Quintic_Midpoint_MatchesAnalytical)
{
	EXPECT_NEAR(Easing::EaseInQuint(0.5f),    0.03125f,  1e-5f);
	EXPECT_NEAR(Easing::EaseOutQuint(0.5f),   0.96875f,  1e-5f);
	EXPECT_NEAR(Easing::EaseInOutQuint(0.5f), 0.5f,      1e-5f);
}

TEST(DiaMaths_Easing_Golden, Sine_Midpoint_MatchesAnalytical)
{
	EXPECT_NEAR(Easing::EaseInSine(0.5f),    0.29289f, 1e-4f);
	EXPECT_NEAR(Easing::EaseOutSine(0.5f),   0.70711f, 1e-4f);
	EXPECT_NEAR(Easing::EaseInOutSine(0.5f), 0.5f,     1e-5f);
}

TEST(DiaMaths_Easing_Golden, Exponential_Midpoint_MatchesAnalytical)
{
	EXPECT_NEAR(Easing::EaseInExpo(0.5f),    0.03125f,  1e-4f);
	EXPECT_NEAR(Easing::EaseOutExpo(0.5f),   0.96875f,  1e-4f);
	EXPECT_NEAR(Easing::EaseInOutExpo(0.5f), 0.5f,      1e-4f);
}

TEST(DiaMaths_Easing_Golden, Circular_Midpoint_MatchesAnalytical)
{
	EXPECT_NEAR(Easing::EaseInCirc(0.5f),    0.13397f,  1e-4f);
	EXPECT_NEAR(Easing::EaseOutCirc(0.5f),   0.86603f,  1e-4f);
	EXPECT_NEAR(Easing::EaseInOutCirc(0.5f), 0.5f,      1e-4f);
}

TEST(DiaMaths_Easing_Golden, Back_Midpoint_MatchesAnalytical)
{
	// InBack: t*t*((s+1)*t-s), s=1.70158 => 0.25*(1.35079-1.70158) = -0.087698
	EXPECT_NEAR(Easing::EaseInBack(0.5f),    -0.087698f, 1e-4f);
	// OutBack: symmetric => 1.087698
	EXPECT_NEAR(Easing::EaseOutBack(0.5f),    1.087698f, 1e-4f);
	EXPECT_NEAR(Easing::EaseInOutBack(0.5f),  0.5f,      1e-4f);
}

TEST(DiaMaths_Easing_Golden, Elastic_Midpoint_MatchesAnalytical)
{
	// InElastic at 0.5: -(pow(2,-5)*sin((-0.5-0.075)*2PI/0.3)) = 0.015625
	EXPECT_NEAR(Easing::EaseInElastic(0.5f),   -0.015625f, 1e-4f);
	// OutElastic at 0.5: pow(2,-5)*sin((0.5-0.075)*2PI/0.3)+1 = 1.015625
	EXPECT_NEAR(Easing::EaseOutElastic(0.5f),   1.015625f, 1e-4f);
	// InOutElastic at 0.5: boundary case evaluates to 0.5
	EXPECT_NEAR(Easing::EaseInOutElastic(0.5f), 0.5f,      1e-4f);
}

TEST(DiaMaths_Easing_Golden, Bounce_Midpoint_MatchesAnalytical)
{
	// InBounce(0.5) = 1 - OutBounce(0.5) = 1 - 0.765625 = 0.234375
	EXPECT_NEAR(Easing::EaseInBounce(0.5f),    0.234375f,  1e-4f);
	// OutBounce(0.5): second branch, t-=1.5/2.75, 7.5625*t*t+0.75 = 0.765625
	EXPECT_NEAR(Easing::EaseOutBounce(0.5f),   0.765625f,  1e-4f);
	EXPECT_NEAR(Easing::EaseInOutBounce(0.5f), 0.5f,       1e-4f);
}

// ---------------------------------------------------------------------------
// 3. Monotonicity test for well-behaved EaseIn/EaseOut variants
// ---------------------------------------------------------------------------

TEST(DiaMaths_Easing_Golden, MonotonicEaseIn_IncreasingOverFullRange)
{
	EasingFn monotonic[] = {
		Easing::EaseInQuad, Easing::EaseInCubic, Easing::EaseInQuart,
		Easing::EaseInQuint, Easing::EaseInSine, Easing::EaseInExpo,
		Easing::EaseInCirc,
	};
	const char* names[] = {
		"EaseInQuad", "EaseInCubic", "EaseInQuart",
		"EaseInQuint", "EaseInSine", "EaseInExpo",
		"EaseInCirc",
	};

	for (int f = 0; f < 7; ++f)
	{
		float prev = monotonic[f](0.0f);
		for (int i = 1; i <= 10; ++i)
		{
			float t = i / 10.0f;
			float val = monotonic[f](t);
			EXPECT_GE(val, prev) << names[f] << " at t=" << t;
			prev = val;
		}
	}
}

TEST(DiaMaths_Easing_Golden, MonotonicEaseOut_IncreasingOverFullRange)
{
	EasingFn monotonic[] = {
		Easing::EaseOutQuad, Easing::EaseOutCubic, Easing::EaseOutQuart,
		Easing::EaseOutQuint, Easing::EaseOutSine, Easing::EaseOutExpo,
		Easing::EaseOutCirc,
	};
	const char* names[] = {
		"EaseOutQuad", "EaseOutCubic", "EaseOutQuart",
		"EaseOutQuint", "EaseOutSine", "EaseOutExpo",
		"EaseOutCirc",
	};

	for (int f = 0; f < 7; ++f)
	{
		float prev = monotonic[f](0.0f);
		for (int i = 1; i <= 10; ++i)
		{
			float t = i / 10.0f;
			float val = monotonic[f](t);
			EXPECT_GE(val, prev) << names[f] << " at t=" << t;
			prev = val;
		}
	}
}

// ---------------------------------------------------------------------------
// 4. Overshoot verification
// ---------------------------------------------------------------------------

TEST(DiaMaths_Easing_Golden, EaseInBack_GoesNegative)
{
	bool foundNeg = false;
	for (int i = 0; i <= 100; ++i)
	{
		float t = i / 100.0f;
		if (Easing::EaseInBack(t) < 0.0f)
		{
			foundNeg = true;
			break;
		}
	}
	EXPECT_TRUE(foundNeg);
}

TEST(DiaMaths_Easing_Golden, EaseOutBack_ExceedsOne)
{
	bool foundOver = false;
	for (int i = 0; i <= 100; ++i)
	{
		float t = i / 100.0f;
		if (Easing::EaseOutBack(t) > 1.0f)
		{
			foundOver = true;
			break;
		}
	}
	EXPECT_TRUE(foundOver);
}

TEST(DiaMaths_Easing_Golden, EaseInElastic_GoesNegative)
{
	bool foundNeg = false;
	for (int i = 0; i <= 100; ++i)
	{
		float t = i / 100.0f;
		if (Easing::EaseInElastic(t) < 0.0f)
		{
			foundNeg = true;
			break;
		}
	}
	EXPECT_TRUE(foundNeg);
}

TEST(DiaMaths_Easing_Golden, EaseOutElastic_ExceedsOne)
{
	bool foundOver = false;
	for (int i = 0; i <= 100; ++i)
	{
		float t = i / 100.0f;
		if (Easing::EaseOutElastic(t) > 1.0f)
		{
			foundOver = true;
			break;
		}
	}
	EXPECT_TRUE(foundOver);
}
