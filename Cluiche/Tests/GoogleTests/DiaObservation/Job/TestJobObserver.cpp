#include <gtest/gtest.h>
#include <DiaObservation/Job/JobObserver.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Profile/Profiler.h>
#include <DiaObservation/Profile/ProfileFileSink.h>
#include <DiaObservation/Testing/ProfileFixture.h>
#include <DiaThreading/JobSystem.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <iterator>
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

// ---------------------------------------------------------------------------------------------
// B1 / AB1 / AB2: worker thread names and worker scopes in profile.jsonl
// ---------------------------------------------------------------------------------------------

namespace
{
	std::string ReadJsonl(const char* path)
	{
		std::ifstream f(path);
		return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	}

	size_t CountOf(const std::string& hay, const std::string& needle)
	{
		size_t n = 0;
		for (size_t p = hay.find(needle); p != std::string::npos; p = hay.find(needle, p + 1)) ++n;
		return n;
	}
}

TEST_F(SLOW_JobObserverTest, AB2_EachWorkerHasExactlyOneThreadNameRecord)
{
	Profiler::ClearThreadNames();
	JobObserver observer;
	JobSystem js;
	js.Initialize(3, nullptr, &observer);

	std::atomic<int> arrived{0};
	std::vector<Dia::Threading::JobHandle> hs;
	for (int i = 0; i < 3; ++i)
		hs.push_back(js.Submit([&]
		{
			++arrived;
			for (int s = 0; s < 5000000 && arrived.load() < 3; ++s) std::this_thread::yield();
		}));
	for (auto& h : hs) js.Wait(h);
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	Profiler::Instance().BeginFrame();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	js.Shutdown();
	Profiler::Instance().Stop();
	const std::string content = ReadJsonl(mJsonlPath);

	for (int w = 0; w < 3; ++w)
	{
		EXPECT_EQ(CountOf(content, "\"name\":\"Worker " + std::to_string(w) + "\"}"), 1u) << "worker " << w << " " << content;
	}
	EXPECT_EQ(CountOf(content, "\"record_type\":\"thread_name\""), 3u);
	Profiler::ClearThreadNames();
}

TEST_F(SLOW_JobObserverTest, AB1_WorkerScopeCarriesWorkerThreadIdAndOwnerSubject)
{
	Profiler::ClearThreadNames();
	Profiler::ClearSubjectNames();
	ASSERT_TRUE(Profiler::RegisterSubjectName(kOwner, "ab1.owner"));
	JobObserver observer;
	JobSystem js;
	js.Initialize(1, nullptr, &observer);

	std::atomic<uint32_t> workerTid{0};
	JobTag tag; tag.owner = kOwner;
	js.Wait(js.Submit([&]
	{
		DIA_PROFILE_SCOPE("ab1.inner", Dia::Observation::Profile::Category::kAll);
		workerTid = Profiler::CurrentThreadId();
	}, tag));
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	Profiler::Instance().BeginFrame();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	js.Shutdown();
	Profiler::Instance().Stop();
	const std::string content = ReadJsonl(mJsonlPath);

	ASSERT_NE(workerTid.load(), 0u);
	ASSERT_NE(workerTid.load(), Profiler::CurrentThreadId());
	const std::string inner = std::to_string(Dia::Core::StringCRC("ab1.inner").Value());
	size_t pos = content.find("\"scope_name\":" + inner + ",");
	ASSERT_NE(pos, std::string::npos) << content;
	size_t lineStart = content.rfind('\n', pos);
	lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;
	const std::string line = content.substr(lineStart, content.find('\n', pos) - lineStart);
	EXPECT_NE(line.find("\"thread_id\":" + std::to_string(workerTid.load()) + ","), std::string::npos) << line;
	EXPECT_NE(line.find("\"subject\":\"ab1.owner\""), std::string::npos) << line;
	// the job.run scope also carries the subject
	const std::string jobRun = std::to_string(Dia::Core::StringCRC("job.run").Value());
	EXPECT_NE(content.find("\"scope_name\":" + jobRun + ","), std::string::npos);
	Profiler::ClearThreadNames();
}

// ---------------------------------------------------------------------------------------------
// B2 / AB3: profile_job records written from the worker's ring via the drain
// ---------------------------------------------------------------------------------------------

