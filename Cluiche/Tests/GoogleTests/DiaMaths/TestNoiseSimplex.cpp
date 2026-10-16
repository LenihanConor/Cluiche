#include <gtest/gtest.h>
#include <DiaMaths/Noise/Noise.h>
#include <DiaMaths/Noise/NoiseHash.h>
#include <DiaMaths/Core/RandomStream.h>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

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
	const uint32_t kGoldenSimplex[18] = { 0x8EDE5FBBu,0xBD611AB7u,0xBD945222u,0xBE9A277Bu,0x3F4E358Au,0xBE19632Fu,0xBE899089u,0x3F3CB46Au,0xBEC644D2u,0x3EDA0415u,0x3EEA2B05u,0x3F4A4A99u,0xBBA58D28u,0x3DE03272u,0x1001A434u,0x10923BCFu,0x3E5EAEEDu,0xBAFF959Du };

	// AC7 Lipschitz bound: measured max slope is about 3.8; 8.0 leaves 2x headroom (same ceiling as Value/Gradient).
	const float kSimplexLipschitz = 8.0f;
	// AC4 band: measured std dev is about 0.42 (mean about 0.0001); band is +-0.07 around it.
	const double kStdDevMin = 0.35, kStdDevMax = 0.49;
	// AC11 rotation bound: measured std dev delta between 0 and 45 degrees is about 0.003; bound is 0.02.
	const double kRotationStdDevDelta = 0.02;

	void MeanStd(double sum, double sumSq, int n, double& mean, double& sd)
	{
		mean = sum / n;
		sd = std::sqrt(sumSq / n - mean * mean);
	}
}

TEST(NoiseSimplex, Golden)
{
	ASSERT_GE(kSampleCount, 16);
	for (int i = 0; i < kSampleCount; ++i)
	{
		const float v = SimplexNoise2D(kSamples[i].seed, kSamples[i].x, kSamples[i].y);
		EXPECT_TRUE(v >= -1.0f && v <= 1.0f) << "sample " << i << " out of range or NaN";
		EXPECT_EQ(kGoldenSimplex[i], Bits(v)) << "sample " << i;
	}
}

TEST(NoiseSimplex, Determinism)
{
	for (int i = 0; i < kSampleCount; ++i)
	{
		const Sample& s = kSamples[i];
		const uint32_t b0 = Bits(SimplexNoise2D(s.seed, s.x, s.y));
		for (int n = 0; n < 1000; ++n)
		{
			ASSERT_EQ(b0, Bits(SimplexNoise2D(s.seed, s.x, s.y)));
		}
	}
}

TEST(NoiseSimplex, Range)
{
	RandomStream rng(2024);
	for (int i = 0; i < 100000; ++i)
	{
		const uint32_t seed = static_cast<uint32_t>(rng.RandomInt());
		const float x = rng.RandomRange(-500.0f, 500.0f);
		const float y = rng.RandomRange(-500.0f, 500.0f);
		const float v = SimplexNoise2D(seed, x, y);
		ASSERT_GE(v, -1.0f);
		ASSERT_LE(v, 1.0f);
	}
}

TEST(NoiseSimplex, Distribution)
{
	double sum = 0, sumSq = 0;
	const int n = 65536;
	for (int j = 0; j < 256; ++j)
	{
		for (int i = 0; i < 256; ++i)
		{
			const double v = SimplexNoise2D(17u, static_cast<float>(i) * 0.37f - 40.0f, static_cast<float>(j) * 0.41f - 50.0f);
			sum += v; sumSq += v * v;
		}
	}
	double mean, sd;
	MeanStd(sum, sumSq, n, mean, sd);
	std::printf("[measure] simplex mean=%.5f sd=%.5f\n", mean, sd);
	EXPECT_NEAR(0.0, mean, 0.03);
	EXPECT_GT(sd, kStdDevMin);
	EXPECT_LT(sd, kStdDevMax);
}

