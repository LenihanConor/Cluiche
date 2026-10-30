#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerViewerDomain.h>
#include <DiaCellLayers/HotkeyTable.h>
#include <DiaCellLayers/LayerReader.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/LayerReaderTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>

#include <stdio.h>
#include <string>

// Task 10a (registry-and-viewer): unregister-while-in-use, re-registration, reentrancy from callbacks, churn beyond
// fixed slot tables, and null/degenerate inputs that the other Feature 1 test files do not cover.

#ifdef DIA_DEBUG

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Graphics::CellFieldRegistry;

namespace
{
	CellLayerRegistry* gRegistry = nullptr;
	uint32_t gDrawsA = 0;
	uint32_t gDrawsB = 0;
	void DrawA(Dia::Core::IDebugDraw&, const void*) { ++gDrawsA; }
	void DrawB(Dia::Core::IDebugDraw&, const void*) { ++gDrawsB; }
	void DrawThenUnregisterSelf(Dia::Core::IDebugDraw&, const void*) { ++gDrawsA; gRegistry->UnregisterOverlay(StringCRC("self")); }

	CellOverlayDesc CountingOverlay(const char* id, void (*fn)(Dia::Core::IDebugDraw&, const void*))
	{
		CellOverlayDesc o = MakeOverlay(id);
		o.draw = fastdelegate::FastDelegate2<Dia::Core::IDebugDraw&, const void*>(fn);
		return o;
	}

	struct LifecycleTest : ::testing::Test
	{
		TestGrid gridA{ 4, 3 };
		TestGrid gridB{ 5, 2 };
		CellFieldRegistry cellField;
		StubProvider temp, rain, soil;
		FakeDebugDraw draw;
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };
		LayerUploader uploader{ cellField, nullptr };
		CellLayerViewerDomain domain{ registry, uploader, nullptr };

		void SetUp() override
		{
			FakePuId() = 1;
			gDrawsA = gDrawsB = 0;
			gRegistry = &registry;
			temp.values = { 5, -2, 7, 3, 1, 0, 4, 9, 2, 2, 2, 2 };
			rain.values = std::vector<float>(12, 1.0f);
			soil.values = std::vector<float>(10, 3.0f);
		}
		void TearDown() override { gRegistry = nullptr; }
	};

	// A read delegate that unregisters its own layer from inside the read (reentrant registry mutation during Update).
	uint32_t gSelfReads = 0;
	void ReadAndUnregisterSelf(float* out, uint32_t n)
	{
		++gSelfReads;
		for (uint32_t i = 0; i < n; ++i) { out[i] = 1.0f; }
		gRegistry->Unregister(StringCRC("selfread"));
	}
}

// ---- overlay state vs registry churn ----

// FOUND_BUG candidate: the viewer keeps a fixed 64-slot enabled-state table that is never freed when an overlay is
// unregistered, so churning more than 64 distinct overlay ids makes ToggleOverlay fail although the registry has room.
TEST_F(LifecycleTest, OverlayToggleStillWorksAfterMoreThan64DistinctOverlayIdsChurned)
{
	registry.Register(temp.MakeDesc("temp", &gridA.desc));
	for (uint32_t i = 0; i < CellLayerRegistry::kMaxOverlays + 6; ++i)
	{
		char id[16];
		snprintf(id, sizeof(id), "churn%u", i);
		ASSERT_EQ(registry.RegisterOverlay(MakeOverlay(id)), CellRegisterResult::Ok);
		EXPECT_TRUE(domain.ToggleOverlay(StringCRC(id))) << "toggle failed for churned overlay #" << i;
		ASSERT_TRUE(registry.UnregisterOverlay(StringCRC(id)));
	}
	EXPECT_EQ(registry.OverlayCount(), 0u);
}

