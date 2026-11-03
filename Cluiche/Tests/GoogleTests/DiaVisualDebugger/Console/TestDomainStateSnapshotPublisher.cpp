////////////////////////////////////////////////////////////////////////////////
// Filename: TestDomainStateSnapshotPublisher.cpp
// Description: DomainStateSnapshotPublisher + DomainStateHealth: Sim-thread-only
//              GetJSONState (AC 1), interest window (AC 5), registry churn (AC 7),
//              health/metrics (ACs 10, 11).
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Console/DomainStateSnapshotPublisher.h>
#include <DiaVisualDebugger/Console/DomainStateQuery.h>
#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>
#include <DiaStreams/FrameStreamStore.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace Dia::VisualDebugger;
using namespace Dia::VisualDebugger::Console;
using Dia::Core::StringCRC;
using Dia::Observation::Health::HealthStatus;

namespace
{
	uint64_t gNowMs = 0;
	uint64_t FakeNow() { return gNowMs; }

	class ThreadDomain : public IDebugDomain
	{
	public:
		ThreadDomain(const char* id, const char* display, size_t payloadBytes = 0)
			: mId(id), mDisplay(display), mPayload(payloadBytes, 'x') {}

		StringCRC GetDomainId() const override { return mId; }
		const char* GetDisplayName() const override { return mDisplay; }
		const char* GetDescription() const override { return "publisher test domain"; }
		StringCRC GetGroup() const override { return StringCRC("Spatial"); }
		Dia::Core::RGBA GetAccentColour() const override { return Dia::Core::RGBA(1, 2, 3, 255); }
		bool HasWorldDrawers() const override { return false; }
		void OnCommand(StringCRC, const Json::Value&) override {}
		void GetJSONState(Json::Value& out) override
		{
			mThreads.push_back(std::this_thread::get_id());
			out["enabled"] = true;
			if (!mPayload.empty()) { out["blob"] = mPayload; }
		}

		std::vector<std::thread::id> mThreads;

	private:
		StringCRC mId;
		const char* mDisplay;
		std::string mPayload;
	};

	class CountingSink : public IDomainStateSink
	{
	public:
		void Write(const DomainStateSnapshot& s) override
		{
			++writes;
			lastFrame = s.simFrame;
			lastCount = s.domainCount;
			lastFlags = s.flags;
			lastJson.assign(s.json, s.jsonBytes);
		}
		uint32_t writes = 0;
		uint64_t lastFrame = 0;
		uint32_t lastCount = 0;
		uint32_t lastFlags = 0;
		std::string lastJson;
	};

	uint64_t CounterValue(const char* name)
	{
		return Dia::Observation::Metric::MetricRegistry::Instance().RegisterCounter(StringCRC(name))->Value();
	}

	struct PublisherTest : public ::testing::Test
	{
		void SetUp() override { gNowMs = 1000; }
		DiaDebugDomainRegistry registry;
		CountingSink sink;
	};
}

// AC 1: GetJSONState runs only on the thread driving Update (the Sim thread), once per Update.
TEST_F(PublisherTest, GetJSONStateRunsOnSimThreadOnly)
{
	ThreadDomain a("pub.a", "A"), b("pub.b", "B");
	registry.Register(a);
	registry.Register(b);
	DomainStateSnapshotPublisher pub(registry, sink, nullptr, &FakeNow);

	std::thread::id simId;
	std::thread sim([&]
	{
		simId = std::this_thread::get_id();
		pub.Arm(5000);
		pub.Update(1);
		pub.Update(2);
	});
	sim.join();

	const std::thread::id mainId = std::this_thread::get_id();
	ASSERT_NE(simId, mainId);
	ASSERT_EQ(a.mThreads.size(), 2u);	// at most once per Update
	ASSERT_EQ(b.mThreads.size(), 2u);
	for (auto id : a.mThreads) { EXPECT_EQ(id, simId); EXPECT_NE(id, mainId); }
	for (auto id : b.mThreads) { EXPECT_EQ(id, simId); EXPECT_NE(id, mainId); }
	EXPECT_EQ(sink.writes, 2u);
	EXPECT_EQ(sink.lastFrame, 2u);
	EXPECT_EQ(sink.lastCount, 2u);
}

