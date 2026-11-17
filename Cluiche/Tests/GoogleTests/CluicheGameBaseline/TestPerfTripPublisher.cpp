////////////////////////////////////////////////////////////////////////////////
// Filename: TestPerfTripPublisher.cpp
// GoogleTest suite - Perf Inspector task 3b: Main-side "perf.trip" ordered topic fed from the
// "$perftrips" stream, and the perf.timing "unavailable" / trips changes.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <Types/PerfTimingPublisher.h>
#include <Types/PerfTripPublisher.h>

#include <DiaObservation/Profile/CoarseChunk.h>
#include <DiaObservation/Profile/CoarseChunkAssembler.h>
#include <DiaObservation/Profile/TripChunk.h>
#include <DiaStreams/EventStreamStore.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

using namespace Dia::Observation::Profile;
using Dia::ApplicationFlow::EventStreamStore;
using Dia::ApplicationFlow::Event;
using Dia::Core::StringCRC;
using Cluiche::AppFlow::IPerfTimingSink;
using Cluiche::AppFlow::IPerfTripSink;
using Cluiche::AppFlow::PerfTimingPublisher;
using Cluiche::AppFlow::PerfTripPublisher;

namespace
{
	struct FakeTripSink : IPerfTripSink
	{
		uint32_t subscribers = 0;
		std::vector<Json::Value> published;
		uint32_t SubscriberCount() const override { return subscribers; }
		void PublishOrdered(const Json::Value& p) override { published.push_back(p); }
	};

	struct FakeTimingSink : IPerfTimingSink
	{
		uint32_t subscribers = 1;
		std::vector<Json::Value> published;
		uint32_t SubscriberCount() const override { return subscribers; }
		void Publish(const Json::Value& p) override { published.push_back(p); }
		bool TryGetSelfMs(double& out) const override { out = 0.1; return true; }
	};

	void Send(EventStreamStore<TripChunk>& store, uint32_t seq, uint32_t first, uint32_t last)
	{
		auto ev = std::make_unique<Event<TripChunk>>();
		std::memset(&ev->payload, 0, sizeof(TripChunk));
		TripChunk& c = ev->payload;
		c.unitCrc = 0x77u;
		c.seq = seq;
		std::strcpy(c.record.unit, "SimPU");
		std::strcpy(c.record.buildConfig, "Debug");
		c.record.firstFrame = first;
		c.record.lastFrame = last;
		c.record.budgetMs = 16.6f;
		c.record.peakMs = 40.0f;
		c.record.suppressedSinceLastTrip = 2;
		c.record.topCount = 1;
		std::strcpy(c.record.top[0].name, "ModA");
		c.record.top[0].ms = 30.0f;
		c.record.recentCount = 2;
		c.record.recent[0] = 5.0f;
		c.record.recent[1] = 40.0f;
		c.record.worstIndex = 1;
		c.record.worstUnitMs = 40.0f;
		store.Send(*ev);
	}

	std::vector<std::string> Unavailable(const Json::Value& p)
	{
		std::vector<std::string> out;
		for (const auto& v : p["unavailable"]) out.push_back(v.asString());
		return out;
	}

	bool Has(const std::vector<std::string>& v, const char* s)
	{
		for (const auto& x : v) if (x == s) return true;
		return false;
	}

	struct PerfTripRig : ::testing::Test
	{
		EventStreamStore<TripChunk> store{ StringCRC("test_perftrips"), StringCRC("TripChunk"), 8, 2 };
		FakeTripSink sink;
		PerfTripPublisher pub{ &store };
		void SetUp() override { pub.SetSink(&sink); }
	};
}

TEST_F(PerfTripRig, NoPublishWithoutSubscriber)
{
	ASSERT_TRUE(pub.HasReader());
	Send(store, 1, 10, 20);
	pub.Drain();
	EXPECT_TRUE(sink.published.empty());
	EXPECT_EQ(0u, pub.GetPublishedCount());
}

