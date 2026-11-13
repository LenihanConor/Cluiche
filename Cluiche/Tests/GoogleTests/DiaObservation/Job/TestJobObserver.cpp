#include <gtest/gtest.h>
#include <DiaObservation/Job/JobObserver.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Profile/Profiler.h>
#include <DiaObservation/Testing/ProfileFixture.h>
#include <DiaThreading/JobSystem.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <vector>

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

	struct SLOW_JobObserverTest : ::testing::Test, Dia::Observation::Testing::ProfileFixture {};

	// Waits until the job and its end-hook (record push / unowned count) have completed.
	template <class Pred> void SpinUntil(Pred p)
	{
		for (int i = 0; i < 5000000 && !p(); ++i) std::this_thread::yield();
	}
}

TEST_F(SLOW_JobObserverTest, ProfileScopeInJobInheritsOwnerSubject)
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

TEST_F(SLOW_JobObserverTest, NoInboxCountsUnowned)
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

TEST_F(SLOW_JobObserverTest, NestedSubmitInheritsOwner)
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

TEST_F(SLOW_JobObserverTest, CaptureUsesSubmitterSubject)
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

// ---------------------------------------------------------------------------------------------
// Gap audit / edge cases (Job Observation task 5)
// ---------------------------------------------------------------------------------------------

TEST_F(SLOW_JobObserverTest, SubmitFromPlainThreadWithoutInbox_CountedUnowned)
{
	JobObserver observer;
	JobSystem js;
	js.Initialize(2, nullptr, &observer);

	std::atomic<int> ran{0};
	std::thread t([&]
	{
		// Plain thread: no inbox, no profiler registration.
		for (int i = 0; i < 10; ++i) js.Wait(js.Submit([&] { ++ran; }));
	});
	t.join();
	SpinUntil([&] { return observer.UnownedCount() == 10; });
	js.Shutdown();
	EXPECT_EQ(ran.load(), 10);
	EXPECT_EQ(observer.UnownedCount(), 10u);
}

TEST_F(SLOW_JobObserverTest, InboxFullDuringFlood_DropsCountedJobsAllComplete)
{
	JobObserver observer;
	JobInbox inbox(8);
	JobObserver::InstallThreadInbox(&inbox);
	JobSystem js;
	js.Initialize(2, nullptr, &observer);

	constexpr int kJobs = 500;
	std::atomic<int> ran{0};
	std::vector<Dia::Threading::JobHandle> hs;
	for (int i = 0; i < kJobs; ++i) hs.push_back(js.Submit([&] { ++ran; }));
	for (auto& h : hs) js.Wait(h);
	js.Shutdown();
	JobObserver::ClearThreadInbox();

	EXPECT_EQ(ran.load(), kJobs);
	EXPECT_EQ(inbox.PushedCount() + inbox.DroppedCount(), static_cast<uint64_t>(kJobs));
	EXPECT_EQ(inbox.PushedCount(), 8u);
	EXPECT_EQ(inbox.DroppedCount(), static_cast<uint64_t>(kJobs - 8));
}

TEST_F(SLOW_JobObserverTest, ThrowingJob_ScopeStackNotCorrupted)
{
	JobObserver observer;
	JobInbox inbox(16);
	JobObserver::InstallThreadInbox(&inbox);
	JobSystem js;
	js.Initialize(1, nullptr, &observer);

	js.Wait(js.Submit([] { throw std::runtime_error("boom"); }));
	std::atomic<uint32_t> subject{1};
	JobTag tag; tag.owner = kOwner;
	js.Wait(js.Submit([&] { subject = Profiler::CurrentSubject(); }, tag));
	SpinUntil([&] { return inbox.PushedCount() == 2; });
	js.Shutdown();
	JobObserver::ClearThreadInbox();

	EXPECT_EQ(inbox.PushedCount(), 2u);
	EXPECT_EQ(subject.load(), kOwner);   // exactly the second job's own scope, no leftover from the thrown job
}

TEST_F(SLOW_JobObserverTest, NestingDeeperThanScopeStack_DoesNotCrash)
{
	JobObserver observer;
	JobInbox inbox(64);
	JobObserver::InstallThreadInbox(&inbox);
	JobSystem js;
	js.Initialize(1, nullptr, &observer);

	// Drive the hooks nested on one thread past the scope-stack capacity (8).
	std::atomic<bool> done{false};
	js.Wait(js.Submit([&]
	{
		Dia::Threading::JobContext ctx = observer.CaptureContext();
		Dia::Threading::JobTimingRecord rec{};
		for (int i = 0; i < 12; ++i) observer.OnJobBegin(ctx);
		(void)observer.CaptureContext();            // depth > capacity: must not index out of range
		for (int i = 0; i < 12; ++i) observer.OnJobEnd(ctx, rec);
		done = true;
	}));
	js.Shutdown();
	JobObserver::ClearThreadInbox();
	EXPECT_TRUE(done.load());
}