// AC 5: nothing is produced without interest.
TEST_F(PublisherTest, NothingPublishedWithoutInterest)
{
	ThreadDomain a("pub.a", "A");
	registry.Register(a);
	DomainStateSnapshotPublisher pub(registry, sink, nullptr, &FakeNow);
	for (uint64_t f = 1; f <= 10; ++f) { pub.Update(f); }
	EXPECT_EQ(sink.writes, 0u);
	EXPECT_TRUE(a.mThreads.empty());	// not even GetJSONState
	EXPECT_FALSE(pub.IsArmed());
}

TEST_F(PublisherTest, PublishesWhileWindowOpenThenStops)
{
	ThreadDomain a("pub.a", "A");
	registry.Register(a);
	DomainStateSnapshotPublisher pub(registry, sink, nullptr, &FakeNow);
	pub.Arm(500);
	pub.Update(1);
	gNowMs += 400;
	pub.Update(2);
	EXPECT_EQ(sink.writes, 2u);
	gNowMs += 200;	// 600 ms after Arm: window closed
	pub.Update(3);
	EXPECT_EQ(sink.writes, 2u);
	EXPECT_FALSE(pub.IsArmed());
	pub.Update(4);
	EXPECT_EQ(sink.writes, 2u);
	EXPECT_EQ(a.mThreads.size(), 2u);
}

TEST_F(PublisherTest, ResumesWithinOneFrameOfNewInterest)
{
	ThreadDomain a("pub.a", "A");
	registry.Register(a);
	DomainStateSnapshotPublisher pub(registry, sink, nullptr, &FakeNow);
	pub.Arm(100);
	pub.Update(1);
	gNowMs += 1000;
	pub.Update(2);
	ASSERT_EQ(sink.writes, 1u);

	pub.Arm(100);	// new interest arrives, then the very next Update publishes
	pub.Update(3);
	EXPECT_EQ(sink.writes, 2u);
	EXPECT_EQ(sink.lastFrame, 3u);
}

TEST_F(PublisherTest, ReArmExtendsWindow)
{
	ThreadDomain a("pub.a", "A");
	registry.Register(a);
	DomainStateSnapshotPublisher pub(registry, sink, nullptr, &FakeNow);
	pub.Arm(500);
	gNowMs += 400;
	pub.Arm(500);
	gNowMs += 400;	// 800 after first Arm, 400 after second
	pub.Update(1);
	EXPECT_EQ(sink.writes, 1u);
}

// AC 7 (single-threaded determinism): churn between Updates shows in the next snapshot.
TEST_F(PublisherTest, RegistryChurnReflectedInNextSnapshot)
{
	ThreadDomain a("pub.a", "A"), b("pub.b", "B"), c("pub.c", "C");
	registry.Register(a);
	registry.Register(b);
	DomainStateSnapshotPublisher pub(registry, sink, nullptr, &FakeNow);
	pub.Arm(60000);

	pub.Update(1);
	EXPECT_EQ(sink.lastCount, 2u);
	EXPECT_NE(sink.lastJson.find("\"B\""), std::string::npos);

	registry.Unregister(b);
	registry.Register(c);
	pub.Update(2);
	EXPECT_EQ(sink.lastCount, 2u);
	EXPECT_EQ(sink.lastJson.find("\"B\""), std::string::npos);
	EXPECT_NE(sink.lastJson.find("\"C\""), std::string::npos);

	registry.Unregister(a);
	registry.Unregister(c);
	pub.Update(3);
	EXPECT_EQ(sink.lastCount, 0u);
	Json::Value v;
	Json::Reader reader;
	EXPECT_TRUE(reader.parse(sink.lastJson, v));
	EXPECT_TRUE(v.isObject());
	EXPECT_EQ(v.size(), 0u);
	EXPECT_EQ(b.mThreads.size(), 1u);	// never called after unregister
}