TEST_F(LifecycleTest, UnregisteredEnabledOverlayIsNotDrawnAndCannotBeToggled)
{
	registry.Register(temp.MakeDesc("temp", &gridA.desc));
	registry.RegisterOverlay(CountingOverlay("a", &DrawA));
	registry.RegisterOverlay(CountingOverlay("b", &DrawB));
	ASSERT_TRUE(domain.ToggleOverlay(StringCRC("a")));
	ASSERT_TRUE(domain.ToggleOverlay(StringCRC("b")));
	domain.Update(draw);
	EXPECT_EQ(gDrawsA, 1u);
	EXPECT_EQ(gDrawsB, 1u);

	ASSERT_TRUE(registry.UnregisterOverlay(StringCRC("a")));
	domain.Update(draw);
	EXPECT_EQ(gDrawsA, 1u);		// gone
	EXPECT_EQ(gDrawsB, 2u);		// the shifted neighbour still draws
	EXPECT_FALSE(domain.ToggleOverlay(StringCRC("a")));
	Json::Value j;
	domain.GetJSONState(j);
	EXPECT_EQ(j["overlays"].size(), 1u);
}

// Characterization: the enabled flag belongs to the viewer and is keyed by id, so a re-registered overlay comes back
// in the state the user left it.
TEST_F(LifecycleTest, ReRegisteredOverlayKeepsItsEnabledState)
{
	registry.Register(temp.MakeDesc("temp", &gridA.desc));
	registry.RegisterOverlay(CountingOverlay("a", &DrawA));
	ASSERT_TRUE(domain.ToggleOverlay(StringCRC("a")));
	ASSERT_TRUE(registry.UnregisterOverlay(StringCRC("a")));
	domain.Update(draw);
	EXPECT_EQ(gDrawsA, 0u);
	ASSERT_EQ(registry.RegisterOverlay(CountingOverlay("a", &DrawA)), CellRegisterResult::Ok);
	EXPECT_TRUE(domain.IsOverlayEnabled(StringCRC("a")));
	domain.Update(draw);
	EXPECT_EQ(gDrawsA, 1u);
}

TEST_F(LifecycleTest, OverlayDrawCallbackMayUnregisterItselfDuringUpdate)
{
	registry.Register(temp.MakeDesc("temp", &gridA.desc));
	registry.RegisterOverlay(CountingOverlay("self", &DrawThenUnregisterSelf));
	registry.RegisterOverlay(CountingOverlay("b", &DrawB));
	ASSERT_TRUE(domain.ToggleOverlay(StringCRC("self")));
	ASSERT_TRUE(domain.ToggleOverlay(StringCRC("b")));
	domain.Update(draw);
	EXPECT_EQ(gDrawsA, 1u);
	EXPECT_EQ(registry.OverlayCount(), 1u);
	domain.Update(draw);
	EXPECT_EQ(gDrawsA, 1u);		// never drawn again
	EXPECT_GE(gDrawsB, 1u);		// the survivor is not lost
}

TEST_F(LifecycleTest, OverlayWithEmptyDrawDelegateIsSkippedWhenEnabled)
{
	registry.Register(temp.MakeDesc("temp", &gridA.desc));
	CellOverlayDesc o = MakeOverlay("nodraw");
	o.draw.clear();
	ASSERT_EQ(registry.RegisterOverlay(o), CellRegisterResult::Ok);
	ASSERT_TRUE(domain.ToggleOverlay(StringCRC("nodraw")));
	domain.Update(draw);
	EXPECT_TRUE(domain.IsOverlayEnabled(StringCRC("nodraw")));
}

// ---- layer unregister / re-register while active ----

