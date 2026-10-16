#include <gtest/gtest.h>
#include <DiaMaths/Noise/Noise.h>
#include <DiaMaths/Noise/NoiseHash.h>
#include <DiaMaths/Core/RandomStream.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <set>

using namespace Dia::Maths;

namespace
{
	uint32_t Bits(float f)
	{
		uint32_t u;
		std::memcpy(&u, &f, sizeof(u));
		return u;
	}

	struct Sample
	{
		uint32_t seed;
		float x;
		float y;
	};

	// Negative, zero, large (beyond the +-2^23 clamp), lattice-aligned and fractional coordinates; several seeds.
	const Sample kSamples[] = {
		{ 0u,          0.0f,        0.0f },
		{ 0u,          0.5f,        0.5f },
		{ 1u,          0.25f,       0.75f },
		{ 1u,         -0.5f,       -0.5f },
		{ 12345u,      3.0f,        4.0f },       // lattice
		{ 12345u,     -3.0f,       -4.0f },       // negative lattice
		{ 12345u,      3.7f,       -4.2f },
		{ 0xDEADBEEFu, 100.123f,    200.456f },
		{ 0xDEADBEEFu,-100.123f,   -200.456f },
		{ 0xFFFFFFFFu, 0.1f,        0.9f },
		{ 7u,          1.0f,        0.0f },
		{ 7u,          0.0f,        1.0f },
		{ 7u,         -0.001f,      0.001f },
		{ 42u,         1000.5f,     -1000.5f },
		{ 42u,         8388608.0f,  8388608.0f }, // 2^23, at the clamp
		{ 42u,         1.0e9f,     -1.0e9f },     // beyond the clamp
		{ 99u,         123456.789f, 0.333f },
		{ 99u,        -7.999f,      7.999f },
	};
	const int kSampleCount = static_cast<int>(sizeof(kSamples) / sizeof(kSamples[0]));

	// Golden bit patterns (captured from the first GREEN run, then reviewed for range and variety).
	const uint32_t kGoldenValue[18] = { 0x3F26B5C0u, 0xBD77FFD0u, 0xBE99CA89u, 0xBEA85F05u, 0xBF490EF8u, 0x3F3F6E74u, 0xBD78BDB0u, 0x3EEF4188u, 0x3E7FCFB8u, 0xBEA03B68u, 0xBF490140u, 0xBDF14CC0u, 0x3E56324Au, 0xBE0F9B50u, 0xBF0A55FAu, 0xBF755860u, 0xBEEE7923u, 0xBF7B877Au };
	const uint32_t kGoldenGradient[18] = { 0x00000000u, 0xBEA73D74u, 0xBEB5D908u, 0x3CB7E3E6u, 0x00000000u, 0x00000000u, 0x3E905B80u, 0xBE74D220u, 0x3EEBA662u, 0xBE3784B4u, 0x00000000u, 0x00000000u, 0x3A48A146u, 0x3DDDF9AAu, 0x00000000u, 0x00000000u, 0xBCFE579Bu, 0x3AB96502u };

	const float kValueLipschitz = 8.0f;     // |dV/dx| <= 2 (lattice delta) * 1.875 (max fade slope) per axis, summed over 2 axes < 8
	const float kGradientLipschitz = 8.0f;  // measured max is far below this; bound is a safe analytical ceiling
}

//-----------------------------------------------------------------------------
// AC1: golden bit patterns
//-----------------------------------------------------------------------------

TEST(NoiseValueGradient, GoldenValue)
{
	ASSERT_GE(kSampleCount, 16);
	for (int i = 0; i < kSampleCount; ++i)
	{
		const float v = ValueNoise2D(kSamples[i].seed, kSamples[i].x, kSamples[i].y);
		EXPECT_TRUE(v >= -1.0f && v <= 1.0f) << "sample " << i << " out of range or NaN";
		EXPECT_EQ(kGoldenValue[i], Bits(v)) << "sample " << i;
	}
}

TEST(NoiseValueGradient, GoldenGradient)
{
	ASSERT_GE(kSampleCount, 16);
	for (int i = 0; i < kSampleCount; ++i)
	{
		const float g = GradientNoise2D(kSamples[i].seed, kSamples[i].x, kSamples[i].y);
		EXPECT_TRUE(g >= -1.0f && g <= 1.0f) << "sample " << i << " out of range or NaN";
		EXPECT_EQ(kGoldenGradient[i], Bits(g)) << "sample " << i;
	}
}

