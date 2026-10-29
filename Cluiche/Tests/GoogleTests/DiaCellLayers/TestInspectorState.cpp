#include <gtest/gtest.h>
#include <memory>
#include <DiaCellLayers/CellSelection.h>
#include <DiaCellLayers/InspectorState.h>
#include <DiaCellLayers/LayerReader.h>
#include <DiaCellLayers/Testing/LayerReaderTestSupport.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;

namespace
{
	struct SelectionInspectorStateTest : ::testing::Test
	{
		Dia::Graphics::CellGridDesc grid = MakeGrid(4, 3);
		Dia::Graphics::CellGridDesc otherGrid = MakeGrid(2, 2);
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };
		CountingLayerProvider pa, pb, pc, pd, pe;
		CellSelection sel;
		InspectorState state;

		void AddLayer(const char* id, const Dia::Graphics::CellGridDesc* g, CountingLayerProvider& p, const char* group)
		{
			CellLayerDesc d = MakeCountingLayer(id, g, p);
			d.group.Clear();
			d.group.Append(group);
			ASSERT_EQ(CellRegisterResult::Ok, registry.Register(d));
		}

		void SetUp() override
		{
			FakePuId() = 1;
			for (CountingLayerProvider* p : { &pa, &pb, &pc, &pd, &pe }) { p->values = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 }; }
			AddLayer("a", &grid, pa, "G1");
			AddLayer("b", &grid, pb, "G1");
			AddLayer("c", &grid, pc, "G2");
			AddLayer("d", &grid, pd, "G2");
			AddLayer("e", &otherGrid, pe, "G1");
			sel.Reset(&grid);
			sel.Set(1, true);
			sel.Set(2, true);
			state.SetActive(registry, &grid, StringCRC("a"));
		}

		LayerReader& Reader()
		{
			if (!mReader) { mReader = std::make_unique<LayerReader>(registry); }
			return *mReader;
		}
		bool Tick() { return state.Tick(registry, Reader(), nullptr, &sel); }
		void Settle() { for (int i = 0; i < 4 && state.IsUpdating(); ++i) { Tick(); } }
	void Pump() { Tick(); Tick(); Tick(); }	// detect a change, then run its pass
		bool Has(const char* id) const { return state.StatsFor(StringCRC(id)) != nullptr; }

		std::unique_ptr<LayerReader> mReader;
	};
}

TEST_F(SelectionInspectorStateTest, ActiveGroupComputedByDefaultOthersNot)
{
	Settle();
	EXPECT_TRUE(Has("a"));
	EXPECT_TRUE(Has("b"));
	EXPECT_FALSE(Has("c"));
	EXPECT_FALSE(Has("d"));
	EXPECT_EQ(2u, state.ShownLayerCount());
	EXPECT_EQ(StringCRC("a"), state.ShownLayerAt(0));
	EXPECT_EQ(0u, pc.readCalls);
	EXPECT_EQ(0u, pd.readCalls);
	EXPECT_TRUE(state.IsGroupExpanded("G1"));
	EXPECT_FALSE(state.IsGroupExpanded("G2"));
	EXPECT_DOUBLE_EQ(2.5, state.StatsFor(StringCRC("a"))->mean);
}

TEST_F(SelectionInspectorStateTest, ExpandingGroupComputesExactlyItsLayers)
{
	Settle();
	const uint32_t passes = state.PassCount();
	state.SetGroupExpanded("G2", true);
	EXPECT_TRUE(state.IsUpdating());
	EXPECT_TRUE(Tick());
	EXPECT_EQ(passes + 1, state.PassCount());
	EXPECT_EQ(2u, state.LastPassLayerCount());
	EXPECT_TRUE(Has("c"));
	EXPECT_TRUE(Has("d"));
	EXPECT_EQ(1u, pc.readCalls);
	EXPECT_EQ(1u, pd.readCalls);
	EXPECT_EQ(1u, pa.readCalls);	// already-computed layers are not re-read
}

TEST_F(SelectionInspectorStateTest, CollapsedGroupStopsRecomputing)
{
	state.SetGroupExpanded("G2", true);
	Settle();
	state.SetGroupExpanded("G2", false);
	Settle();
	EXPECT_FALSE(Has("c"));
	EXPECT_EQ(2u, state.ShownLayerCount());
	const uint32_t reads = pc.readCalls;
	pc.Bump();
	Tick();
	Tick();
	EXPECT_EQ(reads, pc.readCalls);
}