TEST_F(LifecycleTest, UnregisterAndReRegisterActiveBaseBetweenFramesKeepsItActive)
{
	registry.Register(temp.MakeDesc("temp", &gridA.desc));
	registry.Register(rain.MakeDesc("rain", &gridA.desc));
	ASSERT_TRUE(domain.SetBase(StringCRC("rain")));
	domain.Update(draw);
	ASSERT_TRUE(registry.Unregister(StringCRC("rain")));
	ASSERT_EQ(registry.Register(rain.MakeDesc("rain", &gridA.desc)), CellRegisterResult::Ok);	// before the next frame
	domain.Update(draw);
	EXPECT_EQ(domain.ActiveBase(), StringCRC("rain"));
	EXPECT_EQ(domain.ActiveGrid(), &gridA.desc);
}

TEST_F(LifecycleTest, UnregisterThenReRegisterAcrossAFrameFallsBackAndStaysOnFallback)
{
	registry.Register(temp.MakeDesc("temp", &gridA.desc));
	registry.Register(rain.MakeDesc("rain", &gridA.desc));
	ASSERT_TRUE(domain.SetBase(StringCRC("rain")));
	domain.Update(draw);
	ASSERT_TRUE(registry.Unregister(StringCRC("rain")));
	domain.Update(draw);
	EXPECT_EQ(domain.ActiveBase(), StringCRC("temp"));
	ASSERT_EQ(registry.Register(rain.MakeDesc("rain", &gridA.desc)), CellRegisterResult::Ok);
	domain.Update(draw);
	EXPECT_EQ(domain.ActiveBase(), StringCRC("temp"));	// a returning layer does not steal the base
	EXPECT_TRUE(domain.SetBase(StringCRC("rain")));
}

TEST_F(LifecycleTest, NewProviderUnderSameIdIsReadWhenItsVersionDiffers)
{
	registry.Register(temp.MakeDesc("temp", &gridA.desc));
	domain.Update(draw);
	const uint64_t readsBefore = uploader.Reads();
	ASSERT_TRUE(registry.Unregister(StringCRC("temp")));
	StubProvider replacement;
	replacement.values = std::vector<float>(12, 42.0f);
	replacement.versionValue = temp.versionValue + 1;
	ASSERT_EQ(registry.Register(replacement.MakeDesc("temp", &gridA.desc)), CellRegisterResult::Ok);
	domain.Update(draw);
	EXPECT_EQ(uploader.Reads(), readsBefore + 1);
	EXPECT_EQ(replacement.readCalls, 1u);
	uint32_t n = 0;
	const float* v = uploader.BaseValues(StringCRC("temp"), n);
	ASSERT_NE(v, nullptr);
	ASSERT_EQ(n, 12u);
	EXPECT_EQ(v[0], 42.0f);
}

// A read delegate that removes its own layer mid-update must not crash; the next frame recovers via the empty state.
TEST_F(LifecycleTest, ReadDelegateUnregisteringItsOwnLayerDoesNotCrashAndNextFrameRecovers)
{
	registry.Register(temp.MakeDesc("temp", &gridA.desc));
	CellLayerDesc d = MakeLayer("selfread", &gridA.desc);
	d.read = fastdelegate::FastDelegate2<float*, uint32_t>(&ReadAndUnregisterSelf);
	registry.Register(d);
	gSelfReads = 0;
	ASSERT_TRUE(domain.SetBase(StringCRC("selfread")));
	domain.Update(draw);
	EXPECT_EQ(gSelfReads, 1u);
	EXPECT_EQ(registry.Find(StringCRC("selfread")), nullptr);
	domain.Update(draw);
	EXPECT_EQ(domain.ActiveBase(), StringCRC("temp"));
	EXPECT_EQ(gSelfReads, 1u);
}

// A layer without a grid is rejected, so it can never reach the viewer or the JSON writers.
TEST_F(LifecycleTest, LayerRegisteredWithNullGridIsRejectedAndRegistryUnchanged)
{
	const uint32_t version = registry.LayersVersion();
	EXPECT_EQ(registry.Register(MakeLayer("nogrid", nullptr)), CellRegisterResult::NullGrid);
	EXPECT_EQ(registry.Find(StringCRC("nogrid")), nullptr);
	EXPECT_EQ(registry.LayersVersion(), version);
	domain.Update(draw);
	EXPECT_EQ(draw.cellFieldRequests, 0u);
}