//-----------------------------------------------------------------------------
// AC2: determinism
//-----------------------------------------------------------------------------

TEST(NoiseValueGradient, Determinism)
{
	for (int i = 0; i < kSampleCount; ++i)
	{
		const Sample& s = kSamples[i];
		const uint32_t v0 = Bits(ValueNoise2D(s.seed, s.x, s.y));
		const uint32_t g0 = Bits(GradientNoise2D(s.seed, s.x, s.y));
		for (int n = 0; n < 1000; ++n)
		{
			ASSERT_EQ(v0, Bits(ValueNoise2D(s.seed, s.x, s.y)));
			ASSERT_EQ(g0, Bits(GradientNoise2D(s.seed, s.x, s.y)));
		}
	}
}

//-----------------------------------------------------------------------------
// AC3: range
//-----------------------------------------------------------------------------

TEST(NoiseValueGradient, Range)
{
	RandomStream rng(2024);
	float minV = 1.0f, maxV = -1.0f, minG = 1.0f, maxG = -1.0f;
	for (int i = 0; i < 100000; ++i)
	{
		const uint32_t seed = static_cast<uint32_t>(rng.RandomInt());
		const float x = rng.RandomRange(-500.0f, 500.0f);
		const float y = rng.RandomRange(-500.0f, 500.0f);
		const float v = ValueNoise2D(seed, x, y);
		const float g = GradientNoise2D(seed, x, y);
		ASSERT_GE(v, -1.0f); ASSERT_LE(v, 1.0f);
		ASSERT_GE(g, -1.0f); ASSERT_LE(g, 1.0f);
		minV = std::fmin(minV, v); maxV = std::fmax(maxV, v);
		minG = std::fmin(minG, g); maxG = std::fmax(maxG, g);
	}
	// No wasted range: both types reach well beyond half scale.
	EXPECT_GT(maxV, 0.8f); EXPECT_LT(minV, -0.8f);
	EXPECT_GT(maxG, 0.6f); EXPECT_LT(minG, -0.6f);
}

//-----------------------------------------------------------------------------
// AC5: seed sensitivity
//-----------------------------------------------------------------------------

TEST(NoiseValueGradient, SeedSensitivity)
{
	int diffV = 0, diffG = 0;
	for (int i = 0; i < 1024; ++i)
	{
		const float x = 0.37f + 0.731f * static_cast<float>(i % 32) - 11.0f;
		const float y = 0.21f + 0.613f * static_cast<float>(i / 32) - 9.0f;
		if (ValueNoise2D(1u, x, y) != ValueNoise2D(2u, x, y)) ++diffV;
		if (GradientNoise2D(1u, x, y) != GradientNoise2D(2u, x, y)) ++diffG;
		EXPECT_EQ(ValueNoise2D(5u, x, y), ValueNoise2D(5u, x, y));
		EXPECT_EQ(GradientNoise2D(5u, x, y), GradientNoise2D(5u, x, y));
	}
	EXPECT_GE(diffV, 973); // 95% of 1024
	EXPECT_GE(diffG, 973);
}

//-----------------------------------------------------------------------------
// AC6: lattice behaviour
//-----------------------------------------------------------------------------

TEST(NoiseValueGradient, LatticeBehaviour)
{
	int nonZeroValue = 0;
	for (int iy = -8; iy <= 8; ++iy)
	{
		for (int ix = -8; ix <= 8; ++ix)
		{
			const uint32_t seed = 31337u;
			const float x = static_cast<float>(ix);
			const float y = static_cast<float>(iy);
			EXPECT_EQ(0.0f, GradientNoise2D(seed, x, y)) << ix << "," << iy;

			const float expected = NoiseDetail::HashToSigned(NoiseDetail::Hash2D(seed, ix, iy));
			EXPECT_EQ(Bits(expected), Bits(ValueNoise2D(seed, x, y))) << ix << "," << iy;
			if (ValueNoise2D(seed, x, y) != 0.0f) ++nonZeroValue;
		}
	}
	EXPECT_GT(nonZeroValue, 280); // 289 lattice points, essentially all non-zero
}

