#include <gtest/gtest.h>
#include <DiaMaths/Noise/Noise.h>
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

	struct Sample2
	{
		uint32_t seed;
		float x;
		float y;
	};

	// Negative, zero, large, lattice-aligned and fractional coordinates; several seeds.
	const Sample2 kSamples[] = {
		{ 0u,          0.0f,        0.0f },
		{ 0u,          0.5f,        0.5f },
		{ 1u,          0.25f,       0.75f },
		{ 1u,         -0.5f,       -0.5f },
		{ 12345u,      3.0f,        4.0f },
		{ 12345u,     -3.0f,       -4.0f },
		{ 12345u,      3.7f,       -4.2f },
		{ 0xDEADBEEFu, 100.123f,    200.456f },
		{ 0xDEADBEEFu,-100.123f,   -200.456f },
		{ 0xFFFFFFFFu, 0.1f,        0.9f },
		{ 7u,          1.0f,        0.0f },
		{ 7u,          0.0f,        1.0f },
		{ 7u,         -0.001f,      0.001f },
		{ 42u,         1000.5f,     -1000.5f },
		{ 42u,         8388608.0f,  8388608.0f },
		{ 42u,         1.0e9f,     -1.0e9f },
		{ 99u,         123456.789f, 0.333f },
		{ 99u,        -7.999f,      7.999f },
		{ 5u,          2.5f,        2.5f },
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

	RidgedParams MakeRidged(int octaves, bool normalise, int sharpness)
	{
		RidgedParams r;
		r.fractal = MakeParams(octaves, normalise);
		r.sharpness = sharpness;
		return r;
	}

	FractalParams WorleyParams(WorleyOutput out)
	{
		FractalParams p = MakeParams(4, true);
		p.worley = out;
		p.jitter = 0.9f;
		return p;
	}

	// Golden case list.
	enum GoldenCase
	{
		kFbmValue, kFbmGradient, kFbmSimplex,
		kFbmWorleyF1, kFbmWorleyF2, kFbmWorleyF2MinusF1, kFbmWorleyCell,
		kRidgedValue, kRidgedGradient, kRidgedSimplex,
		kWarpX, kWarpY,
		kFbmGradientRaw,
		kGoldenCaseCount
	};

	float Eval(int c, const Sample2& s)
	{
		const FractalParams norm = MakeParams(4, true);
		const RidgedParams rid = MakeRidged(4, true, 2);
		switch (c)
		{
		case kFbmValue:          return FractalNoise2D<NoiseType::Value>(s.seed, s.x, s.y, norm);
		case kFbmGradient:       return FractalNoise2D<NoiseType::Gradient>(s.seed, s.x, s.y, norm);
		case kFbmSimplex:        return FractalNoise2D<NoiseType::Simplex>(s.seed, s.x, s.y, norm);
		case kFbmWorleyF1:       return FractalNoise2D<NoiseType::Worley>(s.seed, s.x, s.y, WorleyParams(WorleyOutput::F1));
		case kFbmWorleyF2:       return FractalNoise2D<NoiseType::Worley>(s.seed, s.x, s.y, WorleyParams(WorleyOutput::F2));
		case kFbmWorleyF2MinusF1:return FractalNoise2D<NoiseType::Worley>(s.seed, s.x, s.y, WorleyParams(WorleyOutput::F2MinusF1));
		case kFbmWorleyCell:     return FractalNoise2D<NoiseType::Worley>(s.seed, s.x, s.y, WorleyParams(WorleyOutput::CellValue));
		case kRidgedValue:       return RidgedNoise2D<NoiseType::Value>(s.seed, s.x, s.y, rid);
		case kRidgedGradient:    return RidgedNoise2D<NoiseType::Gradient>(s.seed, s.x, s.y, rid);
		case kRidgedSimplex:     return RidgedNoise2D<NoiseType::Simplex>(s.seed, s.x, s.y, rid);
		case kWarpX:             return Warp2D<NoiseType::Gradient>(s.seed, s.x, s.y, 4.0f, norm).x;
		case kWarpY:             return Warp2D<NoiseType::Gradient>(s.seed, s.x, s.y, 4.0f, norm).y;
		case kFbmGradientRaw:    return FractalNoise2D<NoiseType::Gradient>(s.seed, s.x, s.y, MakeParams(4, false));
		}
		return 0.0f;
	}

	// Golden bit patterns (captured from the first GREEN run, then reviewed for range and variety).
	const uint32_t kGolden[kGoldenCaseCount][19] = {
		{ 0x3EF1367Fu, 0x3E4D4C6Cu, 0xBE83A9ECu, 0x3BA9AF2Bu, 0xBE3967F5u, 0xBE43356Du, 0xBEBF8E65u, 0xBDDAB00Fu, 0xBE84D1E4u, 0xBE20D0A3u, 0xBF0EEEC2u, 0x3E054FFFu, 0x3E2A7D15u, 0xBEECAAE7u, 0xBE217CA9u, 0xBEF9FBB4u, 0xBD18C89Au, 0xBE6F9916u, 0x3EA06C6Du },
		{ 0x00000000u, 0xBC098283u, 0xBE4A5FE7u, 0xBD9C1985u, 0x3E5D6C17u, 0xBD02F232u, 0x3DB2B051u, 0x3DB2225Fu, 0x3CC8ACCFu, 0x3E30A566u, 0x3D71E541u, 0x3E759035u, 0x38A92C7Cu, 0x3EF31466u, 0x00000000u, 0x00000000u, 0x3E81F27Eu, 0x3C984C93u, 0xBDFD3B4Cu },
		{ 0x9095601Au, 0xBE1DAB7Cu, 0x3A2CC155u, 0xBCECA316u, 0xBE84E62Au, 0x3D03656Fu, 0xBE50A58Du, 0xBBCA3835u, 0x3DB22E00u, 0x3ECA8B4Eu, 0x3E0C7F9Cu, 0x3DD4729Au, 0xBC209ECCu, 0x3E7529C9u, 0x10D21610u, 0x113820BDu, 0xBE2643ACu, 0x3F05B4DAu, 0xBE50FFDAu },
		{ 0xBDD054AAu, 0xBED0E304u, 0xBEE54595u, 0xBDCE78ADu, 0x3CF9CF9Au, 0xBD2B7844u, 0xBE8AF6A1u, 0x3E0AE71Du, 0xBE323FE7u, 0xBEED1129u, 0xBE147BB6u, 0xBDFD2B04u, 0xBE8DEBB4u, 0x3CF378BCu, 0xBEA82B0Bu, 0x3E98D9CAu, 0xBE1B7EAAu, 0x3E0D25C6u, 0xBE11ADCBu },
		{ 0xBEA4FB66u, 0xBEE0F78Cu, 0xBEFF2343u, 0xBECE6C64u, 0xBE8C5A4Eu, 0xBE0FF7C3u, 0xBE4C2272u, 0xBE5ACEA9u, 0xBEABA029u, 0xBEB5A049u, 0xBE9F61FBu, 0xBEC024C6u, 0xBED3EC46u, 0xBE950514u, 0xBEEFC5DEu, 0xBE5F76E1u, 0xBEE0F666u, 0xBE82F191u, 0xBEA3A7FCu },
		{ 0xBF0AF0D1u, 0xBEF10C11u, 0xBF0C8079u, 0xBF349D4Fu, 0xBF1428CBu, 0xBE751175u, 0xBE0257A1u, 0xBF10211Cu, 0xBEFE2061u, 0xBE7C5ECCu, 0xBEF4861Au, 0xBF207F65u, 0xBF0CF66Du, 0xBF1CA0DAu, 0xBF1BB05Au, 0xBF3C2856u, 0xBF3A16BEu, 0xBF263B02u, 0xBEFE7911u },
		{ 0xBF1DAA4Fu, 0xBE58520Bu, 0xBE010B3Eu, 0x3F09820Bu, 0x3EAA1BEAu, 0xBEEE227Au, 0x3ED347CBu, 0x3EE9FEF2u, 0xBF150BDAu, 0x3DADD257u, 0xBE14A36Bu, 0x3E41ED4Au, 0x3ED74585u, 0x3EEC329Fu, 0xBF2846F9u, 0xBE3F1E09u, 0xBE79AB95u, 0xBE88D282u, 0xBE2E8CFEu },
		{ 0x3E207A31u, 0x3F0CDCCEu, 0x3F0BEDA0u, 0x3F303A2Fu, 0x3ED143F2u, 0x3E93FA4Fu, 0x3ED69C79u, 0x3F4E9895u, 0x3EB61C63u, 0x3F0F1580u, 0x3E6E0E27u, 0x3F287C6Du, 0x3EE2C9EAu, 0x3EB7D5AAu, 0x3E90C8A7u, 0x3DF2C5B5u, 0x3F21821Bu, 0x3F197400u, 0x3EC25BBCu },
		{ 0x3F800000u, 0x3F7333CBu, 0x3F20AC5Bu, 0x3EC87B16u, 0x3F1D7B4Bu, 0x3F222F05u, 0x3F46FDB7u, 0x3F359EF6u, 0x3F1870A3u, 0x3F1780DBu, 0x3F26480Eu, 0x3F16F7BEu, 0x3F7EF4EBu, 0x3E888F6Bu, 0x3F800000u, 0x3F800000u, 0x3E90B44Eu, 0x3F0938EDu, 0x3F00D129u },
		{ 0x3F800000u, 0x3F33674Fu, 0x3F00F5CAu, 0x3EFD76E9u, 0x3EEC4E8Fu, 0x3F1413C1u, 0x3EE288C6u, 0x3F3DD435u, 0x3F419B7Fu, 0x3EA50472u, 0x3ED87A9Au, 0x3EC3C1C0u, 0x3F7A94FBu, 0x3EB15D4Du, 0x3F800000u, 0x3F800000u, 0x3F07957Cu, 0x3E9626B5u, 0x3F0DC041u },
		{ 0x00000000u, 0x3F01D7D6u, 0x3F9918C0u, 0xBF114EDCu, 0x4017AAF7u, 0xC01C8AA3u, 0x406A69E7u, 0x42C93A32u, 0xC2C77C2Du, 0x3DA66ACFu, 0x3E888236u, 0xBF9F3B50u, 0x3A22A8BCu, 0x447A2B00u, 0x4B000000u, 0x4E6E6B28u, 0x47F11FF1u, 0xC11DBF18u, 0x40454BB3u },
		{ 0x00000000u, 0x3DBE11DCu, 0x3F4949E1u, 0xBE8FEC4Au, 0x40978724u, 0xC08006EBu, 0xC091658Au, 0x4347DA71u, 0xC3483E3Eu, 0x3F9163C6u, 0x3EF36CC2u, 0x3F58E9BEu, 0xBBDA8DFEu, 0xC47A3909u, 0x4B000000u, 0xCE6E6B28u, 0xBECC5DF4u, 0x411A1709u, 0x40121E2Cu },
		{ 0x00000000u, 0xBC80EA5Bu, 0xBEBDB9E9u, 0xBE1257EDu, 0x3ECF9556u, 0xBD75861Eu, 0x3E27854Cu, 0x3E270039u, 0x3D3C2202u, 0x3EA59B10u, 0x3DE2C6EDu, 0x3EE63732u, 0x391E99B4u, 0x3F63E320u, 0x00000000u, 0x00000000u, 0x3EF3A6ACu, 0x3D0EC7CAu, 0xBE6D6797u },
	};
	

	float UlpOf(float v)
	{
		const float a = std::fabs(v);
		return std::nextafter(a, 1.0e30f) - a;
	}
}

