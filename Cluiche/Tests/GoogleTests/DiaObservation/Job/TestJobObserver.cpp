#include <gtest/gtest.h>
#include <DiaObservation/Job/JobObserver.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaObservation/Profile/Profiler.h>
#include <DiaObservation/Testing/ProfileFixture.h>
#include <DiaThreading/JobSystem.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
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
