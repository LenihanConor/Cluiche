#include <gtest/gtest.h>
#include <DiaCellHistory/CellRing.h>
#include <DiaCellHistory/Testing/BudgetSizing.h>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace Dia::CellHistory;
using Dia::Core::StringCRC;
namespace T = Dia::CellHistory::Testing;

namespace
{
	constexpr uint32_t kCells = 8;
	const StringCRC kA("a");
	const StringCRC kB("b");
	const StringCRC kC("c");

	bool Put(CellRing& r, StringCRC layer, float value)
	{
		std::vector<float> v(r.CellCount(), value);
		return r.Append(layer, v.data(), r.CellCount());
	}
	bool Sample(CellRing& r, uint32_t s, float va)
	{
		return r.BeginSample(s) && Put(r, kA, va);
	}
	float ReadCell0(const CellRing& r, uint32_t s, StringCRC layer, bool* ok = nullptr)
	{
		std::vector<float> v(r.CellCount());
		bool got = r.Read(s, layer, v.data(), r.CellCount());
		if (ok) *ok = got;
		return v[0];
	}
	void MakeFloat(CellRing& r) { ASSERT_TRUE(r.Track(kA, CellRingKind::Float16, 0.f, 100.f)); }
}

TEST(CellRing, WindowLengthArithmetic)
{
	CellRing r(kCells, T::BudgetBytesForWindow(5, T::BytesPerSample(1, 0, kCells)));
	MakeFloat(r);
	EXPECT_EQ(r.BytesPerSample(), kCells * 2u);
	EXPECT_EQ(r.WindowLength(), 5u);
	EXPECT_EQ(r.BytesUsed(), 5u * kCells * 2u);
	ASSERT_TRUE(r.Track(kB, CellRingKind::Category8, 0, 0));
	EXPECT_EQ(r.BytesPerSample(), kCells * 3u);
	EXPECT_EQ(r.WindowLength(), T::WindowLengthFor(5 * kCells * 2, kCells * 3));
}

TEST(CellRing, FloatRoundTripWithinBound)
{
	CellRing r(kCells, 1 << 20);
	ASSERT_TRUE(r.Track(kA, CellRingKind::Float16, -50.f, 150.f));
	const float range = 200.f;
	for (int i = 0; i <= 1000; ++i)
	{
		float v = -50.f + range * float(i) / 1000.f;
		ASSERT_TRUE(r.BeginSample(uint32_t(i)));
		ASSERT_TRUE(Put(r, kA, v));
		float got = ReadCell0(r, uint32_t(i), kA);
		EXPECT_LE(std::fabs(got - v), range / 65535.f + 1e-4f) << v;
	}
}

TEST(CellRing, FloatMaxAndMinWithinBound)
{
	CellRing r(kCells, 1 << 20);
	ASSERT_TRUE(r.Track(kA, CellRingKind::Float16, 0.f, 1.f));
	ASSERT_TRUE(r.BeginSample(0)); ASSERT_TRUE(Put(r, kA, 1.f));
	ASSERT_TRUE(r.BeginSample(1)); ASSERT_TRUE(Put(r, kA, 0.f));
	EXPECT_LE(std::fabs(ReadCell0(r, 0, kA) - 1.f), 1.f / 65535.f);
	EXPECT_LE(std::fabs(ReadCell0(r, 1, kA) - 0.f), 1.f / 65535.f);
}

TEST(CellRing, FloatClampsBothSides)
{
	CellRing r(kCells, 1 << 20);
	MakeFloat(r);
	ASSERT_TRUE(r.BeginSample(0)); ASSERT_TRUE(Put(r, kA, 500.f));
	ASSERT_TRUE(r.BeginSample(1)); ASSERT_TRUE(Put(r, kA, -500.f));
	ASSERT_TRUE(r.BeginSample(2)); ASSERT_TRUE(Put(r, kA, INFINITY));
	EXPECT_NEAR(ReadCell0(r, 0, kA), 100.f, 100.f / 65535.f);
	EXPECT_NEAR(ReadCell0(r, 1, kA), 0.f, 100.f / 65535.f);
	EXPECT_NEAR(ReadCell0(r, 2, kA), 100.f, 100.f / 65535.f);
}