TEST_F(SLOW_JobObserverTest, ReinitializeWithDifferentObserver_UsesNewObserver)
{
	JobObserver a, b;
	JobSystem js;
	js.Initialize(1, nullptr, &a);
	js.Wait(js.Submit([] {}));
	js.Shutdown();
	SpinUntil([&] { return a.UnownedCount() == 1; });
	js.Initialize(1, nullptr, &b);
	js.Wait(js.Submit([] {}));
	js.Wait(js.Submit([] {}));
	SpinUntil([&] { return b.UnownedCount() == 2; });
	js.Shutdown();
	EXPECT_EQ(a.UnownedCount(), 1u);
	EXPECT_EQ(b.UnownedCount(), 2u);
}

TEST_F(SLOW_JobObserverTest, SubmitFromJobOnInboxlessThread_ChildHasNoInbox)
{
	JobObserver observer;
	JobObserver::ClearThreadInbox();
	JobSystem js;
	js.Initialize(2, nullptr, &observer);
	js.Wait(js.Submit([&] { js.Wait(js.Submit([] {})); }));
	SpinUntil([&] { return observer.UnownedCount() == 2; });
	js.Shutdown();
	EXPECT_EQ(observer.UnownedCount(), 2u);
}

// ---------------------------------------------------------------------------------------------
// Benchmarks (AC5 report, AC6 gate). Gate only in optimised builds.
// ---------------------------------------------------------------------------------------------

namespace
{
	using BenchClock = std::chrono::steady_clock;
	double NsSince(BenchClock::time_point t0) { return std::chrono::duration<double, std::nano>(BenchClock::now() - t0).count(); }
}

TEST_F(SLOW_JobObserverTest, Benchmark_ObserverOverheadPerJob)
{
	// (a) Direct cost of the observer hooks for one job: capture + begin + end + record write
	//     (plus two clock reads, as JobSystem adds), on one thread.
	constexpr int kIters = 1000000;
	JobObserver observer;
	JobInbox inbox(4096);
	JobObserver::InstallThreadInbox(&inbox);
	static Dia::Threading::JobTimingRecord scratch[1024];

	auto t0 = BenchClock::now();
	for (int i = 0; i < kIters; ++i)
	{
		Dia::Threading::JobContext ctx = observer.CaptureContext();
		const auto s = BenchClock::now();
		observer.OnJobBegin(ctx);
		Dia::Threading::JobTimingRecord rec{};
		rec.submitNs = static_cast<uint64_t>(s.time_since_epoch().count());
		rec.endNs = static_cast<uint64_t>(BenchClock::now().time_since_epoch().count());
		observer.OnJobEnd(ctx, rec);
		if ((i & 1023) == 1023) inbox.Drain(scratch, 1024);
	}
	const double hookNsPerJob = NsSince(t0) / kIters;
	JobObserver::ClearThreadInbox();

	// (b) End-to-end: Submit loop + completion, 10k empty jobs, null observer vs observer.
	auto flood = [](Dia::Threading::IJobObserver* obs, double& submitNs, double& totalNs)
	{
		constexpr int kJobs = 10000;
		JobSystem js;
		js.Initialize(2, nullptr, obs);
		std::vector<Dia::Threading::JobHandle> hs;
		hs.reserve(kJobs);
		auto a = BenchClock::now();
		for (int i = 0; i < kJobs; ++i) hs.push_back(js.Submit([] {}));
		submitNs = NsSince(a) / kJobs;
		for (auto& h : hs) js.Wait(h);
		totalNs = NsSince(a) / kJobs;
		js.Shutdown();
	};
	JobObserver floodObserver;
	JobInbox floodInbox(16384);
	JobObserver::InstallThreadInbox(&floodInbox);
	double nullSubmit = 0, nullTotal = 0, obsSubmit = 0, obsTotal = 0;
	flood(nullptr, nullSubmit, nullTotal);        // warm-up
	flood(nullptr, nullSubmit, nullTotal);
	flood(&floodObserver, obsSubmit, obsTotal);
	JobObserver::ClearThreadInbox();

	std::printf("[BENCH] observer hooks: %.1f ns/job (AC6, gate 200 ns in Release)\n", hookNsPerJob);
	std::printf("[BENCH] 10k flood, null observer: submit %.1f ns/job, total %.1f ns/job (AC5 baseline)\n", nullSubmit, nullTotal);
	std::printf("[BENCH] 10k flood, JobObserver:   submit %.1f ns/job, total %.1f ns/job\n", obsSubmit, obsTotal);

#ifdef NDEBUG
	EXPECT_LT(hookNsPerJob, 200.0);
#endif
}
