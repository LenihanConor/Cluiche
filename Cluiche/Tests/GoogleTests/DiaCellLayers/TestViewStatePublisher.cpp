#include <gtest/gtest.h>
#include <DiaCellLayers/ViewStatePublisher.h>
#include <DiaCellLayers/IViewHistorySource.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/ViewStateTestSupport.h>
#include <DiaCellLayers/Testing/LogCapture.h>

#include <string>

#ifdef DIA_DEBUG

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;

namespace
{
	struct ViewStateTest : ::testing::Test
	{
		TestGrid grid{ 4, 3 };
		Dia::Graphics::CellFieldRegistry cellField;
		StubProvider temp, rain;
		FakeDebugDraw draw;
		FakeClock clock;
		TopicRecorder topic;
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };
		LayerUploader uploader{ cellField, nullptr };
		CellLayerViewerDomain domain{ registry, uploader, nullptr };
		ViewStatePublisher publisher{ registry, domain, topic, clock };

		void SetUp() override
		{
			FakePuId() = 1;
			temp.values = std::vector<float>(12, 1.0f);
			rain.values = std::vector<float>(12, 2.0f);
			ASSERT_EQ(registry.Register(temp.MakeDesc("temp", &grid.desc)), CellRegisterResult::Ok);
			ASSERT_EQ(registry.Register(rain.MakeDesc("rain", &grid.desc)), CellRegisterResult::Ok);
			ASSERT_EQ(registry.RegisterOverlay(MakeOverlay("wind")), CellRegisterResult::Ok);
			ASSERT_EQ(registry.RegisterOverlay(MakeOverlay("rivers")), CellRegisterResult::Ok);
			domain.Update(draw);	// picks the initial base
		}

		void Frame() { domain.Update(draw); publisher.Update(); }
		void SelectCell(uint32_t q, uint32_t r)
		{
			CellSelection* sel = domain.ActiveSelection();
			ASSERT_NE(sel, nullptr);
			sel->BeginEdit(); sel->Set(r * grid.desc.width + q, true); sel->EndEdit();
		}
	};
}

TEST_F(ViewStateTest, GridIdUsesDescIdWhenSet)
{
	grid.desc.id = StringCRC("world.hex");
	Frame();
	ASSERT_EQ(topic.Count(), 1u);
	EXPECT_EQ(topic.Last()["grid"]["id"].asString(), "world.hex");
}

TEST_F(ViewStateTest, PayloadCarriesEveryField)
{
	ASSERT_TRUE(domain.ToggleOverlay(StringCRC("wind")));
	SelectCell(1, 1);
	Frame();
	ASSERT_EQ(topic.Count(), 1u);
	EXPECT_EQ(topic.entries[0].topic, StringCRC("cells.view.state"));
	const Json::Value& p = topic.Last();
	EXPECT_EQ(p["viewVersion"].asUInt(), 1u);
	EXPECT_EQ(p["layersVersion"].asUInt(), registry.LayersVersion());
	EXPECT_EQ(p["grid"]["id"].asString(), "grid.0");
	EXPECT_EQ(p["grid"]["topology"].asString(), "hex.flat.oddq");
	EXPECT_EQ(p["grid"]["width"].asUInt(), 4u);
	EXPECT_EQ(p["grid"]["height"].asUInt(), 3u);
	EXPECT_EQ(p["base"].asString(), "temp");
	ASSERT_TRUE(p["overlays"].isArray());
	ASSERT_EQ(p["overlays"].size(), 1u);
	EXPECT_EQ(p["overlays"][0].asString(), "wind");
	const SelectionFingerprint fp = SelectionFingerprintFn::Compute(*domain.ActiveSelectionView());
	EXPECT_EQ(p["selection"]["count"].asUInt(), 1u);
	EXPECT_EQ(p["selection"]["version"].asUInt(), fp.mVersion);
	char hex[SelectionFingerprintFn::kHexLength + 1];
	SelectionFingerprintFn::ToHex(fp.mHash, hex);
	EXPECT_EQ(p["selection"]["hash"].asString(), std::string(hex));
	EXPECT_EQ(p["selection"]["hash"].asString().size(), 16u);
	ASSERT_TRUE(p.isMember("history"));
	EXPECT_TRUE(p["history"].isNull());
}

namespace
{
	struct FakeHistorySource : IViewHistorySource
	{
		uint64_t stamp = 1;
		std::string mode = "live";
		Json::Value HistoryField() const override
		{
			Json::Value v(Json::objectValue);
			v["mode"] = mode;
			return v;
		}
		uint64_t HistoryStamp() const override { return stamp; }
	};
}

