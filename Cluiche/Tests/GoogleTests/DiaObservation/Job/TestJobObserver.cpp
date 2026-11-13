#include <gtest/gtest.h>
#include <DiaObservation/Job/JobObserver.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Profile/Profiler.h>
#include <DiaObservation/Testing/ProfileFixture.h>
#include <DiaThreading/JobSystem.h>

#include <atomic>

using Dia::Observation::Job::JobInbox;
using Dia::Observation::Job::JobObserver;
using Dia::Observation::Profile::Profiler;
using Dia::Observation::Profile::ScopeRecord;
using Dia::Threading::JobSystem;
using Dia::Threading::JobTag;
using Dia::Threading::JobTimingRecord;

namespace
{
	constexpr uint32_t kOwner = 0xABCD1234u;

	struct JobObserverTest : ::testing::Test, Dia::Observation::Testing::ProfileFixture {};

	// Waits until the job and its end-hook (record push / unowned count) have completed.
	template <class Pred> void SpinUntil(Pred p)
	{
		for (int i = 0; i < 5000000 && !p(); ++i) std::this_thread::yield();
	}
}

TEST_F(JobObserverTest, ProfileScopeInJobInheritsOwnerSubject)
{
	JobObserver observer;
	JobInbox inbox(16);
	JobObserver::InstallThreadInbox(&inbox);

	JobSystem js;
	js.Initialize(2, nullptr, &observer);

	std::atomic<uint32_t> seenSubject{0};
	std::atomic<uint32_t> recordSubject{0};
	std::atomic<unsigned> recordCount{0};

	JobTag tag; tag.owner = kOwner; tag.tag = 7; tag.range = 2; tag.rangeCount = 4;
	auto h = js.Submit([&]
	{
		{
			DIA_PROFILE_SCOPE("job.section", Dia::Observation::Profile::Category::kAll);
			seenSubject = Profiler::CurrentSubject();
		}
		ScopeRecord recs[16];
		unsigned n = Profiler::Instance().PeekThreadClosedRecords(recs, 16);
		recordCount = n;
		for (unsigned i = 0; i < n; ++i)
			if (recs[i].name == Dia::Core::StringCRC("job.section")) recordSubject = recs[i].subject;
	}, tag);
	js.Wait(h);
	SpinUntil([&] { return inbox.PushedCount() == 1; });

	EXPECT_EQ(seenSubject.load(), kOwner);
	EXPECT_GE(recordCount.load(), 1u);           // scope recorded: worker registered
	EXPECT_EQ(recordSubject.load(), kOwner);

	JobTimingRecord rec{};
	ASSERT_TRUE(inbox.TryPop(rec));
	EXPECT_EQ(rec.owner, kOwner);
	EXPECT_EQ(rec.tag, 7u);
	EXPECT_EQ(rec.range, 2u);
	EXPECT_EQ(rec.rangeCount, 4u);
	EXPECT_LE(rec.submitNs, rec.startNs);
	EXPECT_LE(rec.startNs, rec.endNs);

	js.Shutdown();
	JobObserver::ClearThreadInbox();
	EXPECT_EQ(observer.UnownedCount(), 0u);
}

TEST_F(JobObserverTest, NoInboxCountsUnowned)
{
	JobObserver observer;
	JobObserver::ClearThreadInbox();
	JobSystem js;
	js.Initialize(1, nullptr, &observer);

	std::atomic<bool> ran{false};
	js.Wait(js.Submit([&] { ran = true; }));
	SpinUntil([&] { return observer.UnownedCount() == 1; });
	js.Shutdown();

	EXPECT_TRUE(ran.load());
	EXPECT_EQ(observer.UnownedCount(), 1u);
}

TEST_F(JobObserverTest, NestedSubmitInheritsOwner)
{
	JobObserver observer;
	JobInbox inbox(16);
	JobObserver::InstallThreadInbox(&inbox);

	JobSystem js;
	js.Initialize(2, nullptr, &observer);

	std::atomic<uint32_t> childSubject{0};
	JobTag tag; tag.owner = kOwner;
	auto parent = js.Submit([&]
	{
		auto child = js.Submit([&] { childSubject = Profiler::CurrentSubject(); });
		js.Wait(child);
	}, tag);
	js.Wait(parent);
	SpinUntil([&] { return inbox.PushedCount() == 2; });
	js.Shutdown();
	JobObserver::ClearThreadInbox();

	EXPECT_EQ(childSubject.load(), kOwner);
	JobTimingRecord a{}, b{};
	ASSERT_TRUE(inbox.TryPop(a));
	ASSERT_TRUE(inbox.TryPop(b));
	EXPECT_EQ(a.owner, kOwner);
	EXPECT_EQ(b.owner, kOwner);
}

TEST_F(JobObserverTest, CaptureUsesSubmitterSubject)
{
	JobObserver observer;
	JobSystem js;
	js.Initialize(1, nullptr, &observer);

	std::atomic<uint32_t> seen{0};
	{
		Dia::Observation::Profile::ScopedZone z(Dia::Core::StringCRC("outer"),
			Dia::Observation::Profile::Category::kAll, kOwner);
		js.Wait(js.Submit([&] { seen = Profiler::CurrentSubject(); }));
	}
	js.Shutdown();
	EXPECT_EQ(seen.load(), kOwner);
}