TEST(CellRing, FloatNaNRoundTrips)
{
	CellRing r(kCells, 1 << 20);
	MakeFloat(r);
	ASSERT_TRUE(r.BeginSample(0)); ASSERT_TRUE(Put(r, kA, NAN));
	EXPECT_TRUE(std::isnan(ReadCell0(r, 0, kA)));
}

TEST(CellRing, CategoryRoundTripAndClamp)
{
	CellRing r(kCells, 1 << 20);
	ASSERT_TRUE(r.Track(kB, CellRingKind::Category8, 0, 0));
	const float vals[] = { 0.f, 1.f, 7.f, 200.f, 300.f, -4.f, NAN };
	const float want[] = { 0.f, 1.f, 7.f, 200.f, 255.f, 0.f, 255.f };
	for (uint32_t i = 0; i < 7; ++i)
	{
		ASSERT_TRUE(r.BeginSample(i)); ASSERT_TRUE(Put(r, kB, vals[i]));
		EXPECT_EQ(ReadCell0(r, i, kB), want[i]) << i;
	}
}

TEST(CellRing, PerCellValuesPreserved)
{
	CellRing r(4, 1 << 20);
	ASSERT_TRUE(r.Track(kA, CellRingKind::Float16, 0.f, 100.f));
	const float in[4] = { 10.f, 20.f, 30.f, 40.f };
	ASSERT_TRUE(r.BeginSample(0)); ASSERT_TRUE(r.Append(kA, in, 4));
	float out[4] = {};
	ASSERT_TRUE(r.Read(0, kA, out, 4));
	for (int i = 0; i < 4; ++i) EXPECT_NEAR(out[i], in[i], 100.f / 65535.f);
}

TEST(CellRing, EvictionDropsOldestAndRecordsGap)
{
	CellRing r(kCells, T::BudgetBytesForWindow(3, kCells * 2));
	MakeFloat(r);
	for (uint32_t s = 0; s < 3; ++s) ASSERT_TRUE(Sample(r, s, float(s)));
	EXPECT_EQ(r.GapCount(), 0u);
	EXPECT_EQ(r.EvictionEpisodeCount(), 0u);
	ASSERT_TRUE(Sample(r, 3, 3.f));
	EXPECT_EQ(r.FirstRetained(), 1u);
	EXPECT_EQ(r.LatestSample(), 3u);
	EXPECT_EQ(r.RetainedCount(), 3u);
	EXPECT_FALSE(r.HasSample(0));
	ASSERT_TRUE(Sample(r, 4, 4.f));
	ASSERT_TRUE(Sample(r, 5, 5.f));
	EXPECT_EQ(r.FirstRetained(), 3u);
	ASSERT_EQ(r.GapCount(), 1u);		// adjacent drops merge
	EXPECT_EQ(r.Gap(0).mFirstDropped, 0u);
	EXPECT_EQ(r.Gap(0).mLastDropped, 2u);
	EXPECT_EQ(r.EvictionEpisodeCount(), 1u);	// one Warning for the whole episode
	EXPECT_NEAR(ReadCell0(r, 5, kA), 5.f, 0.01f);
	EXPECT_NEAR(ReadCell0(r, 3, kA), 3.f, 0.01f);
}

TEST(CellRing, EvictionDropsFromAllRingsTogether)
{
	CellRing r(kCells, T::BudgetBytesForWindow(3, kCells * 3));
	MakeFloat(r);
	ASSERT_TRUE(r.Track(kB, CellRingKind::Category8, 0, 0));
	for (uint32_t s = 0; s < 5; ++s)
	{
		ASSERT_TRUE(r.BeginSample(s)); ASSERT_TRUE(Put(r, kA, float(s))); ASSERT_TRUE(Put(r, kB, float(s)));
	}
	std::vector<float> tmp(kCells);
	for (uint32_t s = 0; s < 2; ++s)
	{
		EXPECT_FALSE(r.Read(s, kA, tmp.data(), kCells));
		EXPECT_FALSE(r.Read(s, kB, tmp.data(), kCells));
	}
	for (uint32_t s = 2; s < 5; ++s)
	{
		bool a = false, b = false;
		EXPECT_NEAR(ReadCell0(r, s, kA, &a), float(s), 0.01f);
		EXPECT_EQ(ReadCell0(r, s, kB, &b), float(s));
		EXPECT_TRUE(a); EXPECT_TRUE(b);
	}
}