TEST_F(ViewStateTest, HistorySourceFieldIsPublished)
{
	FakeHistorySource src;
	src.mode = "sample";
	publisher.SetHistorySource(&src);
	Frame();
	ASSERT_EQ(topic.Count(), 1u);
	ASSERT_TRUE(topic.Last()["history"].isObject());
	EXPECT_EQ(topic.Last()["history"]["mode"].asString(), "sample");
}

TEST_F(ViewStateTest, HistoryStampChangeRepublishesAndUnchangedDoesNot)
{
	FakeHistorySource src;
	publisher.SetHistorySource(&src);
	Frame();
	ASSERT_EQ(topic.Count(), 1u);
	clock.Advance(200); Frame();
	EXPECT_EQ(topic.Count(), 1u);
	src.stamp = 2; src.mode = "sample";
	clock.Advance(200); Frame();
	ASSERT_EQ(topic.Count(), 2u);
	EXPECT_EQ(topic.Last()["history"]["mode"].asString(), "sample");
}

TEST_F(ViewStateTest, ClearingHistorySourceReturnsToNullAndRepublishes)
{
	FakeHistorySource src;
	publisher.SetHistorySource(&src);
	Frame();
	publisher.SetHistorySource(nullptr);
	clock.Advance(200); Frame();
	ASSERT_EQ(topic.Count(), 2u);
	EXPECT_TRUE(topic.Last()["history"].isNull());
}

TEST_F(ViewStateTest, NoPublishWhenNothingChanged)
{
	Frame();
	ASSERT_EQ(topic.Count(), 1u);
	for (int i = 0; i < 5; ++i) { clock.Advance(200); Frame(); }
	EXPECT_EQ(topic.Count(), 1u);
}

TEST_F(ViewStateTest, PublishesOnBaseChange)
{
	Frame();
	clock.Advance(200);
	ASSERT_TRUE(domain.SetBase(StringCRC("rain")));
	Frame();
	ASSERT_EQ(topic.Count(), 2u);
	EXPECT_EQ(topic.Last()["base"].asString(), "rain");
}

TEST_F(ViewStateTest, PublishesOnOverlayChange)
{
	Frame();
	clock.Advance(200);
	ASSERT_TRUE(domain.ToggleOverlay(StringCRC("rivers")));
	Frame();
	ASSERT_EQ(topic.Count(), 2u);
	EXPECT_EQ(topic.Last()["overlays"][0].asString(), "rivers");
}

TEST_F(ViewStateTest, PublishesOnSelectionChange)
{
	Frame();
	clock.Advance(200);
	SelectCell(0, 0);
	Frame();
	ASSERT_EQ(topic.Count(), 2u);
	EXPECT_EQ(topic.Last()["selection"]["count"].asUInt(), 1u);
}

TEST_F(ViewStateTest, BurstIsCoalescedToTenPerSecondWithTrailingPublish)
{
	Frame();	// t=0 immediate
	ASSERT_EQ(topic.Count(), 1u);
	// 1 second of changes, one per 10 ms (100 changes), alternating the base
	for (int i = 1; i <= 100; ++i)
	{
		clock.Advance(10);
		ASSERT_TRUE(domain.SetBase(StringCRC((i % 2) ? "rain" : "temp")));
		Frame();
	}
	EXPECT_LE(topic.Count(), 11u);	// first + at most 10 in the following second
	// changes stop on "rain": the held state is delivered once the interval ends
	ASSERT_TRUE(domain.SetBase(StringCRC("rain")));
	clock.Advance(10); Frame();
	clock.Advance(150); Frame();
	EXPECT_EQ(topic.Last()["base"].asString(), "rain");
	const size_t settled = topic.Count();
	clock.Advance(500); Frame();
	EXPECT_EQ(topic.Count(), settled);	// nothing further once the final state is out
	for (size_t i = 1; i < topic.Count(); ++i)
	{
		EXPECT_GT(topic.entries[i].payload["viewVersion"].asUInt(), topic.entries[i - 1].payload["viewVersion"].asUInt());
	}
}

TEST_F(ViewStateTest, ChangeInsideIntervalIsHeldThenSent)
{
	Frame();
	clock.Advance(30);
	ASSERT_TRUE(domain.SetBase(StringCRC("rain")));
	Frame();
	EXPECT_EQ(topic.Count(), 1u);	// held
	clock.Advance(69); Frame();
	EXPECT_EQ(topic.Count(), 1u);	// still inside the 100 ms interval
	clock.Advance(1); Frame();
	ASSERT_EQ(topic.Count(), 2u);
	EXPECT_EQ(topic.Last()["base"].asString(), "rain");
}

