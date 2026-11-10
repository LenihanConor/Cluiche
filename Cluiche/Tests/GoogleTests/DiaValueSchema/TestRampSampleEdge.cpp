#include <gtest/gtest.h>
#include <DiaValueSchema/RampSample.h>
#include <DiaValueSchema/RampLoader.h>
#include "ValueSchemaTestSupport.h"
#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <limits>
#include <random>
#include <set>
#include <string>
#include <vector>

// Exhaustive edge/property/golden tests for SampleRamp and RampColourAt (Feature 5 hardening).

using namespace Dia::ValueSchema;

namespace
{
	constexpr float kInf = std::numeric_limits<float>::infinity();
	constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();

	RampData MakeRamp(std::initializer_list<std::pair<float, uint32_t>> stops)
	{
		RampData r;
		r.mKind = RampKind::Continuous;
		unsigned i = 0;
		for (const auto& s : stops) { r.mStops[i].mT = s.first; r.mStops[i].mRgba8 = s.second; ++i; }
		r.mStopCount = i;
		return r;
	}

	RampData BlackWhite() { return MakeRamp({ { 0.0f, 0x000000FFu }, { 1.0f, 0xFFFFFFFFu } }); }

	RampData MakeCategorical(unsigned count)
	{
		RampData r;
		r.mKind = RampKind::Categorical;
		for (unsigned i = 0; i < count; ++i) { r.mColours[i] = 0x01000000u * (i + 1) | 0xFFu; }
		r.mColourCount = count;
		return r;
	}

	unsigned Ch(uint32_t rgba, int channel) { return (rgba >> (24 - 8 * channel)) & 0xFFu; }

	const RampLoadResult& Shipped()
	{
		static const RampLoadResult sResult = []
		{
			std::string text;
			const auto p = VsTest::FindUpwards(__FILE__, "Dia/DiaValueSchema/Data/ramps.json");
			VsTest::ReadFileBytes(p, text);
			return RampLoader::LoadFromText(text.c_str(), "ramps.json");
		}();
		return sResult;
	}

	const RampData* FindShipped(const std::string& id)
	{
		const RampLoadResult& r = Shipped();
		for (unsigned i = 0; i < r.mRamps.Size(); ++i)
		{
			if (id == r.mRamps[i].mName.AsCStr()) { return &r.mRamps[i]; }
		}
		return nullptr;
	}
}

// ---- special t values ----

TEST(DiaValueSchemaRampSampleEdge, InfinityClampsToEnds)
{
	const RampData r = BlackWhite();
	EXPECT_EQ(0xFFFFFFFFu, SampleRamp(r, kInf));
	EXPECT_EQ(0x000000FFu, SampleRamp(r, -kInf));
}

TEST(DiaValueSchemaRampSampleEdge, NegativeZeroIsFirstStop)
{
	const RampData r = BlackWhite();
	EXPECT_EQ(0x000000FFu, SampleRamp(r, -0.0f));
}

TEST(DiaValueSchemaRampSampleEdge, SubnormalTIsFirstStop)
{
	const RampData r = BlackWhite();
	EXPECT_EQ(0x000000FFu, SampleRamp(r, std::numeric_limits<float>::denorm_min()));
	EXPECT_EQ(0x000000FFu, SampleRamp(r, std::numeric_limits<float>::min() / 2.0f));
	EXPECT_EQ(0x000000FFu, SampleRamp(r, -std::numeric_limits<float>::denorm_min()));
}

TEST(DiaValueSchemaRampSampleEdge, JustAboveAndBelowOne)
{
	const RampData r = BlackWhite();
	const float eps = std::numeric_limits<float>::epsilon();
	EXPECT_EQ(0xFFFFFFFFu, SampleRamp(r, 1.0f + eps));
	EXPECT_EQ(0xFFFFFFFFu, SampleRamp(r, std::nextafter(1.0f, 2.0f)));
	// 1 - eps/2 is the float just below 1: 255 * (1 - 6e-8) = 254.99998 rounds to 255.
	EXPECT_EQ(0xFFFFFFFFu, SampleRamp(r, std::nextafter(1.0f, 0.0f)));
	EXPECT_EQ(0xFFFFFFFFu, SampleRamp(r, 1.0f - eps));
}