// AC 10/11: metrics, truncation counter, warning rate limit.
TEST_F(PublisherTest, MetricsPublishedAndBytes)
{
	ThreadDomain a("pub.a", "A");
	registry.Register(a);
	DomainStateSnapshotPublisher pub(registry, sink, nullptr, &FakeNow);
	const uint64_t before = CounterValue("dia.debugvisualiser.domain_snapshot.published");
	pub.Arm(5000);
	pub.Update(1);
	pub.Update(2);
	pub.Update(3);
	EXPECT_EQ(CounterValue("dia.debugvisualiser.domain_snapshot.published") - before, 3u);
	const double bytes = Dia::Observation::Metric::MetricRegistry::Instance()
		.RegisterGauge(StringCRC("dia.debugvisualiser.domain_snapshot.bytes"))->Value();
	EXPECT_EQ(static_cast<size_t>(bytes), sink.lastJson.size());
	EXPECT_GT(bytes, 0.0);
}

TEST_F(PublisherTest, TruncationCountsAndRateLimitsWarning)
{
	ThreadDomain big("pub.big", "Big", 4000);
	registry.Register(big);
	DomainStateSnapshotPublisher pub(registry, sink, nullptr, &FakeNow, 200);	// tiny budget: the domain cannot fit
	const uint64_t before = CounterValue("dia.debugvisualiser.domain_snapshot.truncated");
	pub.Arm(60000);

	pub.Update(1);	// t=1000
	gNowMs += 100; pub.Update(2);
	gNowMs += 100; pub.Update(3);
	EXPECT_EQ(CounterValue("dia.debugvisualiser.domain_snapshot.truncated") - before, 3u);
	EXPECT_EQ(pub.TruncatedCount(), 3u);
	EXPECT_EQ(pub.WarningCount(), 1u);	// once within a second
	EXPECT_NE(sink.lastFlags & kSnapshotFlagTruncated, 0u);
	EXPECT_EQ(sink.lastCount, 0u);

	gNowMs += 1000;
	pub.Update(4);
	EXPECT_EQ(pub.WarningCount(), 2u);
	EXPECT_EQ(sink.writes, 4u);	// truncated snapshots are still published
}

TEST_F(PublisherTest, UntruncatedDoesNotWarn)
{
	ThreadDomain a("pub.a", "A");
	registry.Register(a);
	DomainStateSnapshotPublisher pub(registry, sink, nullptr, &FakeNow);
	pub.Arm(5000);
	pub.Update(1);
	EXPECT_EQ(pub.TruncatedCount(), 0u);
	EXPECT_EQ(pub.WarningCount(), 0u);
	EXPECT_EQ(sink.lastFlags & kSnapshotFlagTruncated, 0u);
}

TEST(DomainStateHealthTest, OkWhenFresh)
{
	DomainStateHealth h;
	EXPECT_EQ(h.GetReporterName(), StringCRC("dia.debugvisualiser.domain_snapshot"));
	h.OnSnapshot(100);
	EXPECT_EQ(h.Report().status, HealthStatus::kOK);
	EXPECT_EQ(h.AgeMs(), 100u);
}

TEST(DomainStateHealthTest, DegradedWhenStaleOverFiveSeconds)
{
	DomainStateHealth h;
	h.OnSnapshot(5000);
	EXPECT_EQ(h.Report().status, HealthStatus::kOK);	// boundary: not over
	h.OnSnapshot(5001);
	const auto r = h.Report();
	EXPECT_EQ(r.status, HealthStatus::kDegraded);
	EXPECT_EQ(r.reason, StringCRC("domain_snapshot_stale"));
	h.OnSnapshot(10);
	EXPECT_EQ(h.Report().status, HealthStatus::kOK);
}