TEST_F(ViewStateTest, FirstChangeAfterIdleIsImmediate)
{
	Frame();
	clock.Advance(5000);
	ASSERT_TRUE(domain.SetBase(StringCRC("rain")));
	Frame();
	EXPECT_EQ(topic.Count(), 2u);
}

TEST_F(ViewStateTest, LayersVersionBumpsOnlyOnRegisterAndUnregister)
{
	const uint32_t v0 = registry.LayersVersion();
	StubProvider extra;
	ASSERT_EQ(registry.Register(extra.MakeDesc("extra", &grid.desc)), CellRegisterResult::Ok);
	const uint32_t v1 = registry.LayersVersion();
	EXPECT_GT(v1, v0);
	ASSERT_EQ(registry.RegisterOverlay(MakeOverlay("extra.ov")), CellRegisterResult::Ok);
	const uint32_t v2 = registry.LayersVersion();
	EXPECT_GT(v2, v1);

	// base / overlay / selection changes and rejected registrations do not bump it
	domain.SetBase(StringCRC("rain"));
	domain.ToggleOverlay(StringCRC("wind"));
	SelectCell(2, 2);
	EXPECT_EQ(registry.Register(extra.MakeDesc("extra", &grid.desc)), CellRegisterResult::Duplicate);
	EXPECT_FALSE(registry.Unregister(StringCRC("nope")));
	EXPECT_EQ(registry.LayersVersion(), v2);

	ASSERT_TRUE(registry.UnregisterOverlay(StringCRC("extra.ov")));
	const uint32_t v3 = registry.LayersVersion();
	EXPECT_GT(v3, v2);
	ASSERT_TRUE(registry.Unregister(StringCRC("extra")));
	EXPECT_GT(registry.LayersVersion(), v3);
}

TEST_F(ViewStateTest, LayersVersionInPayloadChangesOnlyOnRegistration)
{
	Frame();
	const uint32_t lv = topic.Last()["layersVersion"].asUInt();
	clock.Advance(200);
	domain.SetBase(StringCRC("rain"));
	Frame();
	ASSERT_EQ(topic.Count(), 2u);
	EXPECT_EQ(topic.Last()["layersVersion"].asUInt(), lv);
	clock.Advance(200);
	StubProvider extra;
	ASSERT_EQ(registry.Register(extra.MakeDesc("extra", &grid.desc)), CellRegisterResult::Ok);
	Frame();
	ASSERT_EQ(topic.Count(), 3u);
	EXPECT_GT(topic.Last()["layersVersion"].asUInt(), lv);
}

TEST_F(ViewStateTest, ViewVersionIsMonotonicAndCountsPublishes)
{
	Frame();
	for (int i = 0; i < 4; ++i)
	{
		clock.Advance(200);
		domain.SetBase(StringCRC((i % 2) ? "temp" : "rain"));
		Frame();
	}
	ASSERT_EQ(topic.Count(), 5u);
	for (size_t i = 0; i < topic.Count(); ++i) { EXPECT_EQ(topic.entries[i].payload["viewVersion"].asUInt(), i + 1); }
	EXPECT_EQ(publisher.ViewVersion(), 5u);
}

TEST_F(ViewStateTest, EmptyViewerPublishesEmptyState)
{
	CellLayerRegistry emptyReg(nullptr, &FakePuIdFn);
	LayerUploader up(cellField, nullptr);
	CellLayerViewerDomain emptyDomain(emptyReg, up, nullptr);
	TopicRecorder rec;
	ViewStatePublisher pub(emptyReg, emptyDomain, rec, clock);
	emptyDomain.Update(draw);
	pub.Update();
	ASSERT_EQ(rec.Count(), 1u);
	EXPECT_TRUE(rec.Last()["grid"].isNull());
	EXPECT_EQ(rec.Last()["base"].asString(), "");
	EXPECT_EQ(rec.Last()["selection"]["count"].asUInt(), 0u);
	EXPECT_EQ(rec.Last()["selection"]["hash"].asString(), "0000000000000000");
}

