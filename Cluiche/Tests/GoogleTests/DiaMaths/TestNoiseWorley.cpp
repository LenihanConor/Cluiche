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
		float jitter;
	};

	// Negative, zero, large (beyond the +-2^23 clamp), lattice-aligned and fractional coordinates; several seeds and jitters.
	const Sample kSamples[] = {
		{ 0u,          0.0f,        0.0f,       1.0f },
		{ 0u,          0.5f,        0.5f,       1.0f },
		{ 1u,          0.25f,       0.75f,      0.5f },
		{ 1u,         -0.5f,       -0.5f,       0.0f },
		{ 12345u,      3.0f,        4.0f,       0.0f },   // lattice, regular grid
		{ 12345u,     -3.0f,       -4.0f,       1.0f },   // negative lattice
		{ 12345u,      3.7f,       -4.2f,       0.75f },
		{ 0xDEADBEEFu, 100.123f,    200.456f,   1.0f },
		{ 0xDEADBEEFu,-100.123f,   -200.456f,   0.25f },
		{ 0xFFFFFFFFu, 0.1f,        0.9f,       1.0f },
		{ 7u,          1.0f,        0.0f,       0.5f },
		{ 7u,          0.0f,        1.0f,       1.0f },
		{ 7u,         -0.001f,      0.001f,     1.0f },
		{ 42u,         1000.5f,     -1000.5f,   0.5f },
		{ 42u,         8388608.0f,  8388608.0f, 1.0f },   // 2^23, at the clamp
		{ 42u,         1.0e9f,     -1.0e9f,     1.0f },   // beyond the clamp
		{ 99u,         123456.789f, 0.333f,     1.0f },
		{ 99u,        -7.999f,      7.999f,     0.1f },
	};
	const int kSampleCount = static_cast<int>(sizeof(kSamples) / sizeof(kSamples[0]));

	// Golden bit patterns (captured from the first GREEN run, then reviewed for range and variety).
	const uint32_t kGoldenF1[18]    = { 0x3EB6720Du, 0x3EA92674u, 0x3E8835C1u, 0x3F3504F3u, 0x00000000u, 0x3EDEA6DCu, 0x3EE57EC4u, 0x3E538226u, 0x3EBEDBB6u, 0x3EE5C40Eu, 0x3E9154F6u, 0x3F1D2836u, 0x3E9C0931u, 0x3EF5A266u, 0x3E70C4B8u, 0x3F73F559u, 0x3EC251EBu, 0x3CF35551u };
	const uint32_t kGoldenF2[18]    = { 0x3F0FD751u, 0x3F134A2Fu, 0x3EFBB592u, 0x3F3504F3u, 0x3F800000u, 0x3F512013u, 0x3F0B3E26u, 0x3F372947u, 0x3F1D9DEFu, 0x3EF05F30u, 0x3F541770u, 0x3F236892u, 0x3ECF0455u, 0x3F80B3F2u, 0x3EC9F980u, 0x3F750218u, 0x3F0F856Du, 0x3F6FFFAFu };
	const uint32_t kGoldenF2mF1[18] = { 0x3E52792Au, 0x3E7ADBD4u, 0x3E66FFA2u, 0x00000000u, 0x3F800000u, 0x3EC3994Au, 0x3DC3F620u, 0x3F0248BEu, 0x3E78C050u, 0x3CA9B220u, 0x3F0B6CF5u, 0x3CC80B80u, 0x3DCBEC90u, 0x3F0696B1u, 0x3E232E48u, 0x3B865F80u, 0x3E3971DEu, 0x3F686504u };
	const uint32_t kGoldenId[18]    = { 0x097A3CD7u, 0x0CC68DD6u, 0x263781E5u, 0xFFDD039Bu, 0x386D0F7Du, 0x9D45D19Au, 0x30DFE86Du, 0xA1769693u, 0x056B0BE5u, 0xAAB12841u, 0x4EEFC8EEu, 0x208F78B2u, 0xADADA58Cu, 0x387F4483u, 0x0E085C4Du, 0x3F2682A8u, 0xEFD4D40Bu, 0x7D04CD4Au };

	// Reference feature-point distance: identical formula to the implementation (same per-cell feature function, same operation order).
	float RefSquaredDistance(uint32_t seed, int cx, int cy, int ix, int iy, float fx, float fy, float jitter)
	{
		const uint32_t h = NoiseDetail::WorleyCellHash(seed, cx, cy);
		const float dx = (static_cast<float>(cx - ix) + jitter * NoiseDetail::WorleyJitterX(h)) - fx;
		const float dy = (static_cast<float>(cy - iy) + jitter * NoiseDetail::WorleyJitterY(h)) - fy;
		return dx * dx + dy * dy;
	}

	// Brute force over a (2*radius+1)^2 window.
	void BruteForce(uint32_t seed, float x, float y, float jitter, int radius, float& f1, float& f2, uint32_t& id)
	{
		const int ix = NoiseDetail::FloorToInt(x);
		const int iy = NoiseDetail::FloorToInt(y);
		const float fx = x - static_cast<float>(ix);
		const float fy = y - static_cast<float>(iy);
		float d1 = 3.0e38f, d2 = 3.0e38f;
		uint32_t best = 0u;
		for (int cy = iy - radius; cy <= iy + radius; ++cy)
		{
			for (int cx = ix - radius; cx <= ix + radius; ++cx)
			{
				const float d = RefSquaredDistance(seed, cx, cy, ix, iy, fx, fy, jitter);
				if (d < d1) { d2 = d1; d1 = d; best = NoiseDetail::WorleyCellHash(seed, cx, cy); }
				else if (d < d2) { d2 = d; }
			}
		}
		f1 = sqrtf(d1);
		f2 = sqrtf(d2);
		id = NoiseDetail::WorleyCellId(best);
	}
}