TEST(DiaValueSchemaRampSampleEdge, FloatMaxAndLowest)
{
	const RampData r = BlackWhite();
	EXPECT_EQ(0xFFFFFFFFu, SampleRamp(r, std::numeric_limits<float>::max()));
	EXPECT_EQ(0x000000FFu, SampleRamp(r, std::numeric_limits<float>::lowest()));
}

TEST(DiaValueSchemaRampSampleEdge, AllNaNPayloadsAreZero)
{
	const RampData r = BlackWhite();
	EXPECT_EQ(0x000000FFu, SampleRamp(r, kNaN));
	EXPECT_EQ(0x000000FFu, SampleRamp(r, -kNaN));
	EXPECT_EQ(0x000000FFu, SampleRamp(r, std::numeric_limits<float>::signaling_NaN()));
}

// ---- hand-built ramps ----

TEST(DiaValueSchemaRampSampleEdge, FirstStopAfterZeroHoldsBelow)
{
	const RampData r = MakeRamp({ { 0.25f, 0xFF0000FFu }, { 1.0f, 0x0000FFFFu } });
	EXPECT_EQ(0xFF0000FFu, SampleRamp(r, 0.0f));
	EXPECT_EQ(0xFF0000FFu, SampleRamp(r, 0.1f));
	EXPECT_EQ(0xFF0000FFu, SampleRamp(r, 0.25f));
	EXPECT_EQ(0x0000FFFFu, SampleRamp(r, 1.0f));
	// t = 0.625 is the midpoint of the 0.25..1 span: (255+0)/2 = 127.5 -> 128 on red, blue.
	EXPECT_EQ(0x800080FFu, SampleRamp(r, 0.625f));
}

TEST(DiaValueSchemaRampSampleEdge, LastStopBeforeOneHoldsAbove)
{
	const RampData r = MakeRamp({ { 0.0f, 0x000000FFu }, { 0.5f, 0x00FF00FFu } });
	EXPECT_EQ(0x00FF00FFu, SampleRamp(r, 0.5f));
	EXPECT_EQ(0x00FF00FFu, SampleRamp(r, 0.75f));
	EXPECT_EQ(0x00FF00FFu, SampleRamp(r, 1.0f));
	EXPECT_EQ(0x008000FFu, SampleRamp(r, 0.25f));
}

TEST(DiaValueSchemaRampSampleEdge, SingleStopIsConstant)
{
	const RampData r = MakeRamp({ { 0.5f, 0x336699FFu } });
	for (float t : { -1.0f, 0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 2.0f, kNaN, kInf, -kInf })
	{
		EXPECT_EQ(0x336699FFu, SampleRamp(r, t)) << t;
	}
}

TEST(DiaValueSchemaRampSampleEdge, SingleStopAtZeroAndOne)
{
	EXPECT_EQ(0x112233FFu, SampleRamp(MakeRamp({ { 0.0f, 0x112233FFu } }), 0.7f));
	EXPECT_EQ(0x112233FFu, SampleRamp(MakeRamp({ { 1.0f, 0x112233FFu } }), 0.2f));
}

TEST(DiaValueSchemaRampSampleEdge, ZeroWidthSpanDoesNotDivideByZero)
{
	// Hand-built (the loader rejects equal t): a step between black and white at 0.5.
	const RampData r = MakeRamp({ { 0.0f, 0x000000FFu }, { 0.5f, 0x444444FFu }, { 0.5f, 0xBBBBBBFFu }, { 1.0f, 0xFFFFFFFFu } });
	EXPECT_EQ(0x444444FFu, SampleRamp(r, 0.5f));	// first stop reaching t wins
	EXPECT_EQ(0xFFFFFFFFu, SampleRamp(r, 1.0f));
	EXPECT_EQ(0x000000FFu, SampleRamp(r, 0.0f));
	for (float t = 0.0f; t <= 1.0f; t += 0.01f)
	{
		const uint32_t c = SampleRamp(r, t);
		EXPECT_EQ(0xFFu, c & 0xFFu);
	}
	// The segment between the duplicate stops 0.5/0.5 is never interpolated across: just above 0.5 starts at 0xBB.
	EXPECT_GE(Ch(SampleRamp(r, 0.5001f), 0), 0xBBu);
}

