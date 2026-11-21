#include <gtest/gtest.h>
#include <DiaMaths/Noise/Noise.h>
#include <cfloat>
#include <climits>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

using namespace Dia::Maths;

// Non-finite and extreme inputs: every public noise function must stay finite, inside its documented range and deterministic.
// Behaviour under test (see Noise.h): NaN coordinates map to 0, +-inf and huge values clamp to +-2^23; non-finite
// frequency/lacunarity/gain fall back to the FractalParams defaults (1, 2, 0.5), |gain| is limited to 4; NaN/-inf jitter
// maps to 0 and +inf to 1; a non-finite warp strength maps to 0 and huge strengths clamp to +-2^23; octaves clamp to 1..12.

namespace
{
	uint32_t Bits(float f)
	{
		uint32_t u;
		std::memcpy(&u, &f, sizeof(u));
		return u;
	}

	// Bit-pattern finiteness (no <cmath>).
	bool IsFinite(float f)
	{
		return (Bits(f) & 0x7F800000u) != 0x7F800000u;
	}

	const std::vector<float>& Specials()
	{
		static const std::vector<float> v = {
			std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::signaling_NaN(),
			std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
			FLT_MAX, -FLT_MAX, 1.0e30f, -1.0e30f,
			FLT_MIN, -FLT_MIN, 1.0e-40f, -1.0e-40f, FLT_TRUE_MIN, -FLT_TRUE_MIN,
			0.0f, -0.0f, 8388608.0f, -8388608.0f, 16777216.0f, 0.5f, -0.5f,
		};
		return v;
	}

	// A few ordinary coordinates mixed in with the specials.
	const float kOrdinary[] = { 0.37f, -2.6f, 1234.567f };

	FractalParams Base(bool normalise)
	{
		FractalParams p;
		p.octaves = 4;
		p.frequency = 1.3f;
		p.normalise = normalise;
		return p;
	}

	void ExpectUnit(float v, const char* what, float x, float y)
	{
		EXPECT_TRUE(IsFinite(v)) << what << " non-finite at " << x << "," << y;
		EXPECT_GE(v, -1.0f) << what << " at " << x << "," << y;
		EXPECT_LE(v, 1.0f) << what << " at " << x << "," << y;
	}

	template <typename F>
	void ForAllCoords2(F&& fn)
	{
		std::vector<float> all = Specials();
		for (float o : kOrdinary) all.push_back(o);
		for (float x : all)
		{
			for (float y : all)
			{
				fn(x, y);
			}
		}
	}
}

TEST(NoiseEdgeCases, SingleOctave2DBases)
{
	ForAllCoords2([](float x, float y)
	{
		for (uint32_t seed : { 0u, 1u, 0xFFFFFFFFu })
		{
			const float v = ValueNoise2D(seed, x, y);
			const float g = GradientNoise2D(seed, x, y);
			const float s = SimplexNoise2D(seed, x, y);
			ExpectUnit(v, "Value", x, y);
			ExpectUnit(g, "Gradient", x, y);
			ExpectUnit(s, "Simplex", x, y);
			EXPECT_EQ(Bits(v), Bits(ValueNoise2D(seed, x, y)));
			EXPECT_EQ(Bits(g), Bits(GradientNoise2D(seed, x, y)));
			EXPECT_EQ(Bits(s), Bits(SimplexNoise2D(seed, x, y)));
		}
	});
}

