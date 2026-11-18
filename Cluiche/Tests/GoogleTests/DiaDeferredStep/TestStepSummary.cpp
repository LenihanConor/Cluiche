#include <gtest/gtest.h>
#include <DiaDeferredStep/StepSummary.h>
#include <DiaObservation/Profile/StepSummaryRecord.h>

using namespace Dia::DeferredStep;
namespace P = Dia::Observation::Profile;

static_assert(sizeof(P::StepSummaryRecord) == 88, "wire size");

namespace
{
	LaunchInfo MakeInfo(uint64_t kickKey = 7)
	{
		LaunchInfo i;
		i.channelId = Dia::Core::StringCRC("chan");
		i.kickKey = kickKey;
		return i;
	}

	RangeTiming R(uint64_t enq, uint64_t admit, uint64_t start, uint64_t end, uint64_t capWait, uint32_t flags = 0)
	{
		RangeTiming t;
		t.enqueueNs = enq; t.admitNs = admit; t.startNs = start; t.endNs = end; t.capWaitNs = capWait; t.flags = flags;
		return t;
	}
}

TEST(DeferredStepSummary, SumsAndMinStart)
{
	RangeTiming t[3] = {
		R(100, 0, 100, 150, 0),
		R(100, 110, 130, 200, 10),
		R(100, 120, 125, 180, 20),
	};
	StepTimes times; times.kickNs = 90; times.readyNs = 210; times.applyNs = 220;
	StepSummaryInputs in; in.applyKey = 42;
	P::StepSummaryRecord r = BuildStepSummary(MakeInfo(), t, 3, times, in);
	EXPECT_EQ(r.owner, Dia::Core::StringCRC("chan").Value());
	EXPECT_EQ(r.applyKey, 42u);
	EXPECT_EQ(r.kickNs, 90u);
	EXPECT_EQ(r.readyNs, 210u);
	EXPECT_EQ(r.applyNs, 220u);
	EXPECT_EQ(r.rangeCount, 3);
	EXPECT_EQ(r.cpuNs, 50u + 70u + 55u);
	EXPECT_EQ(r.capWaitNs, 30u);
	EXPECT_EQ(r.workerWaitNs, 20u + 5u);
	EXPECT_EQ(r.firstRangeStartNs, 100u);
	EXPECT_EQ(r.flags, 0);
}

TEST(DeferredStepSummary, FallbacksAndStolenStayDistinct)
{
	RangeTiming t[4] = {
		R(10, 0, 10, 20, 0),
		R(10, 0, 15, 25, 5, kRangeStolen),
		R(10, 0, 12, 30, 0, kRangeTrySubmitFailed),
		R(10, 0, 14, 22, 4, kRangeStolen),
	};
	P::StepSummaryRecord r = BuildStepSummary(MakeInfo(), t, 4, StepTimes(), StepSummaryInputs());
	EXPECT_EQ(r.stolenRanges, 2);
	EXPECT_EQ(r.inlineFallbacks, 1);
	EXPECT_EQ(r.workerWaitNs, 0u);
	EXPECT_EQ(r.capWaitNs, 9u);
}

TEST(DeferredStepSummary, StalledFlagIffStall)
{
	StepTimes a; a.stallNs = 5;
	StepTimes b;
	EXPECT_TRUE(BuildStepSummary(MakeInfo(), nullptr, 0, a, StepSummaryInputs()).flags & P::StepSummaryFlags::kStalled);
	EXPECT_FALSE(BuildStepSummary(MakeInfo(), nullptr, 0, b, StepSummaryInputs()).flags & P::StepSummaryFlags::kStalled);
}

TEST(DeferredStepSummary, InlineWholly)
{
	StepTimes times; times.kickNs = 100; times.readyNs = 350;
	StepSummaryInputs in; in.ranInlineWholly = true;
	P::StepSummaryRecord r = BuildStepSummary(MakeInfo(), nullptr, 0, times, in);
	EXPECT_EQ(r.rangeCount, 1);
	EXPECT_EQ(r.cpuNs, 250u);
	EXPECT_EQ(r.capWaitNs, 0u);
	EXPECT_EQ(r.workerWaitNs, 0u);
	EXPECT_TRUE(r.flags & P::StepSummaryFlags::kRanInlineWholly);
}

TEST(DeferredStepSummary, RestoredFromSave)
{
	RangeTiming t[1] = { R(1, 2, 3, 4, 1) };
	StepTimes times; times.stallNs = 9;
	StepSummaryInputs in; in.restoredFromSave = true; in.applyKey = 77;
	P::StepSummaryRecord r = BuildStepSummary(MakeInfo(), t, 1, times, in);
	EXPECT_EQ(r.rangeCount, 0);
	EXPECT_EQ(r.cpuNs, 0u);
	EXPECT_EQ(r.stallNs, 0u);
	EXPECT_EQ(r.applyKey, 77u);
	EXPECT_EQ(r.kickNs, 0u);
	EXPECT_EQ(r.readyNs, 0u);
	EXPECT_EQ(r.flags, P::StepSummaryFlags::kRestoredFromSave);
}

TEST(DeferredStepSummary, UnderflowClamps)
{
	RangeTiming t[1] = { R(100, 90, 80, 70, 0) };
	P::StepSummaryRecord r = BuildStepSummary(MakeInfo(), t, 1, StepTimes(), StepSummaryInputs());
	EXPECT_EQ(r.cpuNs, 0u);
	EXPECT_EQ(r.workerWaitNs, 0u);
}

TEST(DeferredStepSummary, KickKeyLowTruncates)
{
	P::StepSummaryRecord r = BuildStepSummary(MakeInfo(0xABCD00001234ull), nullptr, 0, StepTimes(), StepSummaryInputs());
	EXPECT_EQ(r.kickKeyLow, 0x1234u);
}