TEST(DiaValueSchemaRampSampleEdge, ZeroWidthLeadingSpan)
{
	const RampData r = MakeRamp({ { 0.0f, 0x102030FFu }, { 0.0f, 0x405060FFu }, { 1.0f, 0xFFFFFFFFu } });
	EXPECT_EQ(0x102030FFu, SampleRamp(r, 0.0f));
	const uint32_t c = SampleRamp(r, 0.5f);
	EXPECT_EQ(0xFFu, c & 0xFFu);
	EXPECT_GE(Ch(c, 0), 0x40u);
}

TEST(DiaValueSchemaRampSampleEdge, RoundingMidpointGoesUp)
{
	const RampData up = MakeRamp({ { 0.0f, 0x000000FFu }, { 1.0f, 0xFFFFFFFFu } });
	EXPECT_EQ(0x808080FFu, SampleRamp(up, 0.5f));	// 127.5 -> 128
	const RampData down = MakeRamp({ { 0.0f, 0xFFFFFFFFu }, { 1.0f, 0x000000FFu } });
	EXPECT_EQ(0x808080FFu, SampleRamp(down, 0.5f));	// 255 - 127.5 = 127.5 -> 128 (symmetric: still rounds up)
}

TEST(DiaValueSchemaRampSampleEdge, ChannelsAreInterpolatedIndependently)
{
	const RampData r = MakeRamp({ { 0.0f, 0xFF0000FFu }, { 1.0f, 0x0000FFFFu } });
	EXPECT_EQ(0xFF0000FFu, SampleRamp(r, 0.0f));
	EXPECT_EQ(0x800080FFu, SampleRamp(r, 0.5f));
	EXPECT_EQ(0x0000FFFFu, SampleRamp(r, 1.0f));
	const RampData g = MakeRamp({ { 0.0f, 0x00FF00FFu }, { 1.0f, 0x00FF00FFu } });
	EXPECT_EQ(0x00FF00FFu, SampleRamp(g, 0.37f));	// constant colour stays constant
}

TEST(DiaValueSchemaRampSampleEdge, StopAlphaIsIgnoredAlwaysOpaque)
{
	const RampData r = MakeRamp({ { 0.0f, 0x11223300u }, { 1.0f, 0x44556600u } });
	EXPECT_EQ(0x112233FFu, SampleRamp(r, 0.0f));
	EXPECT_EQ(0x445566FFu, SampleRamp(r, 1.0f));
	EXPECT_EQ(0xFFu, SampleRamp(r, 0.5f) & 0xFFu);
	const RampData one = MakeRamp({ { 0.5f, 0xAABBCC10u } });
	EXPECT_EQ(0xAABBCCFFu, SampleRamp(one, 0.1f));
}

TEST(DiaValueSchemaRampSampleEdge, MaxStopCountRamp)
{
	RampData r;
	r.mKind = RampKind::Continuous;
	for (unsigned i = 0; i < kMaxRampStops; ++i)
	{
		r.mStops[i].mT = static_cast<float>(i) / static_cast<float>(kMaxRampStops - 1);
		r.mStops[i].mRgba8 = (i * 16u) << 24 | 0xFFu;
	}
	r.mStopCount = kMaxRampStops;
	EXPECT_EQ(r.mStops[0].mRgba8, SampleRamp(r, 0.0f));
	EXPECT_EQ(r.mStops[kMaxRampStops - 1].mRgba8, SampleRamp(r, 1.0f));
	for (unsigned i = 0; i < kMaxRampStops; ++i) { EXPECT_EQ(r.mStops[i].mRgba8, SampleRamp(r, r.mStops[i].mT)) << i; }
}

TEST(DiaValueSchemaRampSampleEdge, CategoricalIgnoresStopsAndT)
{
	RampData r = MakeCategorical(3);
	r.mStops[0].mT = 0.0f; r.mStops[0].mRgba8 = 0x11111111u; r.mStopCount = 1;
	for (float t : { 0.0f, 0.5f, 1.0f, kNaN, kInf }) { EXPECT_EQ(r.mColours[0], SampleRamp(r, t)); }
}