TEST(NoiseEdgeCases, NanCoordinatesBehaveAsZero)
{
	const float nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_EQ(Bits(ValueNoise2D(5u, 0.0f, 0.0f)), Bits(ValueNoise2D(5u, nan, nan)));
	EXPECT_EQ(Bits(GradientNoise2D(5u, 0.0f, 3.5f)), Bits(GradientNoise2D(5u, nan, 3.5f)));
	EXPECT_EQ(Bits(SimplexNoise2D(5u, 2.5f, 0.0f)), Bits(SimplexNoise2D(5u, 2.5f, nan)));
	EXPECT_EQ(Bits(Noise3D(5u, 0.0f, 1.5f, 0.0f)), Bits(Noise3D(5u, nan, 1.5f, nan)));
	// +-inf clamp to +-2^23.
	const float inf = std::numeric_limits<float>::infinity();
	EXPECT_EQ(Bits(ValueNoise2D(5u, 8388608.0f, -8388608.0f)), Bits(ValueNoise2D(5u, inf, -inf)));
}

TEST(NoiseEdgeCases, Worley)
{
	std::vector<float> jitters = Specials();
	jitters.push_back(0.25f); jitters.push_back(1.0f); jitters.push_back(2.0f);
	ForAllCoords2([&](float x, float y)
	{
		for (float j : { 1.0f, 0.0f, std::numeric_limits<float>::quiet_NaN() })
		{
			const WorleyResult r = WorleyNoise2D(3u, x, y, j);
			EXPECT_TRUE(IsFinite(r.f1) && IsFinite(r.f2) && IsFinite(r.f2MinusF1)) << x << "," << y;
			EXPECT_GE(r.f1, 0.0f);
			EXPECT_LE(r.f1, r.f2);
			EXPECT_GE(r.f2MinusF1, 0.0f);
			EXPECT_GE(r.cellValue, 0.0f);
			EXPECT_LT(r.cellValue, 1.0f);
			const WorleyResult r2 = WorleyNoise2D(3u, x, y, j);
			EXPECT_EQ(Bits(r.f1), Bits(r2.f1));
			EXPECT_EQ(Bits(r.f2), Bits(r2.f2));
			EXPECT_EQ(r.cellId, r2.cellId);
		}
	});
	for (float j : jitters)
	{
		const WorleyResult r = WorleyNoise2D(3u, 0.37f, -2.6f, j);
		EXPECT_TRUE(IsFinite(r.f1) && IsFinite(r.f2) && IsFinite(r.f2MinusF1)) << "jitter " << j;
		EXPECT_GE(r.f1, 0.0f);
		EXPECT_LE(r.f1, r.f2);
		EXPECT_LT(r.cellValue, 1.0f);
	}
	// NaN jitter behaves as 0, +inf as 1.
	EXPECT_EQ(Bits(WorleyNoise2D(3u, 0.37f, 1.2f, 0.0f).f1), Bits(WorleyNoise2D(3u, 0.37f, 1.2f, std::numeric_limits<float>::quiet_NaN()).f1));
	EXPECT_EQ(Bits(WorleyNoise2D(3u, 0.37f, 1.2f, 1.0f).f1), Bits(WorleyNoise2D(3u, 0.37f, 1.2f, std::numeric_limits<float>::infinity()).f1));
}

TEST(NoiseEdgeCases, Noise3D)
{
	const std::vector<float>& sp = Specials();
	for (float x : sp)
	{
		for (float y : sp)
		{
			for (float z : sp)
			{
				const float v = Noise3D(9u, x, y, z);
				EXPECT_TRUE(IsFinite(v)) << x << "," << y << "," << z;
				EXPECT_GE(v, -1.0f);
				EXPECT_LE(v, 1.0f);
				EXPECT_EQ(Bits(v), Bits(Noise3D(9u, x, y, z)));
			}
		}
	}
	for (float o : kOrdinary)
	{
		ExpectUnit(Noise3D(9u, sp[0], o, sp[2]), "Noise3D", o, o);
	}
}

namespace
{
	template <NoiseType B>
	void CheckFractal2D(float x, float y, const FractalParams& p)
	{
		const float a = FractalNoise2D<B>(11u, x, y, p);
		EXPECT_TRUE(IsFinite(a)) << "fractal non-finite at " << x << "," << y;
		if (p.normalise)
		{
			EXPECT_GE(a, -1.0f);
			EXPECT_LE(a, 1.0f);
		}
		EXPECT_EQ(Bits(a), Bits(FractalNoise2D<B>(11u, x, y, p)));
	}