TEST_F(SelectionInspectorStateTest, OnePassPerTickAndNoneWhenNothingChanged)
{
	Settle();
	const uint32_t passes = state.PassCount();
	const uint32_t reads = Reader().Reads();
	for (int i = 0; i < 5; ++i)
	{
		EXPECT_FALSE(Tick());
		EXPECT_FALSE(state.IsUpdating());
	}
	EXPECT_EQ(passes, state.PassCount());
	EXPECT_EQ(reads, Reader().Reads());
}

TEST_F(SelectionInspectorStateTest, SelectionVersionChangeRecomputesAllShownOnce)
{
	Settle();
	sel.Set(5, true);
	EXPECT_FALSE(Tick());			// detected this frame
	EXPECT_TRUE(state.IsUpdating());	// card shows the updating mark
	const uint32_t reads = Reader().Reads();
	EXPECT_TRUE(Tick());			// pass next frame
	EXPECT_FALSE(state.IsUpdating());
	EXPECT_EQ(2u, state.LastPassLayerCount());
	EXPECT_EQ(reads, Reader().Reads());	// versions unchanged: LayerReader cache serves both layers
	EXPECT_EQ(3u, state.StatsFor(StringCRC("a"))->count);
	EXPECT_FALSE(Tick());
}

TEST_F(SelectionInspectorStateTest, ChangeDuringPendingFrameIsCoveredBySamePass)
{
	Settle();
	sel.Set(5, true);
	Tick();
	sel.Set(6, true);	// changes again before the pass
	EXPECT_TRUE(Tick());
	EXPECT_FALSE(state.IsUpdating());
	EXPECT_EQ(4u, state.StatsFor(StringCRC("a"))->count);
	EXPECT_FALSE(Tick());
}

TEST_F(SelectionInspectorStateTest, OneLayerVersionBumpRecomputesOnlyThatLayer)
{
	Settle();
	pb.values[1] = 100.0f;
	pb.Bump();
	EXPECT_FALSE(Tick());
	EXPECT_TRUE(state.IsUpdating());
	const uint32_t readsA = pa.readCalls;
	const uint32_t readsB = pb.readCalls;
	EXPECT_TRUE(Tick());
	EXPECT_EQ(1u, state.LastPassLayerCount());
	EXPECT_EQ(readsA, pa.readCalls);
	EXPECT_EQ(readsB + 1, pb.readCalls);
	EXPECT_FLOAT_EQ(100.0f, state.StatsFor(StringCRC("b"))->max);
}

TEST_F(SelectionInspectorStateTest, EmptySelectionComputesNothingAndNeverReads)
{
	sel.Clear();
	for (int i = 0; i < 3; ++i) { EXPECT_FALSE(Tick()); }
	EXPECT_FALSE(state.HasSelection());
	EXPECT_FALSE(state.IsUpdating());
	EXPECT_EQ(0u, pa.readCalls);
	EXPECT_EQ(0u, pb.readCalls);
	EXPECT_EQ(0u, state.PassCount());
	EXPECT_FALSE(Has("a"));
	EXPECT_FALSE(state.Tick(registry, Reader(), nullptr, nullptr));
	EXPECT_EQ(0u, pa.readCalls);
}

TEST_F(SelectionInspectorStateTest, ClearingSelectionDropsStatsThenReselectRecomputes)
{
	Settle();
	ASSERT_TRUE(Has("a"));
	sel.Clear();
	Tick();
	EXPECT_FALSE(state.HasSelection());
	EXPECT_FALSE(Has("a"));
	sel.Set(3, true);
	Tick();
	EXPECT_TRUE(Tick());
	EXPECT_TRUE(state.HasSelection());
	EXPECT_EQ(1u, state.StatsFor(StringCRC("a"))->count);
}

TEST_F(SelectionInspectorStateTest, ActiveGridChangeClearsAndOtherGridLayersIgnored)
{
	state.SetGroupExpanded("G2", true);
	Settle();
	ASSERT_TRUE(Has("c"));
	EXPECT_EQ(0u, pe.readCalls);	// other-grid layer never read while grid is active
	CellSelection otherSel;
	otherSel.Reset(&otherGrid);
	otherSel.Set(0, true);
	state.SetActive(registry, &otherGrid, StringCRC("e"));
	EXPECT_TRUE(state.IsUpdating());
	EXPECT_FALSE(Has("a"));
	EXPECT_FALSE(state.IsGroupExpanded("G2"));
	EXPECT_TRUE(state.Tick(registry, Reader(), nullptr, &otherSel));
	EXPECT_EQ(1u, state.ShownLayerCount());
	EXPECT_TRUE(Has("e"));
	EXPECT_FALSE(Has("a"));
	EXPECT_EQ(1u, pe.readCalls);
}