TEST(DiaValueSchemaRampSampleEdge, CategoricalWithNoColoursIsBlackEvenWithStops)
{
	RampData r;
	r.mKind = RampKind::Categorical;
	r.mStops[0].mT = 0.0f; r.mStops[0].mRgba8 = 0x11111111u; r.mStopCount = 1;
	EXPECT_EQ(0x000000FFu, SampleRamp(r, 0.5f));
	EXPECT_EQ(0x000000FFu, RampColourAt(r, 0));
	EXPECT_EQ(0x000000FFu, RampColourAt(r, UINT_MAX));
}

TEST(DiaValueSchemaRampSampleEdge, ContinuousIgnoresColoursArray)
{
	RampData r = BlackWhite();
	r.mColours[0] = 0xDEADBEEFu; r.mColourCount = 1;
	EXPECT_EQ(0x000000FFu, SampleRamp(r, 0.0f));
	EXPECT_EQ(0x000000FFu, RampColourAt(r, 7));
}

// ---- RampColourAt ----

TEST(DiaValueSchemaRampSampleEdge, RampColourAtUintMaxCountOne)
{
	const RampData r = MakeCategorical(1);
	EXPECT_EQ(r.mColours[0], RampColourAt(r, UINT_MAX));
	EXPECT_EQ(r.mColours[0], RampColourAt(r, 0));
	EXPECT_EQ(r.mColours[0], RampColourAt(r, 12345));
}

TEST(DiaValueSchemaRampSampleEdge, RampColourAtUintMaxCountThirtyTwo)
{
	const RampData r = MakeCategorical(32);
	EXPECT_EQ(r.mColours[UINT_MAX % 32], RampColourAt(r, UINT_MAX));
	EXPECT_EQ(r.mColours[31], RampColourAt(r, UINT_MAX));
	EXPECT_EQ(r.mColours[0], RampColourAt(r, 32));
	EXPECT_EQ(r.mColours[31], RampColourAt(r, 31));
	EXPECT_EQ(r.mColours[1], RampColourAt(r, 33));
}

TEST(DiaValueSchemaRampSampleEdge, RampColourAtEveryIndexWraps)
{
	for (unsigned count : { 1u, 2u, 3u, 12u, 31u, 32u })
	{
		const RampData r = MakeCategorical(count);
		for (unsigned i = 0; i < 100; ++i) { EXPECT_EQ(r.mColours[i % count], RampColourAt(r, i)) << count << "/" << i; }
	}
}

// ---- property tests ----

namespace
{
	struct RandomRamp
	{
		RampData	mRamp;
	};

	RampData RandomValidRamp(std::mt19937& rng)
	{
		std::uniform_int_distribution<int> countDist(2, 16);
		const int n = countDist(rng);
		std::set<int> grid;		// interior stops on a 1/1000 grid, distinct => strictly increasing and well separated
		std::uniform_int_distribution<int> gridDist(1, 999);
		while (static_cast<int>(grid.size()) < n - 2) { grid.insert(gridDist(rng)); }
		RampData r;
		r.mKind = RampKind::Continuous;
		unsigned i = 0;
		r.mStops[i++].mT = 0.0f;
		for (int g : grid) { r.mStops[i++].mT = static_cast<float>(g) / 1000.0f; }
		r.mStops[i++].mT = 1.0f;
		r.mStopCount = i;
		for (unsigned s = 0; s < r.mStopCount; ++s) { r.mStops[s].mRgba8 = (static_cast<uint32_t>(rng()) & 0xFFFFFF00u) | 0xFFu; }
		return r;
	}
}