	template <NoiseType B>
	void CheckRidged(float x, float y, const RidgedParams& p)
	{
		const float a = RidgedNoise2D<B>(11u, x, y, p);
		EXPECT_TRUE(IsFinite(a)) << "ridged non-finite at " << x << "," << y;
		if (p.fractal.normalise)
		{
			EXPECT_GE(a, 0.0f);
			EXPECT_LE(a, 1.0f);
		}
		EXPECT_EQ(Bits(a), Bits(RidgedNoise2D<B>(11u, x, y, p)));
	}

	template <NoiseType W>
	void CheckWarp(float x, float y, float strength, const FractalParams& p)
	{
		const Warp2DResult w = Warp2D<W>(11u, x, y, strength, p);
		EXPECT_TRUE(IsFinite(w.x) && IsFinite(w.y)) << "warp non-finite at " << x << "," << y << " strength " << strength;
		const Warp2DResult w2 = Warp2D<W>(11u, x, y, strength, p);
		EXPECT_EQ(Bits(w.x), Bits(w2.x));
		EXPECT_EQ(Bits(w.y), Bits(w2.y));

		const float n = WarpedNoise2D<W, NoiseType::Gradient>(11u, x, y, strength, p, p);
		EXPECT_TRUE(IsFinite(n));
		if (p.normalise)
		{
			EXPECT_GE(n, -1.0f);
			EXPECT_LE(n, 1.0f);
		}
		EXPECT_EQ(Bits(n), Bits((WarpedNoise2D<W, NoiseType::Gradient>(11u, x, y, strength, p, p))));
	}

	void CheckAllFractals(float x, float y, const FractalParams& p)
	{
		CheckFractal2D<NoiseType::Value>(x, y, p);
		CheckFractal2D<NoiseType::Gradient>(x, y, p);
		CheckFractal2D<NoiseType::Simplex>(x, y, p);
		CheckFractal2D<NoiseType::Worley>(x, y, p);
		RidgedParams rp;
		rp.fractal = p;
		CheckRidged<NoiseType::Value>(x, y, rp);
		CheckRidged<NoiseType::Gradient>(x, y, rp);
		CheckRidged<NoiseType::Simplex>(x, y, rp);
		CheckWarp<NoiseType::Value>(x, y, 4.0f, p);
		CheckWarp<NoiseType::Gradient>(x, y, 4.0f, p);
		CheckWarp<NoiseType::Simplex>(x, y, 4.0f, p);
		CheckWarp<NoiseType::Worley>(x, y, 4.0f, p);

		const float f3 = FractalNoise3D(11u, x, y, 0.5f, p);
		EXPECT_TRUE(IsFinite(f3));
		if (p.normalise)
		{
			EXPECT_GE(f3, -1.0f);
			EXPECT_LE(f3, 1.0f);
		}
		EXPECT_EQ(Bits(f3), Bits(FractalNoise3D(11u, x, y, 0.5f, p)));
	}
}

TEST(NoiseEdgeCases, FractalFamilyExtremeCoordinates)
{
	for (bool normalise : { true, false })
	{
		ForAllCoords2([&](float x, float y) { CheckAllFractals(x, y, Base(normalise)); });
	}
	for (float z : Specials())
	{
		const float f3 = FractalNoise3D(11u, 0.4f, -0.7f, z, Base(true));
		EXPECT_TRUE(IsFinite(f3));
		EXPECT_GE(f3, -1.0f);
		EXPECT_LE(f3, 1.0f);
	}
}

