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

	struct Sample3
	{
		uint32_t seed;
		float x;
		float y;
		float z;
	};

	// Negative, zero, large (beyond the +-2^23 clamp), lattice-aligned and fractional coordinates; several seeds.
	const Sample3 kSamples[] = {
		{ 0u,          0.0f,        0.0f,        0.0f },
		{ 0u,          0.5f,        0.5f,        0.5f },
		{ 1u,          0.25f,       0.75f,       0.125f },
		{ 1u,         -0.5f,       -0.5f,       -0.5f },
		{ 12345u,      3.0f,        4.0f,        5.0f },       // lattice
		{ 12345u,     -3.0f,       -4.0f,       -5.0f },       // negative lattice
		{ 12345u,      3.7f,       -4.2f,        1.3f },
		{ 0xDEADBEEFu, 100.123f,    200.456f,    300.789f },
		{ 0xDEADBEEFu,-100.123f,   -200.456f,   -300.789f },
		{ 0xFFFFFFFFu, 0.1f,        0.9f,        0.5f },
		{ 7u,          1.0f,        0.0f,        0.0f },
		{ 7u,          0.0f,        1.0f,        0.0f },
		{ 7u,          0.0f,        0.0f,        1.0f },
		{ 7u,         -0.001f,      0.001f,     -0.001f },
		{ 42u,         1000.5f,     -1000.5f,    77.25f },
		{ 42u,         8388608.0f,  8388608.0f,  8388608.0f }, // 2^23, at the clamp
		{ 42u,         1.0e9f,     -1.0e9f,      1.0e9f },     // beyond the clamp
		{ 99u,         123456.789f, 0.333f,     -9.5f },
		{ 99u,        -7.999f,      7.999f,      0.001f },
	};
	const int kSampleCount = static_cast<int>(sizeof(kSamples) / sizeof(kSamples[0]));

	FractalParams MakeParams(int octaves, bool normalise)
	{
		FractalParams p;
		p.octaves = octaves;
		p.frequency = 1.3f;
		p.lacunarity = 2.0f;
		p.gain = 0.5f;
		p.normalise = normalise;
		return p;
	}

	float TotalAmplitude(const FractalParams& p)
	{
		return FractalMaxAmplitude(p);
	}

	// Golden bit patterns (captured from the first GREEN run, then reviewed for range and variety).
	const uint32_t kGoldenNoise3D[19] = { 0x00000000u, 0xBE000000u, 0xBEE79D30u, 0x3EC00000u, 0x00000000u, 0x00000000u, 0x3D047C5Au, 0x3EC7A742u, 0x3D9B8F50u, 0x3C5E3D20u, 0x00000000u, 0x00000000u, 0x00000000u, 0x3B031100u, 0xBB8C0000u, 0x00000000u, 0x00000000u, 0x3F102188u, 0xBB030C2Bu };
	const uint32_t kGoldenFractalNorm[19] = { 0x00000000u, 0xBBFE6584u, 0xBE54222Du, 0x3D3FBC1Du, 0x3E3C85BCu, 0xBD269651u, 0x3E96EE60u, 0x3E131AA3u, 0x3E19BC6Fu, 0x3E0480DAu, 0x3DD1053Du, 0xBD259F61u, 0x3C20B634u, 0x3B35AC22u, 0x3C15B833u, 0x00000000u, 0x00000000u, 0x3EB9ADA7u, 0xBEBB74E5u };   // octaves 4, normalise on
	const uint32_t kGoldenFractalRaw[19] = { 0x00000000u, 0xBC6E7F2Cu, 0xBEC6E00Au, 0x3DB3C05Bu, 0x3EB0BD60u, 0xBD9C2CECu, 0x3F0D7F7Au, 0x3E89E8F9u, 0x3E9020A8u, 0x3E787198u, 0x3E43F4E9u, 0xBD9B456Bu, 0x3C96AAD1u, 0x3BAA5160u, 0x3C8C5CB0u, 0x00000000u, 0x00000000u, 0x3F2E12CDu, 0xBF2FBD97u };    // octaves 4, normalise off

	// Documented Lipschitz ceiling for |dN/dx|, |dN/dy|, |dN/dz| (each axis): classic Perlin with edge gradients of
	// length sqrt(2) has a per-axis derivative bound at most about 4; the measured maximum is printed by ContinuityInAllAxes.
	const float kNoise3DLipschitz = 4.0f;   // measured max 2.20 (x), 1.57 (y), 1.46 (z)
}