//-----------------------------------------------------------------------------
// AC1: golden bit patterns
//-----------------------------------------------------------------------------

TEST(SLOW_NoiseFractal, Golden)
{
	for (int c = 0; c < kGoldenCaseCount; ++c)
	{
		for (int i = 0; i < kSampleCount; ++i)
		{
			EXPECT_EQ(kGolden[c][i], Bits(Eval(c, kSamples[i]))) << "case " << c << " sample " << i;
		}
	}
}

//-----------------------------------------------------------------------------
// AC2: determinism
//-----------------------------------------------------------------------------

TEST(SLOW_NoiseFractal, Determinism)
{
	for (int c = 0; c < kGoldenCaseCount; ++c)
	{
		for (int i = 0; i < kSampleCount; ++i)
		{
			const uint32_t first = Bits(Eval(c, kSamples[i]));
			for (int n = 0; n < 1000; ++n)
			{
				ASSERT_EQ(first, Bits(Eval(c, kSamples[i]))) << "case " << c << " sample " << i;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// AC3 / AC13: range over every base
//-----------------------------------------------------------------------------

namespace
{
	template <NoiseType Base>
	void CheckFractalRange(const FractalParams& base, uint32_t rngSeed, int sampleCount = 100000)
	{
		RandomStream rng(rngSeed);
		FractalParams norm = base; norm.normalise = true;
		FractalParams raw = base;  raw.normalise = false;
		const float S = FractalMaxAmplitude(raw);
		for (int i = 0; i < sampleCount; ++i)
		{
			const uint32_t seed = static_cast<uint32_t>(rng.RandomInt());
			const float x = rng.RandomRange(-500.0f, 500.0f);
			const float y = rng.RandomRange(-500.0f, 500.0f);
			const float n = FractalNoise2D<Base>(seed, x, y, norm);
			const float r = FractalNoise2D<Base>(seed, x, y, raw);
			ASSERT_GE(n, -1.0f); ASSERT_LE(n, 1.0f);
			ASSERT_GE(r, -S);    ASSERT_LE(r, S);
		}
	}

	template <NoiseType Base>
	void CheckRidgedRange(uint32_t rngSeed)
	{
		RandomStream rng(rngSeed);
		for (int sharp = 1; sharp <= 4; ++sharp)
		{
			const RidgedParams norm = MakeRidged(5, true, sharp);
			const RidgedParams raw = MakeRidged(5, false, sharp);
			const float S = FractalMaxAmplitude(raw.fractal);
			for (int i = 0; i < 25000; ++i)
			{
				const uint32_t seed = static_cast<uint32_t>(rng.RandomInt());
				const float x = rng.RandomRange(-500.0f, 500.0f);
				const float y = rng.RandomRange(-500.0f, 500.0f);
				const float n = RidgedNoise2D<Base>(seed, x, y, norm);
				const float r = RidgedNoise2D<Base>(seed, x, y, raw);
				ASSERT_GE(n, 0.0f); ASSERT_LE(n, 1.0f);
				ASSERT_GE(r, 0.0f); ASSERT_LE(r, S);
			}
		}
	}
}

TEST(SLOW_NoiseFractal, RangeValue)    { CheckFractalRange<NoiseType::Value>(MakeParams(6, true), 1); }
TEST(SLOW_NoiseFractal, RangeGradient) { CheckFractalRange<NoiseType::Gradient>(MakeParams(6, true), 2); }
TEST(SLOW_NoiseFractal, RangeSimplex)  { CheckFractalRange<NoiseType::Simplex>(MakeParams(6, true), 3); }

TEST(SLOW_NoiseFractal, RangeWorleyAllOutputs)
{
	const WorleyOutput outs[] = { WorleyOutput::F1, WorleyOutput::F2, WorleyOutput::F2MinusF1, WorleyOutput::CellValue };
	const float jitters[] = { 0.0f, 0.5f, 1.0f };
	uint32_t s = 10;
	for (WorleyOutput o : outs)
	{
		for (float j : jitters)
		{
			FractalParams p = MakeParams(4, true);
			p.worley = o;
			p.jitter = j;
			// Full 100k samples at the default jitter; 20k at the degenerate/partial jitters (Worley is the costliest base).
			CheckFractalRange<NoiseType::Worley>(p, s++, j == 1.0f ? 100000 : 20000);
		}
	}
}

// A single Worley octave must use the documented fixed remap and reach most of [-1, 1].
TEST(SLOW_NoiseFractal, WorleySingleOctaveRemap)
{
	const WorleyOutput outs[] = { WorleyOutput::F1, WorleyOutput::F2, WorleyOutput::F2MinusF1, WorleyOutput::CellValue };
	RandomStream rng(31);
	for (WorleyOutput o : outs)
	{
		FractalParams p;
		p.worley = o; p.octaves = 1; p.frequency = 1.0f; p.normalise = false;
		float lo = 1.0f, hi = -1.0f;
		for (int i = 0; i < 50000; ++i)
		{
			const uint32_t seed = static_cast<uint32_t>(rng.RandomInt());
			const float x = rng.RandomRange(-50.0f, 50.0f);
			const float y = rng.RandomRange(-50.0f, 50.0f);
			const float v = FractalNoise2D<NoiseType::Worley>(seed, x, y, p);
			ASSERT_GE(v, -1.0f); ASSERT_LE(v, 1.0f);
			lo = std::fmin(lo, v); hi = std::fmax(hi, v);
		}
		EXPECT_LT(lo, -0.5f) << "output " << static_cast<int>(o);
		EXPECT_GT(hi, 0.0f) << "output " << static_cast<int>(o);
	}
	// Exact mapping check for CellValue: octave 0 returns 2 * cellValue - 1.
	FractalParams p; p.worley = WorleyOutput::CellValue; p.octaves = 1; p.frequency = 1.0f; p.normalise = false; p.jitter = 1.0f;
	const WorleyResult w = WorleyNoise2D(77u, 3.3f, 4.4f, 1.0f);
	EXPECT_EQ(Bits(w.cellValue * 2.0f - 1.0f), Bits(FractalNoise2D<NoiseType::Worley>(77u, 3.3f, 4.4f, p)));
}

TEST(SLOW_NoiseFractal, RidgedRangeValue)    { CheckRidgedRange<NoiseType::Value>(4); }
TEST(SLOW_NoiseFractal, RidgedRangeGradient) { CheckRidgedRange<NoiseType::Gradient>(5); }
TEST(SLOW_NoiseFractal, RidgedRangeSimplex)  { CheckRidgedRange<NoiseType::Simplex>(6); }

// AC13: RidgedNoise2D<NoiseType::Worley> does not compile (static_assert(Base != NoiseType::Worley)
// inside the template). That cannot be exercised at runtime; do not add a call here.

//-----------------------------------------------------------------------------
// AC14: octave behaviour
//-----------------------------------------------------------------------------

TEST(SLOW_NoiseFractal, FractalMaxAmplitude)
{
	FractalParams p = MakeParams(4, false);
	EXPECT_EQ(Bits(1.875f), Bits(FractalMaxAmplitude(p)));
	p.octaves = 0;
	EXPECT_EQ(Bits(1.0f), Bits(FractalMaxAmplitude(p)));   // clamped up to 1 octave
	p.octaves = 100;
	EXPECT_EQ(Bits(FractalMaxAmplitude(MakeParams(kMaxNoiseOctaves, false))), Bits(FractalMaxAmplitude(p)));
}

TEST(SLOW_NoiseFractal, PrefixAdditiveBitExact)
{
	RandomStream rng(555);
	for (int i = 0; i < 300; ++i)
	{
		const uint32_t seed = static_cast<uint32_t>(rng.RandomInt());
		const float x = rng.RandomRange(-100.0f, 100.0f);
		const float y = rng.RandomRange(-100.0f, 100.0f);
		for (int n = 1; n < kMaxNoiseOctaves; ++n)
		{
			const FractalParams pn = MakeParams(n, false);
			const FractalParams pn1 = MakeParams(n + 1, false);
			float amp = 1.0f, freq = pn.frequency;
			for (int k = 0; k < n; ++k) { amp *= pn.gain; freq *= pn.lacunarity; }
			const uint32_t octSeed = seed + static_cast<uint32_t>(n) * kOctaveSeedStride;
			const float term = amp * GradientNoise2D(octSeed, x * freq, y * freq);
			const float prev = FractalNoise2D<NoiseType::Gradient>(seed, x, y, pn);
			const float next = FractalNoise2D<NoiseType::Gradient>(seed, x, y, pn1);
			ASSERT_EQ(Bits(prev + term), Bits(next)) << "octaves " << n;
		}
	}
}

TEST(SLOW_NoiseFractal, OctaveClamp)
{
	const Sample2 s = kSamples[7];
	EXPECT_EQ(Bits(FractalNoise2D<NoiseType::Simplex>(s.seed, s.x, s.y, MakeParams(1, false))),
	          Bits(FractalNoise2D<NoiseType::Simplex>(s.seed, s.x, s.y, MakeParams(-3, false))));
	EXPECT_EQ(Bits(FractalNoise2D<NoiseType::Simplex>(s.seed, s.x, s.y, MakeParams(kMaxNoiseOctaves, false))),
	          Bits(FractalNoise2D<NoiseType::Simplex>(s.seed, s.x, s.y, MakeParams(500, false))));
}

TEST(SLOW_NoiseFractal, NormalisedMatchesUnnormalisedOverS)
{
	RandomStream rng(808);
	for (int oct = 1; oct <= kMaxNoiseOctaves; ++oct)
	{
		const FractalParams norm = MakeParams(oct, true);
		const FractalParams raw = MakeParams(oct, false);
		const float S = FractalMaxAmplitude(raw);
		for (int i = 0; i < 2000; ++i)
		{
			const uint32_t seed = static_cast<uint32_t>(rng.RandomInt());
			const float x = rng.RandomRange(-100.0f, 100.0f);
			const float y = rng.RandomRange(-100.0f, 100.0f);
			const float n = FractalNoise2D<NoiseType::Gradient>(seed, x, y, norm);
			const float r = FractalNoise2D<NoiseType::Gradient>(seed, x, y, raw);
			ASSERT_GE(n, -1.0f); ASSERT_LE(n, 1.0f);
			const float expected = r / S;
			ASSERT_LE(std::fabs(n - expected), UlpOf(expected)) << "octaves " << oct;
		}
	}
}

//-----------------------------------------------------------------------------
// AC15: ridged shape
//-----------------------------------------------------------------------------

TEST(SLOW_NoiseFractal, RidgedCrestsAtBaseZeroCrossings)
{
	// Single octave, frequency 1: ridged == (1 - |n|)^k. Bisect for sign changes of the base along x.
	RidgedParams rp = MakeRidged(1, true, 2);
	rp.fractal.frequency = 1.0f;
	int crossings = 0;
	for (uint32_t seed = 1; seed <= 20; ++seed)
	{
		const float y = 0.37f + static_cast<float>(seed);
		float prevX = 0.13f;
		float prevV = GradientNoise2D(seed, prevX, y);
		for (int k = 1; k < 400; ++k)
		{
			const float cx = 0.13f + 0.05f * static_cast<float>(k);
			const float cv = GradientNoise2D(seed, cx, y);
			if ((prevV < 0.0f) != (cv < 0.0f))
			{
				float lo = prevX, hi = cx, vlo = prevV;
				for (int b = 0; b < 40; ++b)
				{
					const float mid = 0.5f * (lo + hi);
					const float vm = GradientNoise2D(seed, mid, y);
					if ((vm < 0.0f) == (vlo < 0.0f)) { lo = mid; vlo = vm; } else { hi = mid; }
				}
				const float crest = RidgedNoise2D<NoiseType::Gradient>(seed, 0.5f * (lo + hi), y, rp);
				EXPECT_GT(crest, 0.999f) << "seed " << seed;
				++crossings;
			}
			prevX = cx; prevV = cv;
		}
	}
	EXPECT_GT(crossings, 20);

	// And far from a crossing the ridged value is low.
	float minV = 1.0f;
	for (int k = 0; k < 2000; ++k)
	{
		minV = std::fmin(minV, RidgedNoise2D<NoiseType::Gradient>(3u, 0.01f * static_cast<float>(k), 0.5f, rp));
	}
	EXPECT_LT(minV, 0.6f);
}

TEST(SLOW_NoiseFractal, RidgedVarianceNonDecreasingWithSharpness)
{
	float prevVar = -1.0f;
	for (int sharp = 1; sharp <= 4; ++sharp)
	{
		RandomStream rng(4242);   // same samples per sharpness
		const RidgedParams rp = MakeRidged(1, true, sharp);
		double sum = 0.0, sumSq = 0.0;
		const int kCount = 50000;
		for (int i = 0; i < kCount; ++i)
		{
			const uint32_t seed = static_cast<uint32_t>(rng.RandomInt());
			const float x = rng.RandomRange(-300.0f, 300.0f);
			const float y = rng.RandomRange(-300.0f, 300.0f);
			const double v = RidgedNoise2D<NoiseType::Gradient>(seed, x, y, rp);
			sum += v; sumSq += v * v;
		}
		const double mean = sum / kCount;
		const float var = static_cast<float>(sumSq / kCount - mean * mean);
		std::printf("[NoiseFractal] ridged sharpness %d variance %f\n", sharp, var);
		EXPECT_GE(var, prevVar) << "sharpness " << sharp;
		prevVar = var;
	}
}

TEST(SLOW_NoiseFractal, RidgedSharpnessClamps)
{
	const Sample2 s = kSamples[6];
	const float s1 = RidgedNoise2D<NoiseType::Simplex>(s.seed, s.x, s.y, MakeRidged(3, true, 1));
	const float s4 = RidgedNoise2D<NoiseType::Simplex>(s.seed, s.x, s.y, MakeRidged(3, true, 4));
	EXPECT_EQ(Bits(s1), Bits(RidgedNoise2D<NoiseType::Simplex>(s.seed, s.x, s.y, MakeRidged(3, true, 0))));
	EXPECT_EQ(Bits(s1), Bits(RidgedNoise2D<NoiseType::Simplex>(s.seed, s.x, s.y, MakeRidged(3, true, -7))));
	EXPECT_EQ(Bits(s4), Bits(RidgedNoise2D<NoiseType::Simplex>(s.seed, s.x, s.y, MakeRidged(3, true, 5))));
	EXPECT_EQ(Bits(s4), Bits(RidgedNoise2D<NoiseType::Simplex>(s.seed, s.x, s.y, MakeRidged(3, true, 1000))));
}

//-----------------------------------------------------------------------------
// AC16: domain warp
//-----------------------------------------------------------------------------

TEST(SLOW_NoiseFractal, WarpZeroStrengthIsExact)
{
	for (int i = 0; i < kSampleCount; ++i)
	{
		const Sample2& s = kSamples[i];
		const Warp2DResult w = Warp2D<NoiseType::Simplex>(s.seed, s.x, s.y, 0.0f, MakeParams(4, true));
		EXPECT_EQ(Bits(s.x), Bits(w.x));
		EXPECT_EQ(Bits(s.y), Bits(w.y));
	}
}

TEST(SLOW_NoiseFractal, WarpDisplacementBounded)
{
	RandomStream rng(1313);
	const float strength = 3.5f;
	for (int mode = 0; mode < 2; ++mode)
	{
		const FractalParams wp = MakeParams(5, mode == 1);
		const float bound = strength * 1.41421356f * (mode == 1 ? 1.0f : FractalMaxAmplitude(wp));
		float maxD = 0.0f;
		for (int i = 0; i < 50000; ++i)
		{
			const uint32_t seed = static_cast<uint32_t>(rng.RandomInt());
			const float x = rng.RandomRange(-200.0f, 200.0f);
			const float y = rng.RandomRange(-200.0f, 200.0f);
			const Warp2DResult w = Warp2D<NoiseType::Gradient>(seed, x, y, strength, wp);
			const float dx = w.x - x, dy = w.y - y;
			const float d = std::sqrt(dx * dx + dy * dy);
			maxD = std::fmax(maxD, d);
			ASSERT_LE(d, bound * 1.0001f + 1.0e-3f);
		}
		EXPECT_GT(maxD, 0.0f);
	}
}

TEST(SLOW_NoiseFractal, WarpAxesIndependentAndWarpedNoiseMatches)
{
	const Sample2& s = kSamples[6];
	const FractalParams wp = MakeParams(3, true);
	const FractalParams np = MakeParams(4, true);
	const Warp2DResult w = Warp2D<NoiseType::Value>(s.seed, s.x, s.y, 2.0f, wp);
	EXPECT_NE(Bits(w.x - s.x), Bits(w.y - s.y));
	const float composite = WarpedNoise2D<NoiseType::Value, NoiseType::Simplex>(s.seed, s.x, s.y, 2.0f, wp, np);
	EXPECT_EQ(Bits(FractalNoise2D<NoiseType::Simplex>(s.seed, w.x, w.y, np)), Bits(composite));
}

TEST(SLOW_NoiseFractal, WarpOverWorley)
{
	const Warp2DResult w = Warp2D<NoiseType::Worley>(3u, 1.5f, 2.5f, 1.0f, WorleyParams(WorleyOutput::F1));
	EXPECT_LE(std::fabs(w.x - 1.5f), 1.0f);
	EXPECT_LE(std::fabs(w.y - 2.5f), 1.0f);
}