TEST_F(ViewStateTest, SubscriberTransitionsLogInfoOncePerTransition)
{
	LogCapture log("CEL");
	Frame();
	EXPECT_EQ(0u, log.Count(LogCapture::Level::kInfo, "first subscriber"));
	EXPECT_EQ(0u, log.Count(LogCapture::Level::kInfo, "last subscriber left"));

	topic.subscribers = 1;
	Frame();
	Frame();
	topic.subscribers = 3;	// still > 0: no transition
	Frame();
	EXPECT_EQ(1u, log.Count(LogCapture::Level::kInfo, "cells.view.state: first subscriber"));
	EXPECT_EQ(0u, log.Count(LogCapture::Level::kInfo, "last subscriber left"));

	topic.subscribers = 0;
	Frame();
	Frame();
	EXPECT_EQ(1u, log.Count(LogCapture::Level::kInfo, "cells.view.state: first subscriber"));
	EXPECT_EQ(1u, log.Count(LogCapture::Level::kInfo, "cells.view.state: last subscriber left"));

	topic.subscribers = 2;	// a second cycle logs again
	Frame();
	EXPECT_EQ(2u, log.Count(LogCapture::Level::kInfo, "first subscriber"));
}

TEST_F(ViewStateTest, SubscriberTransitionLogsEvenWhenNothingToPublish)
{
	Frame();	// publishes the initial state
	LogCapture log("CEL");
	topic.subscribers = 1;
	Frame();	// state unchanged -> early-out path must still observe the count
	EXPECT_EQ(1u, log.Count(LogCapture::Level::kInfo, "first subscriber"));
}

// ---- Task 10a exhaustive pass ----

TEST_F(ViewStateTest, RevertingInsideTheIntervalPublishesNothingFurther)
{
	Frame();
	ASSERT_EQ(topic.Count(), 1u);
	clock.Advance(20);
	ASSERT_TRUE(domain.SetBase(StringCRC("rain")));
	Frame();	// held
	EXPECT_EQ(publisher.CoalescedCount(), 1u);
	clock.Advance(20);
	ASSERT_TRUE(domain.SetBase(StringCRC("temp")));	// back to what subscribers already have
	Frame();
	clock.Advance(500);
	Frame();
	EXPECT_EQ(topic.Count(), 1u) << "state equals the last published state: nothing to send";
	EXPECT_EQ(publisher.CoalescedCount(), 1u) << "an unchanged frame is not a coalesced one";
}

TEST_F(ViewStateTest, CoalescedCountCountsEveryHeldUpdate)
{
	Frame();
	clock.Advance(10);
	ASSERT_TRUE(domain.SetBase(StringCRC("rain")));
	for (int i = 0; i < 4; ++i) { Frame(); clock.Advance(10); }	// 4 held updates at t = 10..40
	EXPECT_EQ(publisher.CoalescedCount(), 4u);
	EXPECT_EQ(topic.Count(), 1u);
	clock.Advance(100);
	Frame();
	EXPECT_EQ(topic.Count(), 2u);
	EXPECT_EQ(publisher.CoalescedCount(), 4u);
}

TEST_F(ViewStateTest, IntervalBoundaryNinetyNineHoldsHundredPublishes)
{
	Frame();	// t = 0
	clock.Advance(99);
	ASSERT_TRUE(domain.SetBase(StringCRC("rain")));
	Frame();
	EXPECT_EQ(topic.Count(), 1u);
	clock.Advance(1);	// t = 100: exactly one interval since the last publish
	Frame();
	EXPECT_EQ(topic.Count(), 2u);
}

TEST_F(ViewStateTest, TheIntervalRestartsFromTheLastPublishNotTheLastChange)
{
	Frame();	// t = 0
	clock.Advance(100);
	ASSERT_TRUE(domain.SetBase(StringCRC("rain")));
	Frame();	// t = 100: published
	ASSERT_EQ(topic.Count(), 2u);
	clock.Advance(60);
	ASSERT_TRUE(domain.SetBase(StringCRC("temp")));
	Frame();	// t = 160: held
	clock.Advance(39);
	Frame();	// t = 199: still inside the 100 ms that started at t = 100
	EXPECT_EQ(topic.Count(), 2u);
	clock.Advance(1);
	Frame();	// t = 200
	EXPECT_EQ(topic.Count(), 3u);
	EXPECT_EQ(topic.Last()["base"].asString(), "temp");
}

TEST_F(ViewStateTest, StillPublishesWithNoSubscribersSoTheServerCanReplayTheLatest)
{
	topic.subscribers = 0;
	Frame();
	clock.Advance(200);
	domain.SetBase(StringCRC("rain"));
	Frame();
	EXPECT_EQ(topic.Count(), 2u);
}

TEST_F(ViewStateTest, ClockGoingBackwardsDoesNotWedgeThePublisher)
{
	clock.nowMs = 10000;
	Frame();
	ASSERT_EQ(topic.Count(), 1u);
	clock.nowMs = 50;	// a clock reset
	ASSERT_TRUE(domain.SetBase(StringCRC("rain")));
	Frame();
	EXPECT_EQ(topic.Count(), 2u);
	EXPECT_EQ(topic.Last()["base"].asString(), "rain");
}