TEST_F(PerfTripRig, PublishesWhenSubscribedOnePerTrip)
{
	sink.subscribers = 1;
	Send(store, 1, 10, 20);
	Send(store, 2, 30, 31);
	pub.Drain();
	ASSERT_EQ(2u, sink.published.size());
	EXPECT_EQ(10u, sink.published[0]["first_frame"].asUInt());
	EXPECT_EQ(30u, sink.published[1]["first_frame"].asUInt());   // order preserved
	EXPECT_EQ(2u, pub.GetPublishedCount());
	pub.Drain();
	EXPECT_EQ(2u, sink.published.size());   // consumed, not republished
}

TEST_F(PerfTripRig, JsonKeysMatchTripRecordSerialisation)
{
	sink.subscribers = 1;
	Send(store, 1, 10, 20);
	pub.Drain();
	ASSERT_EQ(1u, sink.published.size());
	const Json::Value& j = sink.published[0];
	EXPECT_EQ("SimPU", j["unit"].asString());
	EXPECT_EQ("Debug", j["build_config"].asString());
	EXPECT_EQ(20u, j["last_frame"].asUInt());
	EXPECT_NEAR(16.6, j["budget_ms"].asDouble(), 1e-3);
	EXPECT_NEAR(40.0, j["peak_ms"].asDouble(), 1e-3);
	EXPECT_EQ(2u, j["suppressed_since_last_trip"].asUInt());
	ASSERT_EQ(1u, j["top_modules"].size());
	EXPECT_EQ("ModA", j["top_modules"][0]["module"].asString());
	EXPECT_EQ(1u, j["worst_frame"]["index"].asUInt());
	EXPECT_EQ(2u, j["recent_unit_ms"].size());
}

TEST_F(PerfTripRig, NullSinkStillDrains)
{
	pub.SetSink(nullptr);
	Send(store, 1, 1, 2);
	pub.Drain();
	pub.SetSink(&sink);
	sink.subscribers = 1;
	pub.Drain();
	EXPECT_TRUE(sink.published.empty());
}

TEST(PerfTripPublisher, NullStoreIsNoOp)
{
	PerfTripPublisher pub(nullptr);
	FakeTripSink sink;
	sink.subscribers = 1;
	pub.SetSink(&sink);
	EXPECT_FALSE(pub.HasReader());
	pub.Drain();
	EXPECT_TRUE(sink.published.empty());
}

TEST(PerfTimingTrips, TripsUnavailableUntilAttached)
{
	CoarseChunkAssembler assembler;
	FakeTimingSink sink;
	PerfTimingPublisher pub(assembler, nullptr, sink);
	pub.Tick(0.0);
	ASSERT_EQ(1u, sink.published.size());
	EXPECT_TRUE(Has(Unavailable(sink.published[0]), "trips"));
}

TEST(PerfTimingTrips, AttachedRemovesTripsAndPublishesCumulativeCount)
{
	CoarseChunkAssembler assembler;
	CoarseChunk c;
	std::memset(&c, 0, sizeof(c));
	c.unitCrc = 0x11u;
	c.unitHz = 60.0f;
	c.count = 1;
	c.tripCount = 4;
	assembler.Apply(c);

	FakeTimingSink sink;
	PerfTimingPublisher pub(assembler, nullptr, sink);
	pub.SetTripsAttached(true);
	pub.Tick(0.0);
	ASSERT_EQ(1u, sink.published.size());
	const Json::Value& p = sink.published[0];
	const auto un = Unavailable(p);
	EXPECT_FALSE(Has(un, "trips"));
	EXPECT_TRUE(Has(un, "marker_drops"));
	EXPECT_TRUE(Has(un, "suppressed_trips"));
	EXPECT_TRUE(Has(un, "suppressed_logs"));
	ASSERT_EQ(1u, p["units"].size());
	EXPECT_EQ(4u, p["units"][0]["trips"].asUInt());
}