TEST(DiaValueSchemaRampSampleEdgeProperty, RandomRampsStayBetweenSurroundingStops)
{
	std::mt19937 rng(1234);
	std::uniform_real_distribution<float> tDist(0.0f, 1.0f);
	for (int iter = 0; iter < 300; ++iter)
	{
		const RampData r = RandomValidRamp(rng);
		ASSERT_GE(r.mStopCount, 2u);
		EXPECT_EQ(r.mStops[0].mRgba8, SampleRamp(r, 0.0f));
		EXPECT_EQ(r.mStops[r.mStopCount - 1].mRgba8, SampleRamp(r, 1.0f));
		for (unsigned s = 0; s < r.mStopCount; ++s) { EXPECT_EQ(r.mStops[s].mRgba8, SampleRamp(r, r.mStops[s].mT)) << iter << " stop " << s; }
		for (int k = 0; k < 40; ++k)
		{
			const float t = tDist(rng);
			const uint32_t c = SampleRamp(r, t);
			EXPECT_EQ(0xFFu, c & 0xFFu);
			// Find the surrounding stops.
			unsigned hi = 1;
			while (hi + 1 < r.mStopCount && t > r.mStops[hi].mT) { ++hi; }
			const uint32_t a = r.mStops[hi - 1].mRgba8, b = r.mStops[hi].mRgba8;
			for (int ch = 0; ch < 3; ++ch)
			{
				const unsigned lo = std::min(Ch(a, ch), Ch(b, ch)), up = std::max(Ch(a, ch), Ch(b, ch));
				EXPECT_GE(Ch(c, ch), lo) << iter << " t=" << t << " ch=" << ch;
				EXPECT_LE(Ch(c, ch), up) << iter << " t=" << t << " ch=" << ch;
			}
		}
	}
}

TEST(DiaValueSchemaRampSampleEdgeProperty, MonotoneChannelsSampleMonotone)
{
	std::mt19937 rng(1234);
	for (int iter = 0; iter < 200; ++iter)
	{
		RampData r = RandomValidRamp(rng);
		// Make every channel non-decreasing along the ramp (sort each channel's values), randomly flipping direction.
		for (int ch = 0; ch < 3; ++ch)
		{
			std::vector<unsigned> v;
			for (unsigned s = 0; s < r.mStopCount; ++s) { v.push_back(Ch(r.mStops[s].mRgba8, ch)); }
			std::sort(v.begin(), v.end());
			for (unsigned s = 0; s < r.mStopCount; ++s)
			{
				const int shift = 24 - 8 * ch;
				r.mStops[s].mRgba8 = (r.mStops[s].mRgba8 & ~(0xFFu << shift)) | (v[s] << shift);
			}
		}
		uint32_t prev = SampleRamp(r, 0.0f);
		for (int k = 1; k <= 400; ++k)
		{
			const uint32_t c = SampleRamp(r, static_cast<float>(k) / 400.0f);
			for (int ch = 0; ch < 3; ++ch) { EXPECT_GE(Ch(c, ch), Ch(prev, ch)) << iter << " k=" << k << " ch=" << ch; }
			prev = c;
		}
	}
}

TEST(DiaValueSchemaRampSampleEdgeProperty, ContinuityBound)
{
	std::mt19937 rng(1234);
	std::uniform_real_distribution<float> tDist(0.0f, 0.99f);
	const float dt = 0.001f;
	for (int iter = 0; iter < 200; ++iter)
	{
		const RampData r = RandomValidRamp(rng);
		// Largest slope (channel units per unit t) over all spans.
		double maxSlope = 0.0;
		for (unsigned s = 1; s < r.mStopCount; ++s)
		{
			const double span = r.mStops[s].mT - r.mStops[s - 1].mT;
			for (int ch = 0; ch < 3; ++ch)
			{
				const double d = std::fabs(static_cast<double>(Ch(r.mStops[s].mRgba8, ch)) - static_cast<double>(Ch(r.mStops[s - 1].mRgba8, ch)));
				maxSlope = std::max(maxSlope, d / span);
			}
		}
		const double bound = maxSlope * dt + 1.0 + 1e-2;
		for (int k = 0; k < 40; ++k)
		{
			const float t = tDist(rng);
			const uint32_t a = SampleRamp(r, t), b = SampleRamp(r, t + dt);
			for (int ch = 0; ch < 3; ++ch)
			{
				EXPECT_LE(std::fabs(static_cast<double>(Ch(a, ch)) - static_cast<double>(Ch(b, ch))), bound) << iter << " t=" << t;
			}
		}
	}
}

