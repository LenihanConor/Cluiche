#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/ViewValueSourceTestSupport.h>

#include <algorithm>
#include <random>
#include <stdio.h>
#include <string>
#include <vector>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Graphics::CellGridDesc;

namespace
{
	struct RegistryEdgeTest : ::testing::Test
	{
		CellGridDesc gridA, gridB, gridC, gridD;
		void SetUp() override { FakePuId() = 1; }

		static std::string Id(const char* prefix, uint32_t i) { char b[32]; snprintf(b, sizeof(b), "%s%u", prefix, i); return b; }
	};
}

TEST_F(RegistryEdgeTest, Layer128thAcceptedAnd129thRejectedLeavingStateIntact)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	for (uint32_t i = 0; i < 127; ++i) { ASSERT_EQ(reg.Register(MakeLayer(Id("l", i).c_str(), &gridA)), CellRegisterResult::Ok); }
	EXPECT_EQ(reg.Register(MakeLayer("l127", &gridA)), CellRegisterResult::Ok);
	EXPECT_EQ(reg.LayerCount(), 128u);
	EXPECT_EQ(reg.Register(MakeLayer("l128", &gridA)), CellRegisterResult::CapacityReached);
	EXPECT_EQ(reg.LayerCount(), 128u);
	EXPECT_EQ(reg.Find(StringCRC("l128")), nullptr);
	EXPECT_EQ(reg.LayerAt(127)->id, StringCRC("l127"));
	EXPECT_EQ(reg.LayerAt(128), nullptr);
}

TEST_F(RegistryEdgeTest, CapacityFreedByUnregisterAllowsRegisterAgain)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	for (uint32_t i = 0; i < 128; ++i) { reg.Register(MakeLayer(Id("l", i).c_str(), &gridA)); }
	ASSERT_TRUE(reg.Unregister(StringCRC("l5")));
	EXPECT_EQ(reg.Register(MakeLayer("extra", &gridA)), CellRegisterResult::Ok);
	EXPECT_EQ(reg.LayerAt(127)->id, StringCRC("extra"));
	EXPECT_EQ(reg.Register(MakeLayer("extra2", &gridA)), CellRegisterResult::CapacityReached);
}

TEST_F(RegistryEdgeTest, Overlay64thAcceptedAnd65thRejected)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	for (uint32_t i = 0; i < 64; ++i) { ASSERT_EQ(reg.RegisterOverlay(MakeOverlay(Id("o", i).c_str())), CellRegisterResult::Ok); }
	EXPECT_EQ(reg.OverlayCount(), 64u);
	EXPECT_EQ(reg.RegisterOverlay(MakeOverlay("o64")), CellRegisterResult::CapacityReached);
	EXPECT_EQ(reg.OverlayCount(), 64u);
	EXPECT_EQ(reg.OverlayAt(64), nullptr);
	EXPECT_TRUE(reg.UnregisterOverlay(StringCRC("o0")));
	EXPECT_EQ(reg.RegisterOverlay(MakeOverlay("o64")), CellRegisterResult::Ok);
}

TEST_F(RegistryEdgeTest, DuplicateCheckedBeforeCapacity)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	for (uint32_t i = 0; i < 128; ++i) { reg.Register(MakeLayer(Id("l", i).c_str(), &gridA)); }
	EXPECT_EQ(reg.Register(MakeLayer("l3", &gridA)), CellRegisterResult::Duplicate);
}

TEST_F(RegistryEdgeTest, WrongPuCheckedBeforeDuplicateAndCapacity)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	reg.Register(MakeLayer("a", &gridA));
	FakePuId() = 9;
	EXPECT_EQ(reg.Register(MakeLayer("a", &gridA)), CellRegisterResult::WrongProcessingUnit);
	EXPECT_EQ(reg.RegisterOverlay(MakeOverlay("o")), CellRegisterResult::WrongProcessingUnit);
	FakePuId() = 1;
	EXPECT_EQ(reg.LayerCount(), 1u);
	EXPECT_EQ(reg.OverlayCount(), 0u);
}

