////////////////////////////////////////////////////////////////////////////////
// Filename: TestDomainStateSnapshotPublisher.cpp
// Description: DomainStateSnapshotPublisher + DomainStateHealth: Sim-thread-only
//              GetJSONState (AC 1), interest window (AC 5), registry churn (AC 7),
//              health/metrics (ACs 10, 11).
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Console/DomainStateSnapshotPublisher.h>
#include <DiaVisualDebugger/Domain/DiaDebugDomainRegistry.h>
#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Metric/Counter.h>
#include <DiaObservation/Metric/Gauge.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

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

#endif // DIA_DEBUG