TEST(DomainStateHealthTest, DegradedAfterManyPendingReadsAndRecovers)
{
	DomainStateHealth h;
	const uint64_t before = CounterValue("dia.debugvisualiser.domain_snapshot.reads_pending");
	for (uint32_t i = 0; i < DomainStateHealth::kPendingLimit - 1; ++i) { h.OnPending(); }
	EXPECT_EQ(h.Report().status, HealthStatus::kOK);
	h.OnPending();
	EXPECT_EQ(h.Report().status, HealthStatus::kDegraded);
	EXPECT_EQ(h.Report().reason, StringCRC("domain_snapshot_never_published"));
	EXPECT_EQ(CounterValue("dia.debugvisualiser.domain_snapshot.reads_pending") - before, DomainStateHealth::kPendingLimit);
	h.OnSnapshot(0);
	EXPECT_EQ(h.Report().status, HealthStatus::kOK);
}

TEST(DomainStateHealthTest, AgeGaugeFollowsSnapshots)
{
	DomainStateHealth h;
	h.OnSnapshot(1234);
	EXPECT_EQ(Dia::Observation::Metric::MetricRegistry::Instance()
		.RegisterGauge(StringCRC("dia.debugvisualiser.domain_snapshot.age_ms"))->Value(), 1234.0);
}


// ---- Task 7: edge cases -----------------------------------------------------

// The window is inclusive: an Update exactly at the end still publishes, one ms later it does not.
TEST_F(PublisherTest, WindowBoundary_PublishesAtExactEnd_StopsOneMsLater)
{
	ThreadDomain a("pub.a", "A");
	registry.Register(a);
	DomainStateSnapshotPublisher pub(registry, sink, nullptr, &FakeNow);
	pub.Arm(500);
	gNowMs += 500;
	pub.Update(1);
	EXPECT_EQ(sink.writes, 1u);
	gNowMs += 1;
	pub.Update(2);
	EXPECT_EQ(sink.writes, 1u);
	EXPECT_FALSE(pub.IsArmed());
}

TEST_F(PublisherTest, ArmZeroPublishesOnlyWhileTimeHasNotAdvanced)
{
	ThreadDomain a("pub.a", "A");
	registry.Register(a);
	DomainStateSnapshotPublisher pub(registry, sink, nullptr, &FakeNow);
	pub.Arm(0);
	pub.Update(1);
	EXPECT_EQ(sink.writes, 1u);
	gNowMs += 1;
	pub.Update(2);
	EXPECT_EQ(sink.writes, 1u);
}

// A shorter re-arm never shortens an open window.
TEST_F(PublisherTest, ShorterReArmDoesNotShrinkWindow)
{
	ThreadDomain a("pub.a", "A");
	registry.Register(a);
	DomainStateSnapshotPublisher pub(registry, sink, nullptr, &FakeNow);
	pub.Arm(1000);
	pub.Arm(10);
	gNowMs += 900;
	pub.Update(1);
	EXPECT_EQ(sink.writes, 1u);
}

TEST_F(PublisherTest, EmptyRegistryWhileArmed_PublishesEmptyObject)
{
	DomainStateSnapshotPublisher pub(registry, sink, nullptr, &FakeNow);
	pub.Arm(1000);
	pub.Update(5);
	EXPECT_EQ(sink.writes, 1u);
	EXPECT_EQ(sink.lastCount, 0u);
	EXPECT_EQ(sink.lastJson, "{}");
	EXPECT_EQ(sink.lastFlags, 0u);
	EXPECT_EQ(pub.TruncatedCount(), 0u);
}

TEST_F(PublisherTest, ExtremeSimFramesAreCarriedThrough)
{
	ThreadDomain a("pub.a", "A");
	registry.Register(a);
	DomainStateSnapshotPublisher pub(registry, sink, nullptr, &FakeNow);
	pub.Arm(1000);
	pub.Update(0);
	EXPECT_EQ(sink.lastFrame, 0u);
	pub.Update(UINT64_MAX);
	EXPECT_EQ(sink.lastFrame, UINT64_MAX);
}

