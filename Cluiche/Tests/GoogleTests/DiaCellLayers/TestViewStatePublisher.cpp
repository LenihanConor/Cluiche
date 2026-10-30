#include <gtest/gtest.h>
#include <DiaCellLayers/ViewStatePublisher.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/ViewStateTestSupport.h>

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

#endif // DIA_DEBUG