TEST(CellRing, SeparateEpisodesMakeSeparateGaps)
{
	CellRing r(kCells, T::BudgetBytesForWindow(4, kCells * 2));
	MakeFloat(r);
	for (uint32_t s = 0; s < 6; ++s) ASSERT_TRUE(Sample(r, s, 1.f));	// episode 1 drops 0,1
	EXPECT_EQ(r.EvictionEpisodeCount(), 1u);
	ASSERT_TRUE(r.Track(kB, CellRingKind::Float16, 0.f, 1.f));			// ends the episode; window 2 (drops 2,3)
	EXPECT_EQ(r.EvictionEpisodeCount(), 2u);
	ASSERT_EQ(r.GapCount(), 2u);
	EXPECT_EQ(r.Gap(0).mFirstDropped, 0u);
	EXPECT_EQ(r.Gap(0).mLastDropped, 1u);
	EXPECT_EQ(r.Gap(1).mFirstDropped, 2u);
	EXPECT_EQ(r.Gap(1).mLastDropped, 3u);
}

TEST(CellRing, TrackShrinksWindowAndDropsOldest)
{
	CellRing r(kCells, T::BudgetBytesForWindow(4, kCells * 2));
	MakeFloat(r);
	for (uint32_t s = 0; s < 4; ++s) ASSERT_TRUE(Sample(r, s, float(s)));
	ASSERT_TRUE(r.Track(kB, CellRingKind::Float16, 0.f, 1.f));
	EXPECT_EQ(r.WindowLength(), 2u);
	EXPECT_EQ(r.RetainedCount(), 2u);
	EXPECT_EQ(r.FirstRetained(), 2u);
	EXPECT_EQ(r.LatestSample(), 3u);
	EXPECT_FALSE(r.HasSample(1));
	EXPECT_NEAR(ReadCell0(r, 3, kA), 3.f, 0.01f);
	ASSERT_EQ(r.GapCount(), 1u);
	EXPECT_EQ(r.Gap(0).mFirstDropped, 0u);
	EXPECT_EQ(r.Gap(0).mLastDropped, 1u);
}

TEST(CellRing, LateTrackedLayerHasDataFromNextSampleOnly)
{
	CellRing r(kCells, T::BudgetBytesForWindow(10, kCells * 4));
	MakeFloat(r);
	for (uint32_t s = 0; s < 3; ++s) ASSERT_TRUE(Sample(r, s, float(s)));
	ASSERT_TRUE(r.Track(kB, CellRingKind::Float16, 0.f, 10.f));
	for (uint32_t s = 7; s < 9; ++s)
	{
		ASSERT_TRUE(r.BeginSample(s)); ASSERT_TRUE(Put(r, kA, 1.f)); ASSERT_TRUE(Put(r, kB, 5.f));
	}
	bool ok = true;
	for (uint32_t s = 0; s < 3; ++s)
	{
		ReadCell0(r, s, kB, &ok); EXPECT_FALSE(ok) << s;
		ReadCell0(r, s, kA, &ok); EXPECT_TRUE(ok) << s;
	}
	EXPECT_NEAR(ReadCell0(r, 7, kB, &ok), 5.f, 0.01f);
	EXPECT_TRUE(ok);
	ReadCell0(r, 8, kB, &ok);
	EXPECT_TRUE(ok);
}