TEST_F(RegistryEdgeTest, OwnerIsPuAtConstructionNotAtRegistration)
{
	FakePuId() = 4;
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	FakePuId() = 1;
	EXPECT_EQ(reg.Register(MakeLayer("a", &gridA)), CellRegisterResult::WrongProcessingUnit);
	FakePuId() = 4;
	EXPECT_EQ(reg.Register(MakeLayer("a", &gridA)), CellRegisterResult::Ok);
	FakePuId() = 1;
}

TEST_F(RegistryEdgeTest, UnregisterThenReRegisterSameIdGoesToEnd)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	reg.Register(MakeLayer("a", &gridA));
	reg.Register(MakeLayer("b", &gridA));
	reg.Register(MakeLayer("c", &gridA));
	ASSERT_TRUE(reg.Unregister(StringCRC("a")));
	EXPECT_EQ(reg.Register(MakeLayer("a", &gridB)), CellRegisterResult::Ok);
	ASSERT_EQ(reg.LayerCount(), 3u);
	EXPECT_EQ(reg.LayerAt(0)->id, StringCRC("b"));
	EXPECT_EQ(reg.LayerAt(1)->id, StringCRC("c"));
	EXPECT_EQ(reg.LayerAt(2)->id, StringCRC("a"));
	EXPECT_EQ(reg.Find(StringCRC("a"))->grid, &gridB);
}

TEST_F(RegistryEdgeTest, UnregisterTwiceSecondReturnsFalse)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	reg.Register(MakeLayer("a", &gridA));
	EXPECT_TRUE(reg.Unregister(StringCRC("a")));
	EXPECT_FALSE(reg.Unregister(StringCRC("a")));
	EXPECT_FALSE(reg.UnregisterOverlay(StringCRC("a")));
	EXPECT_FALSE(reg.Unregister(StringCRC::kZero));
}

TEST_F(RegistryEdgeTest, OverlayAndLayerIdSpacesAreIndependent)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	EXPECT_EQ(reg.Register(MakeLayer("x", &gridA)), CellRegisterResult::Ok);
	EXPECT_EQ(reg.RegisterOverlay(MakeOverlay("x")), CellRegisterResult::Ok);
	EXPECT_TRUE(reg.Unregister(StringCRC("x")));
	EXPECT_NE(reg.FindOverlay(StringCRC("x")), nullptr);
}

TEST_F(RegistryEdgeTest, UnregisterRemovesStaleSlotData)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	reg.Register(MakeLayer("a", &gridA));
	reg.Register(MakeLayer("b", &gridB));
	reg.Unregister(StringCRC("b"));
	EXPECT_EQ(reg.LayerAt(1), nullptr);
	EXPECT_EQ(reg.GridCount(), 1u);
	EXPECT_EQ(reg.LayerCountForGrid(&gridB), 0u);
	EXPECT_EQ(reg.DefaultBase(&gridB), nullptr);
}

TEST_F(RegistryEdgeTest, ThreeGridsGroupedInFirstRegistrationOrder)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	reg.Register(MakeLayer("a1", &gridA));
	reg.Register(MakeLayer("b1", &gridB));
	reg.Register(MakeLayer("c1", &gridC));
	reg.Register(MakeLayer("a2", &gridA));
	reg.Register(MakeLayer("c2", &gridC));
	reg.Register(MakeLayer("b2", &gridB));
	reg.Register(MakeLayer("a3", &gridA));
	ASSERT_EQ(reg.GridCount(), 3u);
	EXPECT_EQ(reg.GridAt(0), &gridA);
	EXPECT_EQ(reg.GridAt(1), &gridB);
	EXPECT_EQ(reg.GridAt(2), &gridC);
	EXPECT_EQ(reg.GridAt(3), nullptr);
	EXPECT_EQ(reg.LayerCountForGrid(&gridA), 3u);
	EXPECT_EQ(reg.LayerCountForGrid(&gridB), 2u);
	EXPECT_EQ(reg.LayerCountForGrid(&gridC), 2u);
	EXPECT_EQ(reg.LayerOfGrid(&gridA, 2)->id, StringCRC("a3"));
	EXPECT_EQ(reg.LayerOfGrid(&gridC, 1)->id, StringCRC("c2"));
	EXPECT_EQ(reg.LayerOfGrid(&gridB, 2), nullptr);
	EXPECT_EQ(reg.LayerOfGrid(&gridD, 0), nullptr);
}