TEST_F(LifecycleTest, ValidLayerAfterARejectedNullGridLayerStillBecomesTheBase)
{
	registry.Register(MakeLayer("nogrid", nullptr));
	domain.Update(draw);
	registry.Register(temp.MakeDesc("temp", &gridA.desc));
	domain.Update(draw);
	EXPECT_EQ(domain.ActiveBase(), StringCRC("temp"));
}

TEST_F(LifecycleTest, RejectedNullGridLayerLeavesJsonStateAndListLayersUsable)
{
	registry.Register(MakeLayer("nogrid", nullptr));
	domain.Update(draw);
	Json::Value j;
	domain.GetJSONState(j);
	EXPECT_TRUE(j.isMember("layers"));
	Json::Value l;
	domain.ListLayers(l);
	EXPECT_TRUE(l.isMember("grids"));
}

TEST_F(LifecycleTest, UnregisterEveryLayerThenRegisterOneRecoversWithOneUpload)
{
	registry.Register(temp.MakeDesc("temp", &gridA.desc));
	registry.Register(soil.MakeDesc("soil", &gridB.desc));
	domain.Update(draw);
	registry.Unregister(StringCRC("temp"));
	registry.Unregister(StringCRC("soil"));
	domain.Update(draw);
	EXPECT_TRUE(domain.IsEmptyState());
	const uint64_t uploads = uploader.Uploads();
	registry.Register(rain.MakeDesc("rain", &gridB.desc));
	domain.Update(draw);
	domain.Update(draw);
	EXPECT_EQ(domain.ActiveBase(), StringCRC("rain"));
	EXPECT_EQ(uploader.Uploads(), uploads + 1);
}

// ---- HotkeyTable degenerate inputs ----

TEST(LifecycleHotkey, LookupAndHotkeyOfOnEmptyOrUnknownInputs)
{
	Dia::Graphics::CellGridDesc grid;
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	reg.Register(MakeLayer("a", &grid));
	HotkeyTable table(reg, nullptr);
	EXPECT_EQ(table.BindingCount(), 0u);
	EXPECT_EQ(table.Lookup('a'), nullptr);		// before any Rebuild
	EXPECT_EQ(table.Lookup('\0'), nullptr);
	EXPECT_EQ(table.HotkeyOf(StringCRC("a")), 0);
	EXPECT_EQ(table.HotkeyOf(StringCRC("nope")), 0);
	table.Rebuild(&grid);
	EXPECT_EQ(table.Lookup('\0'), nullptr);		// the NUL key is never a binding
	EXPECT_EQ(table.HotkeyOf(StringCRC("nope")), 0);
}

TEST(LifecycleHotkey, GroupNumberOfUnknownLayerIsZero)
{
	Dia::Graphics::CellGridDesc grid;
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	reg.Register(MakeLayer("a", &grid));
	EXPECT_EQ(HotkeyTable::GroupNumber(reg, &grid, StringCRC("nope")), 0u);
	EXPECT_EQ(HotkeyTable::GroupNumber(reg, nullptr, StringCRC("a")), 0u);
	EXPECT_EQ(HotkeyTable::GroupNumber(reg, &grid, StringCRC("a")), 1u);
}

// ---- LayerReader degenerate inputs ----

TEST(LifecycleReader, SlotCountIsClampedToOneAndMax)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	LayerReader zero(reg, 0);
	EXPECT_EQ(zero.SlotCount(), 1u);
	LayerReader huge(reg, 100000);
	EXPECT_EQ(huge.SlotCount(), CellLayerRegistry::kMaxLayers);
}