TEST(CellRing, UntrackFreesAndGrowsWindow)
{
	CellRing r(kCells, T::BudgetBytesForWindow(4, kCells * 4));
	MakeFloat(r);
	ASSERT_TRUE(r.Track(kB, CellRingKind::Float16, 0.f, 1.f));
	EXPECT_EQ(r.WindowLength(), 4u);
	for (uint32_t s = 0; s < 3; ++s)
	{
		ASSERT_TRUE(r.BeginSample(s)); ASSERT_TRUE(Put(r, kA, float(s))); ASSERT_TRUE(Put(r, kB, 0.5f));
	}
	ASSERT_TRUE(r.Untrack(kB));
	EXPECT_FALSE(r.IsTracked(kB));
	EXPECT_EQ(r.WindowLength(), 8u);
	EXPECT_EQ(r.BytesPerSample(), kCells * 2u);
	EXPECT_EQ(r.RetainedCount(), 3u);
	bool ok = true;
	ReadCell0(r, 1, kB, &ok); EXPECT_FALSE(ok);
	EXPECT_NEAR(ReadCell0(r, 1, kA, &ok), 1.f, 0.01f); EXPECT_TRUE(ok);
	EXPECT_FALSE(r.Untrack(kB));
	EXPECT_EQ(r.BytesUsed(), 8u * kCells * 2u);
}

TEST(CellRing, TrackRejectionsLeaveStateUnchanged)
{
	CellRing r(kCells, T::BudgetBytesForWindow(4, kCells * 2));
	MakeFloat(r);
	for (uint32_t s = 0; s < 6; ++s) ASSERT_TRUE(Sample(r, s, 1.f));
	auto snap = [&] { return std::vector<uint64_t>{ r.TrackedCount(), r.WindowLength(), r.RetainedCount(), r.FirstRetained(), r.LatestSample(), r.BytesPerSample(), r.BytesUsed(), r.GapCount(), r.EvictionEpisodeCount() }; };
	const auto before = snap();
	EXPECT_FALSE(r.Track(kA, CellRingKind::Float16, 0.f, 1.f));			// duplicate
	EXPECT_FALSE(r.Track(kB, CellRingKind::Float16, 1.f, 1.f));			// empty range
	EXPECT_FALSE(r.Track(kB, CellRingKind::Float16, 2.f, 1.f));			// inverted
	EXPECT_FALSE(r.Track(kB, CellRingKind::Float16, NAN, 1.f));
	EXPECT_FALSE(r.Track(kB, CellRingKind::Float16, 0.f, NAN));
	EXPECT_EQ(snap(), before);
	CellRing tiny(kCells, kCells * 2 - 1);									// one sample does not fit
	EXPECT_FALSE(tiny.Track(kA, CellRingKind::Float16, 0.f, 1.f));
	EXPECT_EQ(tiny.TrackedCount(), 0u);
	EXPECT_EQ(tiny.WindowLength(), 0u);
	CellRing r2(kCells, kCells * 2 + 3);									// fits A but not A+B
	ASSERT_TRUE(r2.Track(kA, CellRingKind::Float16, 0.f, 1.f));
	EXPECT_FALSE(r2.Track(kB, CellRingKind::Float16, 0.f, 1.f));
	EXPECT_EQ(r2.TrackedCount(), 1u);
	EXPECT_EQ(r2.WindowLength(), 1u);
}

TEST(CellRing, TrackCapacityReached)
{
	CellRing r(1, 1 << 20);
	char name[16];
	for (uint32_t i = 0; i < CellRing::kMaxLayers; ++i)
	{
		snprintf(name, sizeof(name), "L%u", i);
		ASSERT_TRUE(r.Track(StringCRC(name), CellRingKind::Category8, 0, 0));
	}
	EXPECT_FALSE(r.Track(StringCRC("overflow"), CellRingKind::Category8, 0, 0));
	EXPECT_EQ(r.TrackedCount(), CellRing::kMaxLayers);
}

TEST(CellRing, TruncateAfterDropsNewerSamples)
{
	CellRing r(kCells, 1 << 20);
	MakeFloat(r);
	for (uint32_t s = 0; s < 5; ++s) ASSERT_TRUE(Sample(r, s, float(s)));
	r.TruncateAfter(2);
	EXPECT_EQ(r.LatestSample(), 2u);
	EXPECT_EQ(r.RetainedCount(), 3u);
	EXPECT_FALSE(r.HasSample(3));
	EXPECT_TRUE(r.HasSample(2));
	r.TruncateAfter(10);	// no-op
	EXPECT_EQ(r.RetainedCount(), 3u);
	EXPECT_FALSE(r.BeginSample(2));	// not increasing
	ASSERT_TRUE(Sample(r, 3, 9.f));	// truncated index may be re-appended
	EXPECT_NEAR(ReadCell0(r, 3, kA), 9.f, 0.01f);
	EXPECT_EQ(r.LatestSample(), 3u);
}