TEST_F(RegistryEdgeTest, GridOrderFollowsRemainingFirstLayerAfterUnregister)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	reg.Register(MakeLayer("a1", &gridA));
	reg.Register(MakeLayer("b1", &gridB));
	reg.Register(MakeLayer("a2", &gridA));
	reg.Unregister(StringCRC("a1"));
	ASSERT_EQ(reg.GridCount(), 2u);
	EXPECT_EQ(reg.GridAt(0), &gridB);
	EXPECT_EQ(reg.GridAt(1), &gridA);
}

TEST_F(RegistryEdgeTest, IsMultiGridTransitions)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	EXPECT_FALSE(reg.IsMultiGrid());
	reg.Register(MakeLayer("a", &gridA));
	EXPECT_FALSE(reg.IsMultiGrid());
	reg.Register(MakeLayer("a2", &gridA));
	EXPECT_FALSE(reg.IsMultiGrid());
	reg.Register(MakeLayer("b", &gridB));
	EXPECT_TRUE(reg.IsMultiGrid());
	reg.Register(MakeLayer("c", &gridC));
	EXPECT_TRUE(reg.IsMultiGrid());
	reg.Unregister(StringCRC("c"));
	EXPECT_TRUE(reg.IsMultiGrid());
	reg.Unregister(StringCRC("b"));
	EXPECT_FALSE(reg.IsMultiGrid());
	reg.Unregister(StringCRC("a"));
	reg.Unregister(StringCRC("a2"));
	EXPECT_FALSE(reg.IsMultiGrid());
	EXPECT_EQ(reg.GridCount(), 0u);
}

TEST_F(RegistryEdgeTest, DefaultBaseMultipleFlagsPicksFirstFlagged)
{
	UploaderSchemaFixture fx;
	for (const char* k : { "a", "b", "c", "d" })
	{
		auto e = UploaderSchemaFixture::FloatEntry(k, 0, 1);
		if (k[0] != 'a') { e.mUi["default_base"] = true; }
		fx.Add(e);
	}
	const auto& svc = fx.Publish();
	CellLayerRegistry reg(&svc, &FakePuIdFn);
	for (const char* k : { "a", "b", "c", "d" }) { reg.Register(MakeLayer(k, &gridA, (std::string("cells/") + k).c_str())); }
	EXPECT_EQ(reg.DefaultBase(&gridA)->id, StringCRC("b"));
	reg.Unregister(StringCRC("b"));
	EXPECT_EQ(reg.DefaultBase(&gridA)->id, StringCRC("c"));
	reg.Unregister(StringCRC("c"));
	reg.Unregister(StringCRC("d"));
	EXPECT_EQ(reg.DefaultBase(&gridA)->id, StringCRC("a"));
}

TEST_F(RegistryEdgeTest, DefaultBaseIgnoresNonBoolFalseAndUnknownKeys)
{
	UploaderSchemaFixture fx;
	auto s = UploaderSchemaFixture::FloatEntry("s", 0, 1); s.mUi["default_base"] = "true"; fx.Add(s);
	auto f = UploaderSchemaFixture::FloatEntry("f", 0, 1); f.mUi["default_base"] = false; fx.Add(f);
	auto n = UploaderSchemaFixture::FloatEntry("n", 0, 1); n.mUi["default_base"] = 1; fx.Add(n);
	const auto& svc = fx.Publish();
	CellLayerRegistry reg(&svc, &FakePuIdFn);
	reg.Register(MakeLayer("ghost", &gridA, "cells/ghost"));
	reg.Register(MakeLayer("s", &gridA, "cells/s"));
	reg.Register(MakeLayer("f", &gridA, "cells/f"));
	reg.Register(MakeLayer("n", &gridA, "cells/n"));
	EXPECT_EQ(reg.DefaultBase(&gridA)->id, StringCRC("ghost"));
}