//-----------------------------------------------------------------------------
// AC1: golden bit patterns
//-----------------------------------------------------------------------------

TEST(SLOW_NoiseWorley, GoldenValues)
{
	ASSERT_GE(kSampleCount, 16);
	for (int i = 0; i < kSampleCount; ++i)
	{
		const WorleyResult r = WorleyNoise2D(kSamples[i].seed, kSamples[i].x, kSamples[i].y, kSamples[i].jitter);
		EXPECT_EQ(kGoldenF1[i], Bits(r.f1)) << "f1 sample " << i;
		EXPECT_EQ(kGoldenF2[i], Bits(r.f2)) << "f2 sample " << i;
		EXPECT_EQ(kGoldenF2mF1[i], Bits(r.f2MinusF1)) << "f2MinusF1 sample " << i;
		EXPECT_EQ(kGoldenId[i], r.cellId) << "cellId sample " << i;
	}
}

//-----------------------------------------------------------------------------
// AC2: determinism
//-----------------------------------------------------------------------------

TEST(SLOW_NoiseWorley, Determinism)
{
	for (int i = 0; i < kSampleCount; ++i)
	{
		const Sample& s = kSamples[i];
		const WorleyResult r0 = WorleyNoise2D(s.seed, s.x, s.y, s.jitter);
		for (int n = 0; n < 1000; ++n)
		{
			const WorleyResult r = WorleyNoise2D(s.seed, s.x, s.y, s.jitter);
			ASSERT_EQ(Bits(r0.f1), Bits(r.f1));
			ASSERT_EQ(Bits(r0.f2), Bits(r.f2));
			ASSERT_EQ(Bits(r0.f2MinusF1), Bits(r.f2MinusF1));
			ASSERT_EQ(r0.cellId, r.cellId);
			ASSERT_EQ(Bits(r0.cellValue), Bits(r.cellValue));
		}
	}
}

//-----------------------------------------------------------------------------
// AC3: bounds, AC9: ordering
//-----------------------------------------------------------------------------

TEST(SLOW_NoiseWorley, BoundsAndOrdering)
{
	RandomStream rng(2024);
	const float jitters[] = { 0.0f, 0.5f, 1.0f };
	for (int i = 0; i < 100000; ++i)
	{
		const uint32_t seed = static_cast<uint32_t>(rng.RandomInt());
		const float x = rng.RandomRange(-500.0f, 500.0f);
		const float y = rng.RandomRange(-500.0f, 500.0f);
		const WorleyResult r = WorleyNoise2D(seed, x, y, jitters[i % 3]);
		ASSERT_GE(r.f1, 0.0f);
		ASSERT_LE(r.f1, r.f2);
		ASSERT_LT(r.f1, 1.4143f); // the own cell always holds a candidate within sqrt(2)
		ASSERT_EQ(Bits(r.f2 - r.f1), Bits(r.f2MinusF1));
		ASSERT_GE(r.f2MinusF1, 0.0f);
		ASSERT_GE(r.cellValue, 0.0f);
		ASSERT_LT(r.cellValue, 1.0f);
		ASSERT_EQ(Bits(NoiseDetail::HashToUnit(r.cellId)), Bits(r.cellValue));
	}
}