TEST_F(SelectionInspectorStateTest, BaseHistogramFollowsBase)
{
	Settle();
	EXPECT_EQ(nullptr, state.BaseHistogram());	// stub layers carry no schema range
	EXPECT_TRUE(state.StatsFor(StringCRC("a")) != nullptr);
}

// ---- Pin / compare (Feature 4, Task 4) ----

TEST_F(SelectionInspectorStateTest, PinStoresCellSetAndIsLiveByDefault)
{
	Settle();
	EXPECT_FALSE(state.IsPinned());
	ASSERT_TRUE(state.Pin(registry, &grid, &sel));
	EXPECT_TRUE(state.IsPinned());
	EXPECT_FALSE(state.IsFrozen());
	EXPECT_EQ(2u, state.PinnedCount());
	sel.Clear();
	sel.Set(5, true);
	sel.Set(6, true);
	sel.Set(7, true);
	Pump();
	ASSERT_TRUE(state.StatsForA(StringCRC("a")) != nullptr);
	EXPECT_EQ(2u, state.StatsForA(StringCRC("a"))->count);	// A keeps its own cell set
	EXPECT_DOUBLE_EQ(2.5, state.StatsForA(StringCRC("a"))->mean);
	EXPECT_EQ(3u, state.StatsFor(StringCRC("a"))->count);	// B is the new selection
	EXPECT_EQ(2u, state.PinnedCount());
}

TEST_F(SelectionInspectorStateTest, PinningMarksUpdatingAndOnePassComputesA)
{
	Settle();
	ASSERT_TRUE(state.Pin(registry, &grid, &sel));
	EXPECT_TRUE(state.IsUpdating());
	EXPECT_EQ(nullptr, state.StatsForA(StringCRC("a")));
	EXPECT_TRUE(Tick());
	EXPECT_FALSE(state.IsUpdating());
	EXPECT_TRUE(state.StatsForA(StringCRC("a")) != nullptr);
	EXPECT_TRUE(state.StatsForA(StringCRC("b")) != nullptr);
	EXPECT_EQ(nullptr, state.StatsForA(StringCRC("c")));	// not shown
	EXPECT_FALSE(Tick());
}

TEST_F(SelectionInspectorStateTest, LiveARecomputesWhenLayerChangesFrozenADoesNot)
{
	Settle();
	ASSERT_TRUE(state.Pin(registry, &grid, &sel));
	Settle();
	pa.values[1] = 100.0f;
	pa.Bump();
	Pump();
	EXPECT_FLOAT_EQ(100.0f, state.StatsForA(StringCRC("a"))->max);
	EXPECT_FLOAT_EQ(100.0f, state.StatsFor(StringCRC("a"))->max);

	ASSERT_TRUE(state.PinFreeze(registry, Reader(), nullptr, &grid, &sel));
	EXPECT_TRUE(state.IsFrozen());
	pa.values[2] = 500.0f;
	pa.Bump();
	Pump();
	EXPECT_FLOAT_EQ(100.0f, state.StatsForA(StringCRC("a"))->max);	// frozen: unchanged
	EXPECT_FLOAT_EQ(500.0f, state.StatsFor(StringCRC("a"))->max);	// B follows the layer
}

TEST_F(SelectionInspectorStateTest, ADoesNotRecomputeWhenOnlySelectionChanges)
{
	Settle();
	ASSERT_TRUE(state.Pin(registry, &grid, &sel));
	Settle();
	const uint32_t readsA = pa.readCalls;
	sel.Set(9, true);
	Pump();
	EXPECT_EQ(2u, state.StatsForA(StringCRC("a"))->count);
	EXPECT_EQ(3u, state.StatsFor(StringCRC("a"))->count);
	EXPECT_EQ(readsA, pa.readCalls);	// layer version unchanged: reader cache serves both
}