//-----------------------------------------------------------------------------
// AC1: golden bit patterns
//-----------------------------------------------------------------------------

TEST(Noise3D, GoldenNoise3D)
{
	ASSERT_GE(kSampleCount, 16);
	for (int i = 0; i < kSampleCount; ++i)
	{
		const Sample3& s = kSamples[i];
		const float v = Noise3D(s.seed, s.x, s.y, s.z);
		EXPECT_TRUE(v >= -1.0f && v <= 1.0f) << "sample " << i << " out of range or NaN";
		EXPECT_EQ(kGoldenNoise3D[i], Bits(v)) << "sample " << i;
	}
}

TEST(Noise3D, GoldenFractal)
{
	for (int i = 0; i < kSampleCount; ++i)
	{
		const Sample3& s = kSamples[i];
		const float n = FractalNoise3D(s.seed, s.x, s.y, s.z, MakeParams(4, true));
		const float r = FractalNoise3D(s.seed, s.x, s.y, s.z, MakeParams(4, false));
		EXPECT_TRUE(n >= -1.0f && n <= 1.0f) << "sample " << i;
		EXPECT_EQ(kGoldenFractalNorm[i], Bits(n)) << "sample " << i;
		EXPECT_EQ(kGoldenFractalRaw[i], Bits(r)) << "sample " << i;
	}
}

//-----------------------------------------------------------------------------
// AC2: determinism
//-----------------------------------------------------------------------------

TEST(Noise3D, Determinism)
{
	for (int i = 0; i < kSampleCount; ++i)
	{
		const Sample3& s = kSamples[i];
		const uint32_t a = Bits(Noise3D(s.seed, s.x, s.y, s.z));
		const uint32_t b = Bits(FractalNoise3D(s.seed, s.x, s.y, s.z, MakeParams(5, true)));
		for (int n = 0; n < 1000; ++n)
		{
			ASSERT_EQ(a, Bits(Noise3D(s.seed, s.x, s.y, s.z)));
			ASSERT_EQ(b, Bits(FractalNoise3D(s.seed, s.x, s.y, s.z, MakeParams(5, true))));
		}
	}
}

//-----------------------------------------------------------------------------
// AC3: range
//-----------------------------------------------------------------------------

TEST(Noise3D, Range)
{
	RandomStream rng(2024);
	float minV = 1.0f, maxV = -1.0f, sum = 0.0f, sumSq = 0.0f;
	const FractalParams norm = MakeParams(6, true);
	const FractalParams raw = MakeParams(6, false);
	const float S = TotalAmplitude(raw);
	const int kCount = 100000;
	for (int i = 0; i < kCount; ++i)
	{
		const uint32_t seed = static_cast<uint32_t>(rng.RandomInt());
		const float x = rng.RandomRange(-500.0f, 500.0f);
		const float y = rng.RandomRange(-500.0f, 500.0f);
		const float z = rng.RandomRange(-500.0f, 500.0f);
		const float v = Noise3D(seed, x, y, z);
		ASSERT_GE(v, -1.0f); ASSERT_LE(v, 1.0f);
		minV = std::fmin(minV, v); maxV = std::fmax(maxV, v);
		sum += v; sumSq += v * v;

		const float fn = FractalNoise3D(seed, x, y, z, norm);
		ASSERT_GE(fn, -1.0f); ASSERT_LE(fn, 1.0f);
		const float fr = FractalNoise3D(seed, x, y, z, raw);
		ASSERT_GE(fr, -S); ASSERT_LE(fr, S);
	}
	const float mean = sum / static_cast<float>(kCount);
	const float sd = std::sqrt(sumSq / static_cast<float>(kCount) - mean * mean);
	std::printf("[Noise3D] range min=%f max=%f mean=%f sd=%f\n", minV, maxV, mean, sd);
	// No wasted range, roughly centred.
	EXPECT_GT(maxV, 0.6f); EXPECT_LT(minV, -0.6f);
	EXPECT_NEAR(0.0f, mean, 0.03f);
}