TEST(NoiseSimplex, SeedSensitivity)
{
	int diff = 0;
	for (int i = 0; i < 1024; ++i)
	{
		const float x = 0.37f + 0.731f * static_cast<float>(i % 32) - 11.0f;
		const float y = 0.21f + 0.613f * static_cast<float>(i / 32) - 9.0f;
		if (SimplexNoise2D(1u, x, y) != SimplexNoise2D(2u, x, y)) ++diff;
		EXPECT_EQ(SimplexNoise2D(5u, x, y), SimplexNoise2D(5u, x, y));
	}
	EXPECT_GE(diff, 973); // 95% of 1024
}

TEST(NoiseSimplex, Continuity)
{
	const float d = 0.001f;
	float maxSlope = 0.0f;
	for (int i = -3000; i < 3000; ++i)
	{
		const float t = static_cast<float>(i) * 0.0037f;
		const float u = static_cast<float>(i) * 0.0021f - 2.5f;
		const uint32_t seed = 77u;
		const float dx = std::fabs(SimplexNoise2D(seed, t + d, u) - SimplexNoise2D(seed, t, u));
		const float dy = std::fabs(SimplexNoise2D(seed, t, u + d) - SimplexNoise2D(seed, t, u));
		maxSlope = std::fmax(maxSlope, std::fmax(dx, dy) / d);
	}
	std::printf("[measure] simplex max slope=%.4f\n", maxSlope);
	EXPECT_LT(maxSlope, kSimplexLipschitz);

	// Straddle cell borders (integers and the skewed-lattice diagonals) at 0 and negatives.
	const float borders[] = { -1.0f, 0.0f, 1.0f };
	for (float b : borders)
	{
		EXPECT_LT(std::fabs(SimplexNoise2D(3u, b - 1e-4f, 0.3f) - SimplexNoise2D(3u, b + 1e-4f, 0.3f)), kSimplexLipschitz * 2e-4f);
		EXPECT_LT(std::fabs(SimplexNoise2D(3u, 0.3f, b - 1e-4f) - SimplexNoise2D(3u, 0.3f, b + 1e-4f)), kSimplexLipschitz * 2e-4f);
		EXPECT_LT(std::fabs(SimplexNoise2D(3u, b - 1e-4f, -b - 1e-4f) - SimplexNoise2D(3u, b + 1e-4f, -b + 1e-4f)), kSimplexLipschitz * 4e-4f);
	}
}

// AC11: normaliser verified (measured maximum >= 0.9) and no diagonal artefact.
TEST(NoiseSimplex, NormaliserMaxReachesRange)
{
	float maxV = -1.0f, minV = 1.0f;
	for (int j = 0; j < 700; ++j)
	{
		for (int i = 0; i < 700; ++i)
		{
			const float v = SimplexNoise2D(3u, static_cast<float>(i) * 0.0713f - 25.0f, static_cast<float>(j) * 0.0691f - 25.0f);
			maxV = std::fmax(maxV, v);
			minV = std::fmin(minV, v);
		}
	}
	std::printf("[measure] simplex max=%.5f min=%.5f\n", maxV, minV);
	EXPECT_GE(maxV, 0.9f);
	EXPECT_LE(minV, -0.9f);
}

TEST(NoiseSimplex, RotationDoesNotChangeStdDev)
{
	const float c = 0.70710678f; // cos/sin 45 degrees
	double s0 = 0, q0 = 0, s1 = 0, q1 = 0;
	const int n = 256 * 256;
	for (int j = 0; j < 256; ++j)
	{
		for (int i = 0; i < 256; ++i)
		{
			const float x = static_cast<float>(i) * 0.093f - 12.0f;
			const float y = static_cast<float>(j) * 0.087f - 11.0f;
			const double a = SimplexNoise2D(21u, x, y);
			const double b = SimplexNoise2D(21u, (x - y) * c, (x + y) * c);
			s0 += a; q0 += a * a; s1 += b; q1 += b * b;
		}
	}
	double m0, sd0, m1, sd1;
	MeanStd(s0, q0, n, m0, sd0);
	MeanStd(s1, q1, n, m1, sd1);
	std::printf("[measure] simplex sd=%.5f rotated sd=%.5f delta=%.5f\n", sd0, sd1, std::fabs(sd0 - sd1));
	EXPECT_LT(std::fabs(sd0 - sd1), kRotationStdDevDelta);
}