TEST_F(SelectionInspectorStateTest, PinFreezeComputesEveryRegisteredLayerOfGridOnce)
{
	Settle();
	ASSERT_TRUE(state.PinFreeze(registry, Reader(), nullptr, &grid, &sel));
	EXPECT_TRUE(state.IsFrozen());
	EXPECT_EQ(4u, state.FrozenLayerCount());
	EXPECT_EQ(1u, pa.readCalls);
	EXPECT_EQ(1u, pb.readCalls);
	EXPECT_EQ(1u, pc.readCalls);
	EXPECT_EQ(1u, pd.readCalls);
	EXPECT_EQ(0u, pe.readCalls);	// other grid
	for (const char* id : { "a", "b", "c", "d" })
	{
		ASSERT_TRUE(state.StatsForA(StringCRC(id)) != nullptr) << id;	// available before any Tick, including unshown layers
		EXPECT_TRUE(state.FrozenAvailable(StringCRC(id))) << id;
		EXPECT_EQ(2u, state.StatsForA(StringCRC(id))->count) << id;
	}
	EXPECT_FALSE(state.FrozenAvailable(StringCRC("e")));
	Settle();
	Tick();
	Tick();
	EXPECT_EQ(1u, pc.readCalls);
	EXPECT_EQ(1u, pd.readCalls);
}

TEST_F(SelectionInspectorStateTest, LayerRegisteredAfterFreezeIsNotFrozen)
{
	Settle();
	ASSERT_TRUE(state.PinFreeze(registry, Reader(), nullptr, &grid, &sel));
	CountingLayerProvider pf;
	pf.values = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
	AddLayer("f", &grid, pf, "G1");
	state.SetGroupExpanded("G1", false);
	state.SetGroupExpanded("G1", true);	// the shown set is rebuilt on an expand change
	Pump();
	EXPECT_TRUE(Has("f"));
	EXPECT_FALSE(state.FrozenAvailable(StringCRC("f")));
	EXPECT_EQ(nullptr, state.StatsForA(StringCRC("f")));
	EXPECT_TRUE(state.FrozenAvailable(StringCRC("a")));
	EXPECT_EQ(4u, state.FrozenLayerCount());
}

TEST_F(SelectionInspectorStateTest, PinEmptyOrNullSelectionFailsWithNoChange)
{
	Settle();
	EXPECT_FALSE(state.Pin(registry, &grid, nullptr));
	EXPECT_FALSE(state.PinFreeze(registry, Reader(), nullptr, &grid, nullptr));
	CellSelection empty;
	empty.Reset(&grid);
	EXPECT_FALSE(state.Pin(registry, &grid, &empty));
	EXPECT_FALSE(state.IsPinned());
	EXPECT_EQ(0u, pc.readCalls);	// a failed freeze reads nothing

	ASSERT_TRUE(state.Pin(registry, &grid, &sel));
	EXPECT_FALSE(state.Pin(registry, &grid, &empty));
	EXPECT_FALSE(state.PinFreeze(registry, Reader(), nullptr, &grid, &empty));
	EXPECT_TRUE(state.IsPinned());
	EXPECT_FALSE(state.IsFrozen());
	EXPECT_EQ(2u, state.PinnedCount());	// previous A untouched
	EXPECT_FALSE(state.Pin(registry, &otherGrid, &sel));	// not the active grid
	EXPECT_EQ(2u, state.PinnedCount());
}

TEST_F(SelectionInspectorStateTest, UnpinClearsA)
{
	Settle();
	ASSERT_TRUE(state.PinFreeze(registry, Reader(), nullptr, &grid, &sel));
	state.Unpin();
	EXPECT_FALSE(state.IsPinned());
	EXPECT_FALSE(state.IsFrozen());
	EXPECT_EQ(0u, state.PinnedCount());
	EXPECT_EQ(0u, state.FrozenLayerCount());
	EXPECT_EQ(nullptr, state.StatsForA(StringCRC("a")));
	EXPECT_FALSE(state.FrozenAvailable(StringCRC("a")));
	state.Unpin();	// idempotent
	EXPECT_FALSE(state.IsPinned());
}

TEST_F(SelectionInspectorStateTest, RePinReplacesA)
{
	Settle();
	ASSERT_TRUE(state.PinFreeze(registry, Reader(), nullptr, &grid, &sel));
	sel.Clear();
	sel.Set(5, true);
	Pump();
	ASSERT_TRUE(state.Pin(registry, &grid, &sel));
	EXPECT_EQ(1u, state.PinnedCount());
	EXPECT_FALSE(state.IsFrozen());	// plain pin replaces the frozen A with a live one
	Settle();
	EXPECT_EQ(1u, state.StatsForA(StringCRC("a"))->count);
	EXPECT_DOUBLE_EQ(6.0, state.StatsForA(StringCRC("a"))->mean);
}