TEST(Noise3D, FractalNormaliseRanges)
{
	// Octaves 1..12, both flags, stay inside their documented range.
	RandomStream rng(77);
	for (int oct = 1; oct <= kMaxNoiseOctaves; ++oct)
	{
		const FractalParams norm = MakeParams(oct, true);
		const FractalParams raw = MakeParams(oct, false);
		const float S = TotalAmplitude(raw);
		for (int i = 0; i < 2000; ++i)
		{
			const uint32_t seed = static_cast<uint32_t>(rng.RandomInt());
			const float x = rng.RandomRange(-100.0f, 100.0f);
			const float y = rng.RandomRange(-100.0f, 100.0f);
			const float z = rng.RandomRange(-100.0f, 100.0f);
			const float n = FractalNoise3D(seed, x, y, z, norm);
			const float r = FractalNoise3D(seed, x, y, z, raw);
			ASSERT_GE(n, -1.0f); ASSERT_LE(n, 1.0f);
			ASSERT_GE(r, -S); ASSERT_LE(r, S);
			// normalised == unnormalised / S (within a couple of ulp).
			ASSERT_NEAR(r / S, n, 4.0e-7f + std::fabs(n) * 2.4e-7f);
		}
	}
}

TEST(Noise3D, FractalPrefixAdditivity)
{
	// normalise == false: the N+1-octave result is bit-identical to the N-octave result plus the next term.
	RandomStream rng(555);
	for (int i = 0; i < 500; ++i)
	{
		const uint32_t seed = static_cast<uint32_t>(rng.RandomInt());
		const float x = rng.RandomRange(-50.0f, 50.0f);
		const float y = rng.RandomRange(-50.0f, 50.0f);
		const float z = rng.RandomRange(-50.0f, 50.0f);

		float freq = 1.3f;
		float amp = 1.0f;
		for (int n = 0; n < kMaxNoiseOctaves; ++n)
		{
			const float term = amp * Noise3D(seed + static_cast<uint32_t>(n) * kOctaveSeedStride, x * freq, y * freq, z * freq);
			const float prev = (n == 0) ? 0.0f : FractalNoise3D(seed, x, y, z, MakeParams(n, false));
			const float next = FractalNoise3D(seed, x, y, z, MakeParams(n + 1, false));
			ASSERT_EQ(Bits(prev + term), Bits(next)) << "sample " << i << " octave " << n;
			freq *= 2.0f;
			amp *= 0.5f;
		}
	}
}

TEST(Noise3D, FractalOctaveOneEqualsBase)
{
	FractalParams p = MakeParams(1, true);
	p.frequency = 1.0f;
	for (int i = 0; i < kSampleCount; ++i)
	{
		const Sample3& s = kSamples[i];
		EXPECT_EQ(Bits(Noise3D(s.seed, s.x, s.y, s.z)), Bits(FractalNoise3D(s.seed, s.x, s.y, s.z, p))) << i;
	}
}

//-----------------------------------------------------------------------------
// AC5: seed sensitivity
//-----------------------------------------------------------------------------

TEST(Noise3D, SeedSensitivity)
{
	int diff = 0, diffF = 0;
	for (int i = 0; i < 1024; ++i)
	{
		const float x = 0.37f + 0.731f * static_cast<float>(i % 32) - 11.0f;
		const float y = 0.21f + 0.613f * static_cast<float>(i / 32) - 9.0f;
		const float z = 0.43f + 0.0173f * static_cast<float>(i);
		if (Noise3D(1u, x, y, z) != Noise3D(2u, x, y, z)) ++diff;
		if (FractalNoise3D(1u, x, y, z, MakeParams(3, true)) != FractalNoise3D(2u, x, y, z, MakeParams(3, true))) ++diffF;
		EXPECT_EQ(Noise3D(5u, x, y, z), Noise3D(5u, x, y, z));
	}
	EXPECT_GE(diff, 973); // 95% of 1024
	EXPECT_GE(diffF, 973);
}

//-----------------------------------------------------------------------------
// AC6: lattice behaviour
//-----------------------------------------------------------------------------