//-----------------------------------------------------------------------------
// AC4: distribution sanity
//-----------------------------------------------------------------------------

TEST(SLOW_NoiseWorley, Distribution)
{
	RandomStream rng(555);
	const int n = 65536;
	double sumF1 = 0.0;
	int bins[16] = {};
	for (int i = 0; i < n; ++i)
	{
		const float x = rng.RandomRange(-1000.0f, 1000.0f);
		const float y = rng.RandomRange(-1000.0f, 1000.0f);
		const WorleyResult r = WorleyNoise2D(17u, x, y, 1.0f);
		sumF1 += r.f1;
		++bins[static_cast<int>(r.cellValue * 16.0f)];
	}
	const double meanF1 = sumF1 / n;
	double chi2 = 0.0;
	const double expected = n / 16.0;
	for (int b = 0; b < 16; ++b)
	{
		chi2 += (bins[b] - expected) * (bins[b] - expected) / expected;
	}
	// Measured (seed 17, 65,536 samples): meanF1 = 0.4307, chi2 = 13.6 (15 dof). Bands: meanF1 in [0.41, 0.45]; chi2 < 37.7 (p = 0.001 at 15 dof).
	EXPECT_GT(meanF1, 0.41);
	EXPECT_LT(meanF1, 0.45);
	EXPECT_LT(chi2, 37.7);
}

//-----------------------------------------------------------------------------
// AC5: seed sensitivity
//-----------------------------------------------------------------------------

TEST(SLOW_NoiseWorley, SeedSensitivity)
{
	int diffF1 = 0, diffId = 0;
	for (int i = 0; i < 1024; ++i)
	{
		const float x = 0.37f + 0.731f * static_cast<float>(i % 32) - 11.0f;
		const float y = 0.21f + 0.613f * static_cast<float>(i / 32) - 9.0f;
		const WorleyResult a = WorleyNoise2D(1u, x, y);
		const WorleyResult b = WorleyNoise2D(2u, x, y);
		if (a.f1 != b.f1) ++diffF1;
		if (a.cellId != b.cellId) ++diffId;
	}
	EXPECT_GE(diffF1, 973); // 95% of 1024
	EXPECT_GE(diffId, 973);
}

//-----------------------------------------------------------------------------
// AC7: continuity of f1, cellId changes only across Voronoi borders
//-----------------------------------------------------------------------------

TEST(SLOW_NoiseWorley, Continuity)
{
	// f1 is 1-Lipschitz in the sample position; allow a few ulps of coordinate rounding (|coord| < 64 -> ulp 7.6e-6).
	const float d = 0.001f;
	const float tol = d + 2e-5f;
	for (int i = -3000; i < 3000; ++i)
	{
		const float t = static_cast<float>(i) * 0.0037f;
		const float u = static_cast<float>(i) * 0.0021f - 2.5f;
		const float a = WorleyNoise2D(77u, t, u).f1;
		ASSERT_LE(std::fabs(WorleyNoise2D(77u, t + d, u).f1 - a), tol);
		ASSERT_LE(std::fabs(WorleyNoise2D(77u, t, u + d).f1 - a), tol);
	}

	// Walk along lines; every cellId change must happen where f2MinusF1 is near zero (a Voronoi border).
	int changes = 0;
	for (int line = 0; line < 20; ++line)
	{
		const float y = -9.7f + 1.013f * static_cast<float>(line);
		WorleyResult prev = WorleyNoise2D(5u, -20.0f, y);
		for (int i = 1; i < 40000; ++i)
		{
			const float x = -20.0f + static_cast<float>(i) * 0.001f;
			const WorleyResult cur = WorleyNoise2D(5u, x, y);
			if (cur.cellId != prev.cellId)
			{
				++changes;
				ASSERT_LT(prev.f2MinusF1, 2.0f * d + 1e-4f) << "line " << line << " x " << x;
				ASSERT_LT(cur.f2MinusF1, 2.0f * d + 1e-4f) << "line " << line << " x " << x;
			}
			prev = cur;
		}
	}
	EXPECT_GT(changes, 100);
}