TEST(LifecycleReader, SingleSlotAlternatingLayersRereadsEveryTimeButStaysCorrect)
{
	Dia::Graphics::CellGridDesc grid = MakeGrid(2, 2);
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	CountingLayerProvider a, b;
	a.values = { 1, 2, 3, 4 };
	b.values = { 10, 20, 30, 40 };
	reg.Register(MakeCountingLayer("a", &grid, a));
	reg.Register(MakeCountingLayer("b", &grid, b));
	LayerReader reader(reg, 1);
	for (int i = 0; i < 3; ++i)
	{
		const LayerValues va = reader.Read(StringCRC("a"));
		ASSERT_TRUE(va.hasData);
		EXPECT_EQ(va.values[3], 4.0f);
		const LayerValues vb = reader.Read(StringCRC("b"));
		ASSERT_TRUE(vb.hasData);
		EXPECT_EQ(vb.values[3], 40.0f);
	}
	EXPECT_EQ(a.readCalls, 3u);
	EXPECT_EQ(b.readCalls, 3u);
}

TEST(LifecycleReader, LayerWithoutReadOrVersionOrGridHasNoDataAndNoCall)
{
	Dia::Graphics::CellGridDesc grid = MakeGrid(2, 2);
	Dia::Graphics::CellGridDesc zeroGrid = MakeGrid(0, 0);
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	CountingLayerProvider p;
	CellLayerDesc noRead = MakeCountingLayer("noread", &grid, p);
	noRead.read.clear();
	CellLayerDesc noVersion = MakeCountingLayer("nover", &grid, p);
	noVersion.version.clear();
	reg.Register(noRead);
	reg.Register(noVersion);
	reg.Register(MakeCountingLayer("nogrid", nullptr, p));
	reg.Register(MakeCountingLayer("zerogrid", &zeroGrid, p));
	LayerReader reader(reg);
	for (const char* id : { "noread", "nover", "nogrid", "zerogrid", "unknown" })
	{
		const LayerValues v = reader.Read(StringCRC(id));
		EXPECT_FALSE(v.hasData) << id;
		EXPECT_EQ(v.values, nullptr) << id;
		EXPECT_EQ(v.count, 0u) << id;
	}
	EXPECT_EQ(p.readCalls, 0u);
	EXPECT_EQ(reader.Reads(), 0u);
}

TEST(LifecycleReader, InvalidateOnEmptyReaderAndUnregisteredLayerAreSafe)
{
	Dia::Graphics::CellGridDesc grid = MakeGrid(2, 2);
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	CountingLayerProvider p;
	p.values = { 1, 2, 3, 4 };
	reg.Register(MakeCountingLayer("a", &grid, p));
	LayerReader reader(reg);
	reader.Invalidate();
	ASSERT_TRUE(reader.Read(StringCRC("a")).hasData);
	reg.Unregister(StringCRC("a"));
	EXPECT_FALSE(reader.Read(StringCRC("a")).hasData);		// the cache never resurrects an unregistered layer
	reader.Invalidate();
	reader.Invalidate();
	EXPECT_EQ(p.readCalls, 1u);
}

TEST(LifecycleReader, ReRegisteredSameIdWithNewVersionRereadsNewProvider)
{
	Dia::Graphics::CellGridDesc grid = MakeGrid(2, 2);
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	CountingLayerProvider oldP, newP;
	oldP.values = { 1, 1, 1, 1 };
	newP.values = { 7, 7, 7, 7 };
	newP.version = 99;
	reg.Register(MakeCountingLayer("a", &grid, oldP));
	LayerReader reader(reg);
	ASSERT_EQ(reader.Read(StringCRC("a")).values[0], 1.0f);
	reg.Unregister(StringCRC("a"));
	reg.Register(MakeCountingLayer("a", &grid, newP));
	const LayerValues v = reader.Read(StringCRC("a"));
	ASSERT_TRUE(v.hasData);
	EXPECT_EQ(v.values[0], 7.0f);
	EXPECT_EQ(newP.readCalls, 1u);
}

#endif