//-----------------------------------------------------------------------------
// AC7: continuity (including cell borders, zero and negative coordinates)
//-----------------------------------------------------------------------------

TEST(NoiseValueGradient, Continuity)
{
	const float d = 0.001f;
	float maxSlopeV = 0.0f, maxSlopeG = 0.0f;
	// Walk across x and y in fine steps spanning negative, zero and positive cells (borders at integers).
	for (int i = -3000; i < 3000; ++i)
	{
		const float t = static_cast<float>(i) * 0.0037f;
		const float u = static_cast<float>(i) * 0.0021f - 2.5f;
		const uint32_t seed = 77u;
		const float dvx = std::fabs(ValueNoise2D(seed, t + d, u) - ValueNoise2D(seed, t, u));
		const float dvy = std::fabs(ValueNoise2D(seed, t, u + d) - ValueNoise2D(seed, t, u));
		const float dgx = std::fabs(GradientNoise2D(seed, t + d, u) - GradientNoise2D(seed, t, u));
		const float dgy = std::fabs(GradientNoise2D(seed, t, u + d) - GradientNoise2D(seed, t, u));
		maxSlopeV = std::fmax(maxSlopeV, std::fmax(dvx, dvy) / d);
		maxSlopeG = std::fmax(maxSlopeG, std::fmax(dgx, dgy) / d);
	}
	EXPECT_LT(maxSlopeV, kValueLipschitz);
	EXPECT_LT(maxSlopeG, kGradientLipschitz);

	// Straddle the exact cell border at 0 and at -1 in both directions.
	const float borders[] = { -1.0f, 0.0f, 1.0f };
	for (float b : borders)
	{
		EXPECT_LT(std::fabs(ValueNoise2D(3u, b - 1e-4f, 0.3f) - ValueNoise2D(3u, b + 1e-4f, 0.3f)), kValueLipschitz * 2e-4f);
		EXPECT_LT(std::fabs(GradientNoise2D(3u, 0.3f, b - 1e-4f) - GradientNoise2D(3u, 0.3f, b + 1e-4f)), kGradientLipschitz * 2e-4f);
	}
}

//-----------------------------------------------------------------------------
// AC8: gradient set and axis bias
//-----------------------------------------------------------------------------

TEST(NoiseValueGradient, SixteenDistinctGradients)
{
	// Table: 16 unit vectors, all distinct.
	std::set<std::pair<float, float>> table;
	for (int i = 0; i < 16; ++i)
	{
		float gx, gy;
		NoiseDetail::Gradient2D(static_cast<uint32_t>(i) << 28, gx, gy);
		EXPECT_NEAR(1.0f, gx * gx + gy * gy, 1e-6f) << i;
		table.insert({ gx, gy });
	}
	EXPECT_EQ(16u, table.size());

	// Selection: hashes of 100,000 lattice points reach exactly 16 distinct gradients.
	std::set<int> used;
	for (int i = 0; i < 100000; ++i)
	{
		used.insert(static_cast<int>(NoiseDetail::GradientIndex2D(NoiseDetail::Hash2D(9u, i % 317, i / 317))));
	}
	EXPECT_EQ(16u, used.size());
}

TEST(NoiseValueGradient, AxisBias)
{
	// Autocorrelation at lag 0.3 along x vs along y over a large grid; they must agree.
	const float lag = 0.3f;
	double sumXX = 0, sumXY = 0, sumYY = 0, sumYX = 0, sumAA = 0;
	for (int j = 0; j < 160; ++j)
	{
		for (int i = 0; i < 160; ++i)
		{
			const float x = static_cast<float>(i) * 0.173f - 13.0f;
			const float y = static_cast<float>(j) * 0.167f - 13.0f;
			const double a = GradientNoise2D(11u, x, y);
			const double bx = GradientNoise2D(11u, x + lag, y);
			const double by = GradientNoise2D(11u, x, y + lag);
			sumAA += a * a;
			sumXY += a * bx;
			sumYX += a * by;
		}
	}
	(void)sumXX; (void)sumYY;
	const double corrX = sumXY / sumAA;
	const double corrY = sumYX / sumAA;
	EXPECT_GT(corrX, 0.3);
	EXPECT_GT(corrY, 0.3);
	EXPECT_LT(std::fabs(corrX - corrY), 0.05);
}