TEST_F(SelectionInspectorStateTest, AClearedWhenActiveGridChanges)
{
	Settle();
	ASSERT_TRUE(state.Pin(registry, &grid, &sel));
	state.SetActive(registry, &otherGrid, StringCRC("e"));
	EXPECT_FALSE(state.IsPinned());
	EXPECT_EQ(0u, state.PinnedCount());
}

TEST_F(SelectionInspectorStateTest, AClearedWhenGridSizeChanges)
{
	Settle();
	ASSERT_TRUE(state.PinFreeze(registry, Reader(), nullptr, &grid, &sel));
	grid.width = 5;
	Tick();
	EXPECT_FALSE(state.IsPinned());
	EXPECT_EQ(0u, state.FrozenLayerCount());
}

TEST_F(SelectionInspectorStateTest, AClearedWhenGridHeightOrTopologyIdChanges)
{
	Settle();
	ASSERT_TRUE(state.Pin(registry, &grid, &sel));
	grid.topology = StringCRC("other.topology");
	Tick();
	EXPECT_FALSE(state.IsPinned());
	ASSERT_TRUE(state.Pin(registry, &grid, &sel));
	grid.height = 7;
	Tick();
	EXPECT_FALSE(state.IsPinned());
}

TEST_F(SelectionInspectorStateTest, AClearedEvenWhenSelectionIsEmpty)
{
	Settle();
	ASSERT_TRUE(state.Pin(registry, &grid, &sel));
	sel.Clear();
	grid.width = 5;
	Tick();
	EXPECT_FALSE(state.IsPinned());
}

TEST_F(SelectionInspectorStateTest, DeltaIsBMinusA)
{
	LayerStats a, b;
	a.hasData = true; a.count = 4; a.hasRange = true; a.min = 1.0f; a.max = 9.0f; a.mean = 5.0;
	b.hasData = true; b.count = 10; b.hasRange = true; b.min = 0.5f; b.max = 12.0f; b.mean = 6.5;
	LayerDelta d;
	ComputeDelta(b, a, d);
	EXPECT_EQ(6, d.countDelta);
	ASSERT_TRUE(d.hasRange);
	EXPECT_DOUBLE_EQ(-0.5, d.minDelta);
	EXPECT_DOUBLE_EQ(1.5, d.meanDelta);
	EXPECT_DOUBLE_EQ(3.0, d.maxDelta);
	ComputeDelta(a, b, d);
	EXPECT_EQ(-6, d.countDelta);
	EXPECT_DOUBLE_EQ(0.5, d.minDelta);
}

TEST_F(SelectionInspectorStateTest, DeltaWithOneSideWithoutRange)
{
	LayerStats a, b;
	a.hasData = true; a.count = 0; a.hasRange = false;
	b.hasData = true; b.count = 3; b.hasRange = true; b.min = 1.0f; b.max = 2.0f; b.mean = 1.5;
	LayerDelta d;
	d.minDelta = 99.0;
	ComputeDelta(b, a, d);
	EXPECT_EQ(3, d.countDelta);
	EXPECT_FALSE(d.hasRange);
	ComputeDelta(a, b, d);
	EXPECT_EQ(-3, d.countDelta);
	EXPECT_FALSE(d.hasRange);
	ComputeDelta(a, a, d);
	EXPECT_EQ(0, d.countDelta);
	EXPECT_FALSE(d.hasRange);
}

TEST_F(SelectionInspectorStateTest, DeltaFromLiveStats)
{
	Settle();
	ASSERT_TRUE(state.Pin(registry, &grid, &sel));	// A = {1,2} -> values 2,3
	sel.Clear();
	sel.Set(5, true);	// B = {5} -> 6
	Settle();
	LayerDelta d;
	ComputeDelta(*state.StatsFor(StringCRC("a")), *state.StatsForA(StringCRC("a")), d);
	EXPECT_EQ(-1, d.countDelta);
	ASSERT_TRUE(d.hasRange);
	EXPECT_DOUBLE_EQ(4.0, d.minDelta);		// 6 - 2
	EXPECT_DOUBLE_EQ(3.5, d.meanDelta);		// 6 - 2.5
	EXPECT_DOUBLE_EQ(3.0, d.maxDelta);		// 6 - 3
}