//-----------------------------------------------------------------------------
// AC9: brute-force 9 x 9 reference, bit-exact
//-----------------------------------------------------------------------------

TEST(SLOW_NoiseWorley, MatchesBruteForce9x9)
{
	RandomStream rng(31337);
	const float jitters[] = { 0.0f, 0.5f, 1.0f };
	for (float jitter : jitters)
	{
		for (int i = 0; i < 100000; ++i)
		{
			const uint32_t seed = static_cast<uint32_t>(rng.RandomInt());
			const float x = rng.RandomRange(-500.0f, 500.0f);
			const float y = rng.RandomRange(-500.0f, 500.0f);
			const WorleyResult r = WorleyNoise2D(seed, x, y, jitter);
			float f1, f2;
			uint32_t id;
			BruteForce(seed, x, y, jitter, 4, f1, f2, id);
			ASSERT_EQ(Bits(f1), Bits(r.f1)) << "jitter " << jitter << " sample " << i;
			ASSERT_EQ(Bits(f2), Bits(r.f2)) << "jitter " << jitter << " sample " << i;
			if (jitter == 1.0f)
			{
				ASSERT_EQ(id, r.cellId) << "sample " << i;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// AC10: jitter behaviour and cell ids
//-----------------------------------------------------------------------------

TEST(SLOW_NoiseWorley, ZeroJitterIsRegularGrid)
{
	const uint32_t seed = 31337u;
	for (int iy = -8; iy <= 8; ++iy)
	{
		for (int ix = -8; ix <= 8; ++ix)
		{
			const WorleyResult r = WorleyNoise2D(seed, static_cast<float>(ix), static_cast<float>(iy), 0.0f);
			EXPECT_EQ(0.0f, r.f1) << ix << "," << iy;
			EXPECT_EQ(1.0f, r.f2) << ix << "," << iy;
			EXPECT_EQ(NoiseDetail::WorleyCellId(NoiseDetail::Hash2D(seed, ix, iy)), r.cellId) << ix << "," << iy;
		}
	}
}

TEST(SLOW_NoiseWorley, FullJitterMovesFeaturePoints)
{
	const uint32_t seed = 9u;
	int movedF1 = 0;
	std::set<uint32_t> ids;
	for (int iy = -8; iy <= 8; ++iy)
	{
		for (int ix = -8; ix <= 8; ++ix)
		{
			if (WorleyNoise2D(seed, static_cast<float>(ix), static_cast<float>(iy), 1.0f).f1 > 0.0f) ++movedF1;
			ids.insert(WorleyNoise2D(seed, static_cast<float>(ix) + 0.5f, static_cast<float>(iy) + 0.5f, 1.0f).cellId);
		}
	}
	EXPECT_GE(movedF1, 270); // 289 lattice points, essentially none keeps a feature point exactly on it
	EXPECT_GE(ids.size(), 200u);
}

TEST(SLOW_NoiseWorley, CellIdIsPerFeaturePoint)
{
	const uint32_t seed = 4242u;
	for (int cy = -5; cy <= 5; ++cy)
	{
		for (int cx = -5; cx <= 5; ++cx)
		{
			const uint32_t h = NoiseDetail::WorleyCellHash(seed, cx, cy);
			const float px = static_cast<float>(cx) + NoiseDetail::WorleyJitterX(h);
			const float py = static_cast<float>(cy) + NoiseDetail::WorleyJitterY(h);
			const uint32_t id = NoiseDetail::WorleyCellId(h);

			const WorleyResult at = WorleyNoise2D(seed, px, py, 1.0f);
			EXPECT_LT(at.f1, 1e-4f);
			EXPECT_EQ(id, at.cellId);

			// Points very close to the feature point share its id.
			EXPECT_EQ(id, WorleyNoise2D(seed, px + 0.01f, py - 0.01f, 1.0f).cellId);
			EXPECT_EQ(id, WorleyNoise2D(seed, px - 0.01f, py + 0.01f, 1.0f).cellId);

			// The neighbouring cell's feature point has a different id.
			const uint32_t hn = NoiseDetail::WorleyCellHash(seed, cx + 1, cy);
			const float nx = static_cast<float>(cx + 1) + NoiseDetail::WorleyJitterX(hn);
			const float ny = static_cast<float>(cy) + NoiseDetail::WorleyJitterY(hn);
			EXPECT_NE(id, WorleyNoise2D(seed, nx, ny, 1.0f).cellId);
		}
	}
}