// ---- shipped ramp goldens ----
// Generated independently with a float32-emulating script over Dia/DiaValueSchema/Data/ramps.json.

namespace
{
	const float kGoldenT[8] = { 0.0f, 0.1f, 0.25f, 0.3333333f, 0.5f, 0.6f, 0.875f, 1.0f };

	struct Golden { const char* mId; uint32_t mColours[8]; };

	const Golden kGoldens[] = {
		{ "sequential.green", { 0xF7FCF5FF, 0xE9F6E4FF, 0xC7E9C0FF, 0xAEDEA7FF, 0x74C476FF, 0x4BB062FF, 0x006D2CFF, 0x00441BFF } },
		{ "sequential.blue", { 0xF7FBFFFF, 0xE3EEF9FF, 0xC6DBEFFF, 0xABD0E6FF, 0x6BAED6FF, 0x4A98C9FF, 0x08519CFF, 0x08306BFF } },
		{ "sequential.heat", { 0xFFFFCCFF, 0xFFF1A9FF, 0xFED976FF, 0xFEBF5AFF, 0xFD8D3CFF, 0xFC5B2EFF, 0xBD0026FF, 0x800026FF } },
		{ "sequential.grey", { 0xFFFFFFFF, 0xF3F3F3FF, 0xD9D9D9FF, 0xC6C6C6FF, 0x969696FF, 0x7A7A7AFF, 0x252525FF, 0x000000FF } },
		{ "sequential.purple", { 0xFCFBFDFF, 0xF2F0F7FF, 0xDADAEBFF, 0xC6C7E1FF, 0x9E9AC8FF, 0x8683BDFF, 0x54278FFF, 0x3F007DFF } },
		{ "sequential.terrain", { 0x3B6FA8FF, 0x73A6C9FF, 0x76AE8FFF, 0x7AAA6DFF, 0xC9C27AFF, 0xB59D61FF, 0x8E745EFF, 0xF5F5F5FF } },
		{ "diverging.blue_red", { 0x053061FF, 0x2166ACFF, 0x6BACD1FF, 0xA7D0E4FF, 0xF7F7F7FF, 0xFDDBC8FF, 0xC23E46FF, 0x67001FFF } },
		{ "categorical.12", { 0xE9EA86FF, 0xE9EA86FF, 0xE9EA86FF, 0xE9EA86FF, 0xE9EA86FF, 0xE9EA86FF, 0xE9EA86FF, 0xE9EA86FF } },
	};
}

TEST(DiaValueSchemaRampSampleEdgeGolden, ShippedRampsAtEightTValues)
{
	ASSERT_TRUE(Shipped().mOk);
	ASSERT_EQ(8u, Shipped().mRamps.Size());
	for (const Golden& g : kGoldens)
	{
		const RampData* r = FindShipped(g.mId);
		ASSERT_NE(r, nullptr) << g.mId;
		for (int i = 0; i < 8; ++i)
		{
			EXPECT_EQ(g.mColours[i], SampleRamp(*r, kGoldenT[i])) << g.mId << " t=" << kGoldenT[i];
		}
	}
}

TEST(DiaValueSchemaRampSampleEdgeGolden, ShippedContinuousRampsAreOpaqueEverywhere)
{
	const RampLoadResult& all = Shipped();
	for (unsigned i = 0; i < all.mRamps.Size(); ++i)
	{
		const RampData& r = all.mRamps[i];
		if (r.mKind != RampKind::Continuous) { continue; }
		for (int k = 0; k <= 1000; ++k) { EXPECT_EQ(0xFFu, SampleRamp(r, static_cast<float>(k) / 1000.0f) & 0xFFu) << r.mName.AsCStr(); }
	}
}

TEST(DiaValueSchemaRampSampleEdgeGolden, ShippedCategoricalColourAtIndices)
{
	const RampData* r = FindShipped("categorical.12");
	ASSERT_NE(r, nullptr);
	ASSERT_EQ(12u, r->mColourCount);
	for (unsigned i = 0; i < 40; ++i) { EXPECT_EQ(r->mColours[i % 12], RampColourAt(*r, i)); }
	EXPECT_EQ(r->mColours[UINT_MAX % 12], RampColourAt(*r, UINT_MAX));
}