TEST_F(ViewStateTest, SameCountDifferentCellsChangesTheHashAndClearingZeroesIt)
{
	Frame();
	clock.Advance(200);
	SelectCell(0, 0);
	Frame();
	const std::string h1 = topic.Last()["selection"]["hash"].asString();
	clock.Advance(200);
	{
		CellSelection* sel = domain.ActiveSelection();
		ASSERT_NE(sel, nullptr);
		sel->BeginEdit(); sel->Set(0, false); sel->Set(1, true); sel->EndEdit();
	}
	Frame();
	ASSERT_EQ(topic.Count(), 3u);
	EXPECT_EQ(topic.Last()["selection"]["count"].asUInt(), 1u);
	EXPECT_NE(topic.Last()["selection"]["hash"].asString(), h1);
	clock.Advance(200);
	{
		CellSelection* sel = domain.ActiveSelection();
		sel->BeginEdit(); sel->Set(1, false); sel->EndEdit();
	}
	Frame();
	ASSERT_EQ(topic.Count(), 4u);
	EXPECT_EQ(topic.Last()["selection"]["count"].asUInt(), 0u);
	EXPECT_EQ(topic.Last()["selection"]["hash"].asString(), "0000000000000000");
}

TEST_F(ViewStateTest, OverlaysListInRegistryOrderAndDropWhenDisabled)
{
	Frame();
	clock.Advance(200);
	ASSERT_TRUE(domain.ToggleOverlay(StringCRC("rivers")));
	ASSERT_TRUE(domain.ToggleOverlay(StringCRC("wind")));	// enabled second, registered first
	Frame();
	ASSERT_EQ(topic.Last()["overlays"].size(), 2u);
	EXPECT_EQ(topic.Last()["overlays"][0u].asString(), "wind");
	EXPECT_EQ(topic.Last()["overlays"][1u].asString(), "rivers");
	clock.Advance(200);
	ASSERT_TRUE(domain.ToggleOverlay(StringCRC("wind")));
	Frame();
	ASSERT_EQ(topic.Last()["overlays"].size(), 1u);
	EXPECT_EQ(topic.Last()["overlays"][0u].asString(), "rivers");
}

TEST_F(ViewStateTest, UnregisteringTheActiveBaseRepublishesWithANewLayersVersion)
{
	Frame();
	const uint32_t lv = topic.Last()["layersVersion"].asUInt();
	ASSERT_EQ(topic.Last()["base"].asString(), "temp");
	clock.Advance(200);
	ASSERT_TRUE(registry.Unregister(StringCRC("temp")));
	Frame();
	ASSERT_EQ(topic.Count(), 2u);
	EXPECT_GT(topic.Last()["layersVersion"].asUInt(), lv);
	EXPECT_NE(topic.Last()["base"].asString(), "temp");
}

TEST(ViewStateGridSwitch, ChangingTheBaseToAnotherGridChangesTheGridBlock)
{
	TestGrid gridA{ 4, 3 };
	TestGrid gridB{ 2, 2 };
	gridB.desc.id = StringCRC("second");
	Dia::Graphics::CellFieldRegistry cellField;
	StubProvider pa, pb;
	pa.values = std::vector<float>(12, 1.0f);
	pb.values = std::vector<float>(4, 2.0f);
	FakeDebugDraw draw;
	FakeClock clock;
	TopicRecorder topic;
	FakePuId() = 1;
	CellLayerRegistry registry{ nullptr, &FakePuIdFn };
	LayerUploader uploader{ cellField, nullptr };
	CellLayerViewerDomain domain{ registry, uploader, nullptr };
	ViewStatePublisher publisher{ registry, domain, topic, clock };
	ASSERT_EQ(registry.Register(pa.MakeDesc("a", &gridA.desc)), CellRegisterResult::Ok);
	ASSERT_EQ(registry.Register(pb.MakeDesc("b", &gridB.desc)), CellRegisterResult::Ok);
	domain.Update(draw);
	publisher.Update();
	ASSERT_EQ(topic.Count(), 1u);
	EXPECT_EQ(topic.Last()["grid"]["id"].asString(), "grid.0");
	clock.Advance(200);
	ASSERT_TRUE(domain.SetBase(StringCRC("b")));
	domain.Update(draw);
	publisher.Update();
	ASSERT_EQ(topic.Count(), 2u);
	EXPECT_EQ(topic.Last()["grid"]["id"].asString(), "second");
	EXPECT_EQ(topic.Last()["grid"]["width"].asUInt(), 2u);
	EXPECT_EQ(topic.Last()["base"].asString(), "b");
}

#endif // DIA_DEBUG