TEST(Noise3D, ExactlyZeroAtLatticePoints)
{
	for (int iz = -6; iz <= 6; ++iz)
	{
		for (int iy = -6; iy <= 6; ++iy)
		{
			for (int ix = -6; ix <= 6; ++ix)
			{
				const float v = Noise3D(31337u, static_cast<float>(ix), static_cast<float>(iy), static_cast<float>(iz));
				EXPECT_EQ(0.0f, v) << ix << "," << iy << "," << iz;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// AC12: 3D slice continuity
//-----------------------------------------------------------------------------

TEST(Noise3D, ContinuityInAllAxes)
{
	const float d = 0.001f;
	float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;
	for (int i = -3000; i < 3000; ++i)
	{
		const float t = static_cast<float>(i) * 0.0037f;
		const float u = static_cast<float>(i) * 0.0021f - 2.5f;
		const float w = static_cast<float>(i) * 0.0013f + 0.4f;
		const uint32_t seed = 77u;
		const float base = Noise3D(seed, t, u, w);
		maxX = std::fmax(maxX, std::fabs(Noise3D(seed, t + d, u, w) - base) / d);
		maxY = std::fmax(maxY, std::fabs(Noise3D(seed, t, u + d, w) - base) / d);
		maxZ = std::fmax(maxZ, std::fabs(Noise3D(seed, t, u, w + d) - base) / d);
	}
	std::printf("[Noise3D] measured Lipschitz x=%f y=%f z=%f (ceiling %f)\n", maxX, maxY, maxZ, kNoise3DLipschitz);
	EXPECT_LT(maxX, kNoise3DLipschitz);
	EXPECT_LT(maxY, kNoise3DLipschitz);
	EXPECT_LT(maxZ, kNoise3DLipschitz);

	// Straddle exact cell borders (including 0 and negatives) on each axis.
	const float borders[] = { -1.0f, 0.0f, 1.0f };
	for (float b : borders)
	{
		EXPECT_LT(std::fabs(Noise3D(3u, b - 1e-4f, 0.3f, 0.6f) - Noise3D(3u, b + 1e-4f, 0.3f, 0.6f)), kNoise3DLipschitz * 2e-4f);
		EXPECT_LT(std::fabs(Noise3D(3u, 0.3f, b - 1e-4f, 0.6f) - Noise3D(3u, 0.3f, b + 1e-4f, 0.6f)), kNoise3DLipschitz * 2e-4f);
		EXPECT_LT(std::fabs(Noise3D(3u, 0.3f, 0.6f, b - 1e-4f) - Noise3D(3u, 0.3f, 0.6f, b + 1e-4f)), kNoise3DLipschitz * 2e-4f);
	}
}

TEST(Noise3D, SlicesEvolveAndAreCorrelated)
{
	int differing = 0;
	for (int i = 0; i < 1024; ++i)
	{
		const float x = 0.37f + 0.231f * static_cast<float>(i % 32) - 3.0f;
		const float y = 0.21f + 0.213f * static_cast<float>(i / 32) - 3.0f;
		if (Noise3D(9u, x, y, 0.0f) != Noise3D(9u, x, y, 1.0f)) ++differing;
	}
	EXPECT_GT(differing, 900); // z = 0 slice is not the z = 1 slice

	// Pearson correlation of z and z + 0.001 over 1024 points.
	double sa = 0, sb = 0, saa = 0, sbb = 0, sab = 0;
	for (int i = 0; i < 1024; ++i)
	{
		const float x = 0.37f + 0.231f * static_cast<float>(i % 32) - 3.0f;
		const float y = 0.21f + 0.213f * static_cast<float>(i / 32) - 3.0f;
		const double a = Noise3D(9u, x, y, 0.5f);
		const double b = Noise3D(9u, x, y, 0.501f);
		sa += a; sb += b; saa += a * a; sbb += b * b; sab += a * b;
	}
	const double n = 1024.0;
	const double corr = (sab - sa * sb / n) / std::sqrt((saa - sa * sa / n) * (sbb - sb * sb / n));
	std::printf("[Noise3D] z vs z+0.001 correlation = %f\n", corr);
	EXPECT_GT(corr, 0.99);
}