// Rate limit is a closed interval: exactly kTruncatedWarnIntervalMs later warns again, 1 ms earlier does not.
TEST_F(PublisherTest, WarningIntervalBoundary)
{
	ThreadDomain big("pub.big", "Big", 4000);
	registry.Register(big);
	DomainStateSnapshotPublisher pub(registry, sink, nullptr, &FakeNow, 200);
	pub.Arm(600000);
	pub.Update(1);
	EXPECT_EQ(pub.WarningCount(), 1u);
	gNowMs += DomainStateSnapshotPublisher::kTruncatedWarnIntervalMs - 1;
	pub.Update(2);
	EXPECT_EQ(pub.WarningCount(), 1u);
	gNowMs += 1;
	pub.Update(3);
	EXPECT_EQ(pub.WarningCount(), 2u);
}

// The scratch snapshot is reused: a smaller snapshot after a larger one carries no stale tail.
TEST_F(PublisherTest, SmallerSnapshotAfterLarger_NoStaleBytes)
{
	ThreadDomain big("pub.big", "Big", 3000), small("pub.small", "Small");
	registry.Register(big);
	registry.Register(small);
	DomainStateSnapshotPublisher pub(registry, sink, nullptr, &FakeNow);
	pub.Arm(60000);
	pub.Update(1);
	const size_t large = sink.lastJson.size();
	registry.Unregister(big);
	pub.Update(2);
	EXPECT_LT(sink.lastJson.size(), large);
	Json::Value v;
	Json::Reader r;
	EXPECT_TRUE(r.parse(sink.lastJson, v)) << sink.lastJson;
	EXPECT_EQ(sink.lastCount, 1u);
}

TEST(DomainStateHealthTest, StaleBoundaryAndPendingTakesPrecedence)
{
	DomainStateHealth h;
	h.OnSnapshot(DomainStateHealth::kStaleMs);
	EXPECT_EQ(h.Report().status, HealthStatus::kOK);
	h.OnSnapshot(DomainStateHealth::kStaleMs + 1);
	EXPECT_EQ(h.Report().reason, StringCRC("domain_snapshot_stale"));
	for (uint32_t i = 0; i < DomainStateHealth::kPendingLimit; ++i) { h.OnPending(); }
	EXPECT_EQ(h.Report().reason, StringCRC("domain_snapshot_never_published"));
}

TEST(DomainStateHealthTest, PendingBelowLimitResetsOnSnapshot)
{
	DomainStateHealth h;
	for (uint32_t i = 0; i < DomainStateHealth::kPendingLimit - 1; ++i) { h.OnPending(); }
	h.OnSnapshot(0);
	h.OnPending();	// counter restarted: one pending is far below the limit
	EXPECT_EQ(h.Report().status, HealthStatus::kOK);
}


// AC 12 / 7: Sim thread churns domains and their state while Main reads snapshots through a real
// FrameStream. Every snapshot Main sees must parse and be internally consistent. Run under
// `dia check sanitizer` (ASan) for memory errors; the only shared object is the FrameStreamStore.
namespace
{
	class SinkToStream : public IDomainStateSink
	{
	public:
		explicit SinkToStream(Dia::ApplicationFlow::FrameStreamStore<DomainStateSnapshot>& store) : mStore(store) {}
		void Write(const DomainStateSnapshot& s) override
		{
			mStore.Write(s, Dia::Core::TimeAbsolute::CreateFromMilliseconds(static_cast<int>(s.simFrame)));
		}
	private:
		Dia::ApplicationFlow::FrameStreamStore<DomainStateSnapshot>& mStore;
	};