namespace
{
	std::vector<std::string> LinesWith(const std::string& content, const std::string& needle)
	{
		std::vector<std::string> out;
		size_t start = 0;
		while (start < content.size())
		{
			size_t end = content.find('\n', start);
			if (end == std::string::npos) end = content.size();
			std::string line = content.substr(start, end - start);
			if (line.find(needle) != std::string::npos) out.push_back(line);
			start = end + 1;
		}
		return out;
	}

	long long NumField(const std::string& line, const char* key)
	{
		const std::string k = std::string("\"") + key + "\":";
		size_t p = line.find(k);
		if (p == std::string::npos) return -1;
		return std::stoll(line.substr(p + k.size()));
	}

	// Runs n jobs (each tagged), then drains normally (BeginFrame + wait) before shutdown.
	void RunJobsAndDrain(JobSystem& js, int n, const JobTag& tag, bool beginFrame = true)
	{
		std::vector<Dia::Threading::JobHandle> hs;
		for (int i = 0; i < n; ++i) hs.push_back(js.Submit([] {}, tag));
		for (auto& h : hs) js.Wait(h);
		if (beginFrame)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			Profiler::Instance().BeginFrame();
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}
}

TEST_F(SLOW_JobObserverTest, AB3_ProfilingOn_NJobsWriteExactlyNProfileJobLines)
{
	Profiler::ClearThreadNames();
	Profiler::ClearSubjectNames();
	ASSERT_TRUE(Profiler::RegisterSubjectName(kOwner, "ab3.owner"));
	JobObserver observer;
	JobSystem js;
	js.Initialize(2, nullptr, &observer);

	JobTag tag; tag.owner = kOwner; tag.tag = 4; tag.range = 2; tag.rangeCount = 8;
	constexpr int kN = 20;
	RunJobsAndDrain(js, kN, tag);
	js.Shutdown();
	Profiler::Instance().Stop();
	const std::string content = ReadJsonl(mJsonlPath);

	const auto lines = LinesWith(content, "\"record_type\":\"profile_job\"");
	ASSERT_EQ(lines.size(), static_cast<size_t>(kN)) << content;
	for (const auto& l : lines)
	{
		EXPECT_NE(l.find("\"owner\":\"ab3.owner\""), std::string::npos) << l;
		EXPECT_EQ(NumField(l, "tag"), 4) << l;
		EXPECT_EQ(NumField(l, "range"), 2) << l;
		EXPECT_EQ(NumField(l, "range_count"), 8) << l;
		const long long s = NumField(l, "submit_unix_nano");
		const long long b = NumField(l, "start_unix_nano");
		const long long e = NumField(l, "end_unix_nano");
		EXPECT_GT(s, 0) << l;
		EXPECT_LE(s, b) << l;
		EXPECT_LE(b, e) << l;
		EXPECT_GT(NumField(l, "thread_id"), 0) << l;
	}
	Profiler::ClearThreadNames();
	Profiler::ClearSubjectNames();
}

TEST_F(SLOW_JobObserverTest, AB3_ProfilingOff_NoProfileJobLines)
{
	Profiler::ClearThreadNames();
	JobObserver observer;
	JobSystem js;
	js.Initialize(2, nullptr, &observer);
	Profiler::Instance().SetActiveMask(Dia::Observation::Profile::Category::kNone);

	JobTag tag; tag.owner = kOwner;
	RunJobsAndDrain(js, 10, tag);
	js.Shutdown();
	Profiler::Instance().Stop();
	EXPECT_EQ(CountOf(ReadJsonl(mJsonlPath), "profile_job"), 0u);
	Profiler::ClearThreadNames();
}

TEST_F(SLOW_JobObserverTest, AB3_ProfilerStopped_NoProfileJobLines)
{
	Profiler::ClearThreadNames();
	JobObserver observer;
	JobSystem js;
	js.Initialize(2, nullptr, &observer);
	Profiler::Instance().Stop();

	JobTag tag; tag.owner = kOwner;
	RunJobsAndDrain(js, 10, tag, false);
	js.Shutdown();
	EXPECT_EQ(CountOf(ReadJsonl(mJsonlPath), "profile_job"), 0u);
	Profiler::ClearThreadNames();
}