TEST_F(RegistryEdgeTest, DefaultBaseIsPerGridAndNullForUnknownGrid)
{
	UploaderSchemaFixture fx;
	auto a = UploaderSchemaFixture::FloatEntry("b2", 0, 1); a.mUi["default_base"] = true; fx.Add(a);
	const auto& svc = fx.Publish();
	CellLayerRegistry reg(&svc, &FakePuIdFn);
	reg.Register(MakeLayer("a1", &gridA, "cells/a1"));
	reg.Register(MakeLayer("b1", &gridB, "cells/b1"));
	reg.Register(MakeLayer("b2", &gridB, "cells/b2"));
	EXPECT_EQ(reg.DefaultBase(&gridA)->id, StringCRC("a1"));
	EXPECT_EQ(reg.DefaultBase(&gridB)->id, StringCRC("b2"));
	EXPECT_EQ(reg.DefaultBase(&gridC), nullptr);
}

TEST_F(RegistryEdgeTest, ViewSourceLiveTransitions)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	StubViewValueSource src;
	EXPECT_TRUE(reg.IsLive());
	EXPECT_EQ(reg.ViewValueSource(), nullptr);
	src.live = false;
	ASSERT_TRUE(reg.SetViewValueSource(&src));
	EXPECT_FALSE(reg.IsLive());
	src.live = true;
	EXPECT_TRUE(reg.IsLive());
	src.live = false;
	EXPECT_FALSE(reg.IsLive());
	reg.ClearViewValueSource();
	EXPECT_TRUE(reg.IsLive());
	reg.ClearViewValueSource();
	EXPECT_EQ(reg.ViewValueSource(), nullptr);
}

TEST(RegistryEdgeNoFixture, SchemaWithUnknownKeyAndNullSchemaBothFallBackToFirst)
{
	CellGridDesc g;
	FakePuId() = 1;
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	reg.Register(MakeLayer("z", &g));
	reg.Register(MakeLayer("y", &g));
	EXPECT_EQ(reg.DefaultBase(&g)->id, StringCRC("z"));
}

TEST_F(RegistryEdgeTest, RandomRegisterUnregisterSequenceMatchesModel)
{
	std::mt19937 rng(424242);
	CellGridDesc grids[4];
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	struct Model { std::string id; int grid; };
	std::vector<Model> model;
	std::vector<std::string> overlays;

	for (int iter = 0; iter < 2000; ++iter)
	{
		const uint32_t pick = rng() % 100;
		const std::string id = Id("n", rng() % 160);
		if (pick < 45)
		{
			const int gi = static_cast<int>(rng() % 4);
			const CellRegisterResult r = reg.Register(MakeLayer(id.c_str(), &grids[gi]));
			const bool dup = std::any_of(model.begin(), model.end(), [&](const Model& m) { return m.id == id; });
			if (dup) { ASSERT_EQ(r, CellRegisterResult::Duplicate); }
			else if (model.size() >= CellLayerRegistry::kMaxLayers) { ASSERT_EQ(r, CellRegisterResult::CapacityReached); }
			else { ASSERT_EQ(r, CellRegisterResult::Ok); model.push_back({ id, gi }); }
		}
		else if (pick < 85)
		{
			const bool had = std::any_of(model.begin(), model.end(), [&](const Model& m) { return m.id == id; });
			ASSERT_EQ(reg.Unregister(StringCRC(id.c_str())), had);
			model.erase(std::remove_if(model.begin(), model.end(), [&](const Model& m) { return m.id == id; }), model.end());
		}
		else if (pick < 93)
		{
			const bool dup = std::find(overlays.begin(), overlays.end(), id) != overlays.end();
			const CellRegisterResult r = reg.RegisterOverlay(MakeOverlay(id.c_str()));
			if (dup) { ASSERT_EQ(r, CellRegisterResult::Duplicate); }
			else if (overlays.size() >= CellLayerRegistry::kMaxOverlays) { ASSERT_EQ(r, CellRegisterResult::CapacityReached); }
			else { ASSERT_EQ(r, CellRegisterResult::Ok); overlays.push_back(id); }
		}
		else
		{
			const bool had = std::find(overlays.begin(), overlays.end(), id) != overlays.end();
			ASSERT_EQ(reg.UnregisterOverlay(StringCRC(id.c_str())), had);
			overlays.erase(std::remove(overlays.begin(), overlays.end(), id), overlays.end());
		}

		ASSERT_EQ(reg.LayerCount(), model.size());
		ASSERT_EQ(reg.OverlayCount(), overlays.size());
		for (size_t i = 0; i < model.size(); ++i)
		{
			ASSERT_EQ(reg.LayerAt(static_cast<uint32_t>(i))->id, StringCRC(model[i].id.c_str()));
			ASSERT_EQ(reg.LayerAt(static_cast<uint32_t>(i))->grid, &grids[model[i].grid]);
		}
		ASSERT_EQ(reg.LayerAt(static_cast<uint32_t>(model.size())), nullptr);
		for (size_t i = 0; i < overlays.size(); ++i) { ASSERT_EQ(reg.OverlayAt(static_cast<uint32_t>(i))->id, StringCRC(overlays[i].c_str())); }

		uint32_t gridsSeen = 0, summed = 0;
		for (int gi = 0; gi < 4; ++gi)
		{
			uint32_t n = 0;
			for (const Model& m : model) { if (m.grid == gi) { ++n; } }
			ASSERT_EQ(reg.LayerCountForGrid(&grids[gi]), n);
			if (n > 0) { ++gridsSeen; }
			summed += n;
		}
		ASSERT_EQ(summed, reg.LayerCount());
		ASSERT_EQ(reg.GridCount(), gridsSeen);
		ASSERT_EQ(reg.IsMultiGrid(), gridsSeen > 1);
	}
}