TEST(CellRing, TruncateAfterEverythingEmpties)
{
	CellRing r(kCells, 1 << 20);
	MakeFloat(r);
	for (uint32_t s = 5; s < 8; ++s) ASSERT_TRUE(Sample(r, s, 1.f));
	r.TruncateAfter(4);
	EXPECT_TRUE(r.Empty());
	EXPECT_EQ(r.RetainedCount(), 0u);
	ASSERT_TRUE(Sample(r, 5, 2.f));
	EXPECT_EQ(r.FirstRetained(), 5u);
}

TEST(CellRing, TruncateAfterWrappedRing)
{
	CellRing r(kCells, T::BudgetBytesForWindow(3, kCells * 2));
	MakeFloat(r);
	for (uint32_t s = 0; s < 7; ++s) ASSERT_TRUE(Sample(r, s, float(s)));	// retains 4,5,6
	r.TruncateAfter(5);
	EXPECT_EQ(r.FirstRetained(), 4u);
	EXPECT_EQ(r.LatestSample(), 5u);
	ASSERT_TRUE(Sample(r, 6, 60.f));
	EXPECT_NEAR(ReadCell0(r, 4, kA), 4.f, 0.01f);
	EXPECT_NEAR(ReadCell0(r, 6, kA), 60.f, 0.01f);
}

TEST(CellRing, NonContiguousSampleIndices)
{
	CellRing r(kCells, T::BudgetBytesForWindow(3, kCells * 2));
	MakeFloat(r);
	const uint32_t idx[] = { 7, 9, 20, 21 };
	for (uint32_t s : idx) ASSERT_TRUE(Sample(r, s, float(s)));
	EXPECT_EQ(r.FirstRetained(), 9u);
	EXPECT_EQ(r.LatestSample(), 21u);
	EXPECT_FALSE(r.HasSample(7));
	EXPECT_TRUE(r.HasSample(9));
	EXPECT_FALSE(r.HasSample(10));	// skipped index never recorded
	EXPECT_TRUE(r.HasSample(20));
	EXPECT_NEAR(ReadCell0(r, 20, kA), 20.f, 0.01f);
	EXPECT_FALSE(r.BeginSample(21));
	EXPECT_FALSE(r.BeginSample(5));
}

TEST(CellRing, ReadFailureCases)
{
	CellRing r(kCells, 1 << 20);
	MakeFloat(r);
	ASSERT_TRUE(Sample(r, 0, 1.f));
	std::vector<float> v(kCells);
	EXPECT_TRUE(r.Read(0, kA, v.data(), kCells));
	EXPECT_FALSE(r.Read(1, kA, v.data(), kCells));		// never recorded
	EXPECT_FALSE(r.Read(0, kB, v.data(), kCells));		// unknown layer
	EXPECT_FALSE(r.Read(0, kA, v.data(), kCells - 1));	// wrong n
	EXPECT_FALSE(r.Append(kA, v.data(), kCells - 1));
	EXPECT_FALSE(r.Append(kC, v.data(), kCells));
}

TEST(CellRing, BeginSampleRequiresLayers)
{
	CellRing r(kCells, 1 << 20);
	EXPECT_FALSE(r.BeginSample(0));
	EXPECT_TRUE(r.Empty());
}

TEST(CellRing, MemoryNeverExceedsBudget)
{
	const uint64_t budget = 1000;
	CellRing r(kCells, budget);
	MakeFloat(r);
	ASSERT_TRUE(r.Track(kB, CellRingKind::Category8, 0, 0));
	EXPECT_LE(r.BytesUsed(), budget);
	for (uint32_t s = 0; s < 100; ++s)
	{
		ASSERT_TRUE(r.BeginSample(s)); Put(r, kA, 1.f); Put(r, kB, 1.f);
		EXPECT_LE(r.RetainedCount(), r.WindowLength());
	}
}