	// Its state echoes a counter that changes every frame, so a torn/mixed read would show as a
	// mismatch between "a" and "b" inside one domain.
	class ChurnDomain : public IDebugDomain
	{
	public:
		ChurnDomain(const char* id, const char* display) : mId(id), mDisplay(display) {}
		StringCRC GetDomainId() const override { return mId; }
		const char* GetDisplayName() const override { return mDisplay.c_str(); }
		const char* GetDescription() const override { return "churn"; }
		StringCRC GetGroup() const override { return StringCRC("Spatial"); }
		Dia::Core::RGBA GetAccentColour() const override { return Dia::Core::RGBA(1, 2, 3, 255); }
		bool HasWorldDrawers() const override { return false; }
		void OnCommand(StringCRC, const Json::Value&) override {}
		void GetJSONState(Json::Value& out) override
		{
			++mCounter;
			out["a"] = static_cast<Json::UInt64>(mCounter);
			out["b"] = static_cast<Json::UInt64>(mCounter);
			out["blob"] = std::string(512 + (mCounter % 7) * 100, 'y');
		}
		uint64_t mCounter = 0;
	private:
		StringCRC mId;
		std::string mDisplay;
	};
}

TEST(DomainStateStress, SimChurnsWhileMainReadsSnapshots)
{
	auto store = std::make_unique<Dia::ApplicationFlow::FrameStreamStore<DomainStateSnapshot>>(StringCRC("stress_domain_state"));
	DiaDebugDomainRegistry registry;
	SinkToStream sink(*store);
	gNowMs = 1000;
	DomainStateSnapshotPublisher pub(registry, sink, nullptr, &FakeNow);

	constexpr uint32_t kDomains = 6;
	constexpr uint64_t kFrames = 3000;
	std::vector<std::unique_ptr<ChurnDomain>> domains;
	std::vector<std::string> names;
	for (uint32_t i = 0; i < kDomains; ++i)
	{
		names.push_back("stress.domain." + std::to_string(i));
		domains.push_back(std::make_unique<ChurnDomain>(names.back().c_str(), names.back().c_str()));
	}

	std::atomic<bool> done{ false };
	std::thread sim([&]
	{
		pub.Arm(1000000);
		for (uint64_t frame = 1; frame <= kFrames; ++frame)
		{
			// Registry is Sim-owned: churn it between Updates.
			ChurnDomain& d = *domains[frame % kDomains];
			if ((frame / kDomains) % 2 == 0) { registry.Register(d); } else { registry.Unregister(d); }
			pub.Update(frame);
		}
		done.store(true, std::memory_order_release);
	});

	uint64_t reads = 0, lastFrame = 0, unreadable = 0;
	auto readOnce = [&]
	{
		const DomainStateSnapshot* snap = store->FetchLatest();
		if (snap == nullptr) { return; }
		DomainStateSnapshot copy = *snap;	// private copy so a concurrent overwrite is not parsed in place
		Json::Value root;
		if (!ParseDomainStateSnapshot(copy, root)) { ++unreadable; return; }
		++reads;
		EXPECT_GE(copy.simFrame, lastFrame) << "simFrame must never go backwards";
		lastFrame = copy.simFrame;
		for (const std::string& n : root.getMemberNames())
		{
			if (n == "simFrame") { continue; }
			const Json::Value& dom = root[n];
			if (!dom.isObject() || !dom.isMember("a")) { continue; }
			EXPECT_EQ(dom["a"].asUInt64(), dom["b"].asUInt64()) << "torn domain state in " << n;
		}
	};
	while (!done.load(std::memory_order_acquire)) { readOnce(); }
	sim.join();
	readOnce();

	EXPECT_GT(reads, 0u);
	EXPECT_EQ(lastFrame, kFrames);
	EXPECT_EQ(pub.PublishCount(), kFrames);
	// A copy taken while the writer laps the whole ring would be unreadable; with 8 slots this is
	// expected to be at most a rare event, never the norm.
	EXPECT_LT(unreadable, reads / 10 + 1);
}

#endif // DIA_DEBUG