TEST_F(RegistryEdgeTest, RejectedRegistrationsCountsDuplicateCapacityAndWrongPuForLayersAndOverlays)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	EXPECT_EQ(reg.RejectedRegistrations(), 0u);
	ASSERT_EQ(reg.Register(MakeLayer("a", &gridA)), CellRegisterResult::Ok);
	ASSERT_EQ(reg.RegisterOverlay(MakeOverlay("o")), CellRegisterResult::Ok);
	EXPECT_EQ(reg.RejectedRegistrations(), 0u);	// successes and unregisters never count
	reg.Unregister(StringCRC("missing"));
	EXPECT_EQ(reg.RejectedRegistrations(), 0u);

	EXPECT_EQ(reg.Register(MakeLayer("a", &gridA)), CellRegisterResult::Duplicate);
	EXPECT_EQ(reg.RejectedRegistrations(), 1u);
	EXPECT_EQ(reg.RegisterOverlay(MakeOverlay("o")), CellRegisterResult::Duplicate);
	EXPECT_EQ(reg.RejectedRegistrations(), 2u);

	FakePuId() = 9;
	EXPECT_EQ(reg.Register(MakeLayer("b", &gridA)), CellRegisterResult::WrongProcessingUnit);
	EXPECT_EQ(reg.RegisterOverlay(MakeOverlay("p")), CellRegisterResult::WrongProcessingUnit);
	FakePuId() = 1;
	EXPECT_EQ(reg.RejectedRegistrations(), 4u);

	for (uint32_t i = 0; i < 127; ++i) { reg.Register(MakeLayer(Id("l", i).c_str(), &gridA)); }
	EXPECT_EQ(reg.Register(MakeLayer("over", &gridA)), CellRegisterResult::CapacityReached);
	EXPECT_EQ(reg.RejectedRegistrations(), 5u);
	for (uint32_t i = 0; i < 63; ++i) { reg.RegisterOverlay(MakeOverlay(Id("o", i).c_str())); }
	EXPECT_EQ(reg.RegisterOverlay(MakeOverlay("over")), CellRegisterResult::CapacityReached);
	EXPECT_EQ(reg.RejectedRegistrations(), 6u);
}