TEST(NoiseEdgeCases, FractalFamilyExtremeParameters)
{
	const float pts[][2] = { { 0.37f, -2.6f }, { 1234.567f, 0.5f }, { 0.0f, 0.0f }, { FLT_MAX, -FLT_MAX } };
	for (bool normalise : { true, false })
	{
		for (float v : Specials())
		{
			for (int field = 0; field < 4; ++field)
			{
				FractalParams p = Base(normalise);
				switch (field)
				{
				case 0: p.frequency = v; break;
				case 1: p.lacunarity = v; break;
				case 2: p.gain = v; break;
				default: p.jitter = v; break;
				}
				for (const auto& pt : pts)
				{
					CheckAllFractals(pt[0], pt[1], p);
				}
				EXPECT_TRUE(IsFinite(FractalMaxAmplitude(p))) << "field " << field << " value " << v;
			}
		}
	}
}

TEST(NoiseEdgeCases, WarpStrengthExtremes)
{
	const float pts[][2] = { { 0.37f, -2.6f }, { FLT_MAX, FLT_MAX }, { -FLT_MAX, 0.0f }, { 8388608.0f, -8388608.0f } };
	for (float s : Specials())
	{
		for (const auto& pt : pts)
		{
			CheckWarp<NoiseType::Gradient>(pt[0], pt[1], s, Base(true));
			CheckWarp<NoiseType::Worley>(pt[0], pt[1], s, Base(false));
		}
	}
}

TEST(NoiseEdgeCases, OctavesOutsideRange)
{
	const int counts[] = { INT_MIN, -1000, -1, 0, 1, 12, 13, 64, 1000000, INT_MAX };
	for (int oc : counts)
	{
		for (bool normalise : { true, false })
		{
			FractalParams p = Base(normalise);
			p.octaves = oc;
			CheckAllFractals(0.37f, -2.6f, p);
			const int clamped = oc < 1 ? 1 : (oc > kMaxNoiseOctaves ? kMaxNoiseOctaves : oc);
			FractalParams q = p;
			q.octaves = clamped;
			EXPECT_EQ(Bits(FractalNoise2D<NoiseType::Gradient>(11u, 0.37f, -2.6f, p)), Bits(FractalNoise2D<NoiseType::Gradient>(11u, 0.37f, -2.6f, q))) << oc;
			EXPECT_EQ(Bits(FractalNoise3D(11u, 0.37f, -2.6f, 1.1f, p)), Bits(FractalNoise3D(11u, 0.37f, -2.6f, 1.1f, q))) << oc;
			EXPECT_EQ(Bits(FractalMaxAmplitude(p)), Bits(FractalMaxAmplitude(q))) << oc;
		}
	}
}

TEST(NoiseEdgeCases, RidgedSharpnessOutsideRange)
{
	const int sharp[] = { INT_MIN, -1, 0, 1, 4, 5, INT_MAX };
	for (int s : sharp)
	{
		RidgedParams rp;
		rp.fractal = Base(true);
		rp.sharpness = s;
		CheckRidged<NoiseType::Gradient>(0.37f, -2.6f, rp);
	}
}

TEST(NoiseEdgeCases, NonFiniteParametersFallBackToDefaults)
{
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const float inf = std::numeric_limits<float>::infinity();
	const FractalParams ref = Base(true); // frequency 1.3 retained
	FractalParams bad = ref;
	bad.lacunarity = nan;
	bad.gain = inf;
	FractalParams def = ref;
	def.lacunarity = 2.0f;
	def.gain = 0.5f;
	EXPECT_EQ(Bits(FractalNoise2D<NoiseType::Gradient>(2u, 0.37f, 0.61f, def)), Bits(FractalNoise2D<NoiseType::Gradient>(2u, 0.37f, 0.61f, bad)));
	FractalParams badF = ref;
	badF.frequency = -inf;
	FractalParams defF = ref;
	defF.frequency = 1.0f;
	EXPECT_EQ(Bits(FractalNoise3D(2u, 0.37f, 0.61f, 0.2f, defF)), Bits(FractalNoise3D(2u, 0.37f, 0.61f, 0.2f, badF)));
}