TEST_F(SLOW_JobObserverTest, AB3_UnknownOwner_WrittenAsHex)
{
	Profiler::ClearThreadNames();
	Profiler::ClearSubjectNames();
	JobObserver observer;
	JobSystem js;
	js.Initialize(1, nullptr, &observer);

	JobTag tag; tag.owner = kOwner;
	RunJobsAndDrain(js, 1, tag);
	js.Shutdown();
	Profiler::Instance().Stop();
	const auto lines = LinesWith(ReadJsonl(mJsonlPath), "\"record_type\":\"profile_job\"");
	ASSERT_EQ(lines.size(), 1u);
	EXPECT_NE(lines[0].find("\"owner\":\"0xabcd1234\""), std::string::npos) << lines[0];
	Profiler::ClearThreadNames();
}

TEST_F(SLOW_JobObserverTest, AB3_SubmitThreadIsSubmitterIdAndMatchesScopeThreadId_WorkerWritesRecord)
{
	Profiler::ClearThreadNames();
	JobObserver observer;
	JobSystem js;
	js.Initialize(1, nullptr, &observer);

	const uint32_t mainTid = Profiler::CurrentThreadId();
	JobTag tag; tag.owner = kOwner;
	{
		DIA_PROFILE_SCOPE("ab3.submitter", Dia::Observation::Profile::Category::kAll);
		js.Wait(js.Submit([] {}, tag));
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	Profiler::Instance().BeginFrame();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	js.Shutdown();
	Profiler::Instance().Stop();
	const std::string content = ReadJsonl(mJsonlPath);

	const auto lines = LinesWith(content, "\"record_type\":\"profile_job\"");
	ASSERT_EQ(lines.size(), 1u) << content;
	EXPECT_EQ(NumField(lines[0], "submit_thread"), static_cast<long long>(mainTid)) << lines[0];
	// The record came from the worker's ring, not the submitter's.
	EXPECT_NE(NumField(lines[0], "thread_id"), static_cast<long long>(mainTid)) << lines[0];
	// submit_thread is in the same id space as thread_id of scope lines.
	const std::string scopeName = std::to_string(Dia::Core::StringCRC("ab3.submitter").Value());
	const auto scopeLines = LinesWith(content, "\"scope_name\":" + scopeName + ",");
	ASSERT_EQ(scopeLines.size(), 1u) << content;
	EXPECT_EQ(NumField(scopeLines[0], "thread_id"), NumField(lines[0], "submit_thread"));
	Profiler::Instance();
	Profiler::ClearThreadNames();
}

TEST_F(SLOW_JobObserverTest, AB3_JobRecordsNeverLandInSubmittingThreadsScopeRing)
{
	JobObserver observer;
	JobSystem js;
	js.Initialize(1, nullptr, &observer);
	ScopeRecord before[512];
	const unsigned nBefore = Profiler::Instance().PeekThreadClosedRecords(before, 512);
	JobTag tag; tag.owner = kOwner;
	RunJobsAndDrain(js, 5, tag, false);
	EXPECT_EQ(Profiler::Instance().PeekThreadJobRecordCount(), 0u);
	ScopeRecord after[512];
	EXPECT_EQ(Profiler::Instance().PeekThreadClosedRecords(after, 512), nBefore);
	js.Shutdown();
}

TEST_F(SLOW_JobObserverTest, AB3_JobRingFull_DropsCountedAndDoesNotBlock)
{
	const uint64_t droppedBefore = Profiler::DroppedJobRecords();
	constexpr unsigned kPush = Dia::Observation::Profile::Profiler::kJobRingCapacity + 100;
	std::atomic<bool> finished{false};
	std::thread t([&]
	{
		Profiler::Instance().RegisterThreadScopeBuffer();
		Dia::Observation::Profile::ProfileJobRecord r{};
		r.startNs = 1;
		for (unsigned i = 0; i < kPush; ++i) Profiler::Instance().RecordJob(r);
		EXPECT_EQ(Profiler::Instance().PeekThreadJobRecordCount(), Dia::Observation::Profile::Profiler::kJobRingCapacity - 1);
		finished = true;
		Profiler::Instance().UnregisterThreadScopeBuffer();
	});
	t.join();
	EXPECT_TRUE(finished.load());
	EXPECT_EQ(Profiler::DroppedJobRecords() - droppedBefore,
	          static_cast<uint64_t>(kPush - (Dia::Observation::Profile::Profiler::kJobRingCapacity - 1)));
}

TEST_F(SLOW_JobObserverTest, AB3_WorkerStopFlushesUndrainedJobRecords)
{
	Profiler::ClearThreadNames();
	JobObserver observer;
	JobSystem js;
	js.Initialize(2, nullptr, &observer);
	JobTag tag; tag.owner = kOwner;
	RunJobsAndDrain(js, 12, tag, false);   // no BeginFrame: nothing drained by the frame loop
	js.Shutdown();                         // OnWorkerStop must flush
	Profiler::Instance().Stop();
	EXPECT_EQ(CountOf(ReadJsonl(mJsonlPath), "\"record_type\":\"profile_job\""), 12u);
	Profiler::ClearThreadNames();
}

// ---------------------------------------------------------------------------------------------
// Task 5 gap audit: edge cases for worker-thread profiling (profiling-worker-threads plan)
// ---------------------------------------------------------------------------------------------

namespace
{
	std::string WriteTempPath(const char* name) { return std::string("profiler-edge-") + name + ".jsonl"; }
}

TEST_F(SLOW_JobObserverTest, Edge_ThrowingJob_StillWritesProfileJobAndPopsScope)
{
	Profiler::ClearThreadNames();
	JobObserver observer;
	JobSystem js;
	js.Initialize(1, nullptr, &observer);

	JobTag tag; tag.owner = kOwner; tag.tag = 9;
	js.Wait(js.Submit([] { throw std::runtime_error("boom"); }, tag));
	std::atomic<uint32_t> after{1};
	js.Wait(js.Submit([&] { after = Profiler::CurrentSubject(); }));   // no owner: must not inherit a leaked scope
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	Profiler::Instance().BeginFrame();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	js.Shutdown();
	Profiler::Instance().Stop();
	const std::string content = ReadJsonl(mJsonlPath);

	const auto lines = LinesWith(content, "\"record_type\":\"profile_job\"");
	ASSERT_EQ(lines.size(), 2u) << content;
	size_t withTag9 = 0;
	for (const auto& l : lines) if (NumField(l, "tag") == 9) ++withTag9;
	EXPECT_EQ(withTag9, 1u);
	EXPECT_EQ(after.load(), 0u);
	Profiler::ClearThreadNames();
}

TEST_F(SLOW_JobObserverTest, Edge_InlineHooksOnRegisteredNonWorkerThread_UseThatThreadsRing)
{
	// TrySubmit never runs a job inline today, so this drives the same hooks a caller-runs fallback would:
	// begin/end on a registered, non-worker thread (the fixture's main thread).
	Profiler::ClearThreadNames();
	JobObserver observer;
	JobInbox inbox(8);
	JobObserver::InstallThreadInbox(&inbox);

	const uint32_t tid = Profiler::CurrentThreadId();
	Dia::Threading::JobContext ctx = observer.CaptureContext();
	ctx.attribution.owner = kOwner;
	ctx.attribution.tag = 3;
	Dia::Threading::JobTimingRecord rec{};
	rec.submitNs = 100; rec.startNs = 200; rec.endNs = 300;
	rec.owner = kOwner; rec.tag = 3;
	observer.OnJobBegin(ctx);
	observer.OnJobEnd(ctx, rec);
	JobObserver::ClearThreadInbox();

	EXPECT_EQ(Profiler::Instance().PeekThreadJobRecordCount(), 1u);   // landed in THIS thread's ring
	EXPECT_EQ(inbox.PushedCount(), 1u);
	Profiler::Instance().BeginFrame();
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	EXPECT_EQ(Profiler::Instance().PeekThreadJobRecordCount(), 0u);   // drained
	Profiler::Instance().Stop();
	const std::string content = ReadJsonl(mJsonlPath);
	const auto lines = LinesWith(content, "\"record_type\":\"profile_job\"");
	ASSERT_EQ(lines.size(), 1u) << content;
	EXPECT_EQ(NumField(lines[0], "thread_id"), static_cast<long long>(tid));
	EXPECT_EQ(NumField(lines[0], "submit_thread"), static_cast<long long>(tid));
	EXPECT_EQ(NumField(lines[0], "start_unix_nano"), 200);
	EXPECT_NE(content.find("\"scope_name\":" + std::to_string(Dia::Core::StringCRC("job.run").Value()) + ","), std::string::npos);
	Profiler::ClearThreadNames();
}

TEST_F(SLOW_JobObserverTest, Edge_StopWhileWorkersMidJob_NoCrashAndLateRecordsDropped)
{
	Profiler::ClearThreadNames();
	JobObserver observer;
	JobSystem js;
	js.Initialize(3, nullptr, &observer);

	std::atomic<int> started{0};
	std::atomic<bool> release{false};
	std::vector<Dia::Threading::JobHandle> hs;
	for (int i = 0; i < 3; ++i)
		hs.push_back(js.Submit([&]
		{
			DIA_PROFILE_SCOPE("edge.mid", Dia::Observation::Profile::Category::kAll);
			++started;
			for (int s = 0; s < 50000000 && !release.load(); ++s) std::this_thread::yield();
		}));
	SpinUntil([&] { return started.load() == 3; });
	ASSERT_EQ(started.load(), 3);

	Profiler::Instance().Stop();      // sink deleted while 3 workers are inside jobs
	release = true;
	for (auto& h : hs) js.Wait(h);
	js.Shutdown();                    // OnWorkerStop -> Unregister with no sink
	EXPECT_FALSE(Profiler::Instance().IsStarted());
	EXPECT_EQ(CountOf(ReadJsonl(mJsonlPath), "\"record_type\":\"profile_job\""), 0u);   // ended after Stop: dropped
	Profiler::ClearThreadNames();
}

TEST_F(SLOW_JobObserverTest, Edge_StopDuringSubmitFlood_NoCrash)
{
	Profiler::ClearThreadNames();
	JobObserver observer;
	JobSystem js;
	js.Initialize(4, nullptr, &observer);
	std::atomic<int> ran{0};
	std::vector<Dia::Threading::JobHandle> hs;
	for (int i = 0; i < 5000; ++i) hs.push_back(js.Submit([&] { DIA_PROFILE_SCOPE("edge.flood", Dia::Observation::Profile::Category::kAll); ++ran; }));
	Profiler::Instance().BeginFrame();
	Profiler::Instance().Stop();   // races with workers recording and the drain
	for (auto& h : hs) js.Wait(h);
	js.Shutdown();
	EXPECT_EQ(ran.load(), 5000);
	Profiler::ClearThreadNames();
}

TEST_F(SLOW_JobObserverTest, Edge_WorkerStopWithEmptyRings_NoRecordsNoCrash)
{
	Profiler::ClearThreadNames();
	JobObserver observer;
	JobSystem js;
	js.Initialize(3, nullptr, &observer);
	std::this_thread::sleep_for(std::chrono::milliseconds(30));
	js.Shutdown();
	Profiler::Instance().Stop();
	const std::string content = ReadJsonl(mJsonlPath);
	EXPECT_EQ(CountOf(content, "profile_job"), 0u);
	EXPECT_EQ(CountOf(content, "\"record_type\":\"thread_name\""), 0u);   // names announced lazily, with records
	Profiler::ClearThreadNames();
}

TEST_F(SLOW_JobObserverTest, Edge_StartStopStart_WorkerRegistrationsSurviveRestart)
{
	Profiler::ClearThreadNames();
	JobObserver observer;
	JobSystem js;
	js.Initialize(2, nullptr, &observer);
	JobTag tag; tag.owner = kOwner;
	RunJobsAndDrain(js, 4, tag);
	Profiler::Instance().Stop();
	EXPECT_EQ(CountOf(ReadJsonl(mJsonlPath), "\"record_type\":\"profile_job\""), 4u);

	const std::string path2 = WriteTempPath("restart");
	std::remove(path2.c_str());
	ASSERT_TRUE(Profiler::Instance().Start(path2.c_str(), Dia::Observation::Profile::Category::kAll, "restart", 0));
	RunJobsAndDrain(js, 6, tag);              // same workers, still registered, new sink
	js.Shutdown();
	Profiler::Instance().Stop();
	const std::string second = ReadJsonl(path2.c_str());
	EXPECT_EQ(CountOf(second, "\"record_type\":\"profile_job\""), 6u) << second;
	EXPECT_GE(CountOf(second, "\"record_type\":\"thread_name\""), 2u) << second;   // re-announced for the new sink
	EXPECT_EQ(CountOf(second, "\"name\":\"Worker 0\"") + CountOf(second, "\"name\":\"Worker 1\""), 2u) << second;
	std::remove(path2.c_str());
	Profiler::ClearThreadNames();
}

TEST_F(SLOW_JobObserverTest, Edge_ThreadNameTableFullWithRealWorkers_NoCrashStillRecords)
{
	Profiler::ClearThreadNames();
	for (unsigned i = 0; i < Profiler::kMaxThreadNames; ++i) ASSERT_TRUE(Profiler::RegisterThreadName(900000u + i, "Filler"));
	JobObserver observer;
	JobSystem js;
	js.Initialize(2, nullptr, &observer);   // OnWorkerStart: RegisterThreadName returns false, ignored
	JobTag tag; tag.owner = kOwner;
	RunJobsAndDrain(js, 5, tag);
	js.Shutdown();
	Profiler::Instance().Stop();
	const std::string content = ReadJsonl(mJsonlPath);
	EXPECT_EQ(CountOf(content, "\"record_type\":\"profile_job\""), 5u);
	EXPECT_EQ(CountOf(content, "\"name\":\"Worker "), 0u);
	Profiler::ClearThreadNames();
}

// --- ProfileFileSink direct edge cases (no threads needed) ---

TEST(ProfilerSinkEdge, AnnouncedSetFull_129thThreadDoesNotCrashOrAnnounce)
{
	using namespace Dia::Observation::Profile;
	const std::string path = WriteTempPath("announce");
	std::remove(path.c_str());
	Profiler::ClearThreadNames();
	{
		std::unique_ptr<ProfileFileSink> sink(new ProfileFileSink(path.c_str(), nullptr, "t", 0));
		ASSERT_TRUE(sink->IsOpen());
		for (uint32_t i = 0; i < 128; ++i) Profiler::RegisterThreadName(7000u + i, "A");
		ProfileJobRecord r{}; r.startNs = 1; r.submitNs = 1; r.endNs = 2;
		for (uint32_t i = 0; i < 128; ++i) { r.threadId = 7000u + i; r.submitThread = 7000u + i; sink->OnJobRecord(r); }
		// 128 fresh ids: the announced set is full, so none is announced (and no crash).
		Profiler::ClearThreadNames();
		for (uint32_t i = 0; i < 128; ++i) Profiler::RegisterThreadName(8000u + i, "B");
		for (uint32_t i = 0; i < 128; ++i) { r.threadId = 8000u + i; r.submitThread = 8000u + i; sink->OnJobRecord(r); }
	}
	const std::string content = ReadJsonl(path.c_str());
	EXPECT_EQ(CountOf(content, "\"record_type\":\"thread_name\""), 128u);
	EXPECT_EQ(CountOf(content, "\"name\":\"B\""), 0u);
	EXPECT_EQ(CountOf(content, "\"record_type\":\"profile_job\""), 256u);   // records still written
	std::remove(path.c_str());
	Profiler::ClearThreadNames();
}

TEST(ProfilerSinkEdge, SubjectTableFull_JobOwnerWrittenAsHex)
{
	using namespace Dia::Observation::Profile;
	const std::string path = WriteTempPath("subjfull");
	std::remove(path.c_str());
	Profiler::ClearSubjectNames();
	for (uint32_t i = 0; i < Profiler::kMaxSubjectNames; ++i) ASSERT_TRUE(Profiler::RegisterSubjectName(0xD0000000u + i, "Filler"));
	EXPECT_FALSE(Profiler::RegisterSubjectName(0xE0000001u, "Late"));
	{
		std::unique_ptr<ProfileFileSink> sink(new ProfileFileSink(path.c_str(), nullptr, "t", 0));
		ProfileJobRecord r{}; r.owner = 0xE0000001u; r.threadId = 1; r.submitThread = 1; r.startNs = 1; r.submitNs = 1; r.endNs = 2;
		sink->OnJobRecord(r);
		r.owner = 0xD0000003u;   // registered one still resolves
		sink->OnJobRecord(r);
	}
	const std::string content = ReadJsonl(path.c_str());
	EXPECT_NE(content.find("\"owner\":\"0xe0000001\""), std::string::npos) << content;
	EXPECT_NE(content.find("\"owner\":\"Filler\""), std::string::npos) << content;
	std::remove(path.c_str());
	Profiler::ClearSubjectNames();
}

TEST(ProfilerSinkEdge, SubmitThreadZeroOrUnknown_WrittenWithoutThreadNameLine)
{
	using namespace Dia::Observation::Profile;
	const std::string path = WriteTempPath("submit0");
	std::remove(path.c_str());
	Profiler::ClearThreadNames();
	Profiler::RegisterThreadName(42u, "Worker 0");
	{
		std::unique_ptr<ProfileFileSink> sink(new ProfileFileSink(path.c_str(), nullptr, "t", 0));
		ProfileJobRecord r{}; r.threadId = 42u; r.startNs = 5; r.submitNs = 4; r.endNs = 6;
		r.submitThread = 0;        sink->OnJobRecord(r);
		r.submitThread = 99999u;   sink->OnJobRecord(r);   // never named
	}
	const std::string content = ReadJsonl(path.c_str());
	const auto lines = LinesWith(content, "\"record_type\":\"profile_job\"");
	ASSERT_EQ(lines.size(), 2u) << content;
	EXPECT_EQ(NumField(lines[0], "submit_thread"), 0);
	EXPECT_EQ(NumField(lines[1], "submit_thread"), 99999);
	EXPECT_EQ(CountOf(content, "\"record_type\":\"thread_name\""), 1u);   // only the worker
	std::remove(path.c_str());
	Profiler::ClearThreadNames();
}

// ---------------------------------------------------------------------------------------------
// Flood benchmark (open question 1). Reports ring drops; does not gate.
// ---------------------------------------------------------------------------------------------

namespace
{
	struct FloodResult { uint64_t jobs, jobLines, ringDrops, scopeLines, scopeDrops; double wallMs; };

	// frames > 1: `perFrame` jobs per frame, 16 ms sleep + BeginFrame per frame.
	// frames == 1: burst, everything submitted inside one frame.
	FloodResult RunFlood(const char* jsonlPath, unsigned workers, unsigned frames, unsigned perFrame)
	{
		Profiler::ClearThreadNames();
		JobObserver observer;
		JobSystem js;
		js.Initialize(workers, nullptr, &observer);
		const uint64_t dropsBefore = Profiler::DroppedJobRecords();
		JobTag tag; tag.owner = kOwner;
		std::vector<Dia::Threading::JobHandle> hs;
		hs.reserve(static_cast<size_t>(frames) * perFrame);
		const auto t0 = std::chrono::steady_clock::now();
		for (unsigned f = 0; f < frames; ++f)
		{
			for (unsigned i = 0; i < perFrame; ++i) hs.push_back(js.Submit([] {}, tag));
			if (frames > 1) std::this_thread::sleep_for(std::chrono::milliseconds(16));
			Profiler::Instance().BeginFrame();
		}
		for (auto& h : hs) js.Wait(h);
		const double wall = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
		const uint64_t drops = Profiler::DroppedJobRecords() - dropsBefore;
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		Profiler::Instance().BeginFrame();
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		js.Shutdown();
		Profiler::Instance().Stop();
		const std::string content = ReadJsonl(jsonlPath);
		FloodResult r{};
		r.jobs = static_cast<uint64_t>(frames) * perFrame;
		r.jobLines = CountOf(content, "\"record_type\":\"profile_job\"");
		r.ringDrops = drops;
		r.scopeLines = CountOf(content, "\"scope_name\":" + std::to_string(Dia::Core::StringCRC("job.run").Value()) + ",");
		r.scopeDrops = r.jobs > r.scopeLines ? r.jobs - r.scopeLines : 0;
		r.wallMs = wall;
		Profiler::ClearThreadNames();
		return r;
	}

	void PrintFlood(const char* label, unsigned workers, const FloodResult& r)
	{
		std::printf("[BENCH] %s workers=%u jobs=%llu wall=%.0fms  job lines=%llu ring drops=%llu  job.run scopes=%llu scope drops=%llu\n",
			label, workers, (unsigned long long)r.jobs, r.wallMs, (unsigned long long)r.jobLines,
			(unsigned long long)r.ringDrops, (unsigned long long)r.scopeLines, (unsigned long long)r.scopeDrops);
	}
}

TEST_F(SLOW_JobObserverTest, Benchmark_ProfileJobFlood_Uniform10kPerSec)
{
	for (unsigned workers : {2u, 3u, 4u})
	{
		Profiler::Instance().Stop();
		std::remove(mJsonlPath);
		ASSERT_TRUE(Profiler::Instance().Start(mJsonlPath, Dia::Observation::Profile::Category::kAll, "flood", 0));
		const FloodResult r = RunFlood(mJsonlPath, workers, 100, 310);   // Windows sleep_for(16ms) ~ 31 ms real, so 310/frame ~ 10,000 jobs/s
		PrintFlood("uniform 10k/s", workers, r);
	}
}

TEST_F(SLOW_JobObserverTest, Benchmark_ProfileJobFlood_BurstInOneFrame)
{
	for (unsigned workers : {2u, 4u})
	{
		for (unsigned burst : {1000u, 10000u})
		{
			Profiler::Instance().Stop();
			std::remove(mJsonlPath);
			ASSERT_TRUE(Profiler::Instance().Start(mJsonlPath, Dia::Observation::Profile::Category::kAll, "flood", 0));
			const FloodResult r = RunFlood(mJsonlPath, workers, 1, burst);
			PrintFlood(burst == 1000u ? "burst 1k/frame" : "burst 10k/frame", workers, r);
		}
	}
}

// ---------------------------------------------------------------------------------------------
// Converter integration: real engine-path profile.jsonl. Written to $DIA_ENGINE_PROFILE_FIXTURE
// (or ./engine-profile-fixture.jsonl); Dia/DiaCLI/tests/test_cli_perf_convert.py picks it up.
// ---------------------------------------------------------------------------------------------

TEST_F(SLOW_JobObserverTest, Integration_WritesRealEngineProfileForConverter)
{
	const char* env = std::getenv("DIA_ENGINE_PROFILE_FIXTURE");
	const std::string out = (env && *env) ? env : "engine-profile-fixture.jsonl";
	Profiler::Instance().Stop();
	std::remove(out.c_str());
	Profiler::ClearThreadNames();
	Profiler::ClearSubjectNames();
	ASSERT_TRUE(Profiler::RegisterSubjectName(kOwner, "ecology"));
	ASSERT_TRUE(Profiler::Instance().Start(out.c_str(), Dia::Observation::Profile::Category::kAll, "engine-fixture", 1700000000000000000LL));
	Profiler::RegisterThreadName(Profiler::CurrentThreadId(), "Main");

	JobObserver observer;
	JobSystem js;
	js.Initialize(3, nullptr, &observer);
	JobTag tag; tag.owner = kOwner; tag.tag = 4; tag.rangeCount = 8;
	for (int frame = 0; frame < 3; ++frame)
	{
		std::vector<Dia::Threading::JobHandle> hs;
		{
			DIA_PROFILE_SCOPE("fixture.frame", Dia::Observation::Profile::Category::kAll);
			for (unsigned i = 0; i < 8; ++i)
			{
				tag.range = static_cast<uint16_t>(i);
				hs.push_back(js.Submit([] { DIA_PROFILE_SCOPE("fixture.work", Dia::Observation::Profile::Category::kAll); std::this_thread::sleep_for(std::chrono::microseconds(300)); }, tag));
			}
			for (auto& h : hs) js.Wait(h);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		Profiler::Instance().BeginFrame();
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(100));
	js.Shutdown();
	Profiler::Instance().Stop();
	const std::string content = ReadJsonl(out.c_str());
	EXPECT_EQ(CountOf(content, "\"record_type\":\"profile_job\""), 24u);
	EXPECT_GE(CountOf(content, "\"record_type\":\"thread_name\""), 2u);
	EXPECT_NE(content.find("\"subject\":\"ecology\""), std::string::npos);
	Profiler::ClearThreadNames();
	Profiler::ClearSubjectNames();
}
