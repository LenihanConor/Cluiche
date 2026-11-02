#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <stdio.h>
#include <string>
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

TEST_F(SelectionInspectorStateTest, GroupNameOver31CharsExpandsAndCollapsesLikeAShortOne)
{
	// Stored names truncate to String32; lookups must truncate the same way (CellDetailRegistry had the same defect).
	const std::string longName(40, 'g');
	state.SetGroupExpanded(longName.c_str(), true);
	EXPECT_TRUE(state.IsGroupExpanded(longName.c_str()));
	state.SetGroupExpanded(longName.c_str(), true);	// idempotent: no duplicate entry
	state.SetGroupExpanded(longName.c_str(), false);
	EXPECT_FALSE(state.IsGroupExpanded(longName.c_str()));
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

// ---- Task 10a exhaustive pass (Feature 4): shown-set rules, registry changes, caps, pin edges ----

TEST_F(SelectionInspectorStateTest, LayerRegisteredLaterInExpandedGroupJoinsShownSet)
{
	Settle();
	ASSERT_EQ(2u, state.ShownLayerCount());
	CountingLayerProvider pf;
	pf.values = { 10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120 };
	AddLayer("f", &grid, pf, "G1");	// the active group is expanded, so the new layer must be shown
	Pump();
	EXPECT_EQ(3u, state.ShownLayerCount());
	ASSERT_TRUE(Has("f"));
	EXPECT_DOUBLE_EQ(25.0, state.StatsFor(StringCRC("f"))->mean);	// cells 1, 2 -> 20, 30
	EXPECT_EQ(1u, pf.readCalls);
	EXPECT_EQ(1u, pa.readCalls);	// the layers already computed are not re-read
}

TEST_F(SelectionInspectorStateTest, LayerRegisteredLaterInCollapsedGroupStaysHidden)
{
	Settle();
	CountingLayerProvider pf;
	pf.values = { 1, 2, 3 };
	AddLayer("f", &grid, pf, "G3");
	Pump();
	EXPECT_EQ(2u, state.ShownLayerCount());
	EXPECT_FALSE(Has("f"));
	EXPECT_EQ(0u, pf.readCalls);
	state.SetGroupExpanded("G3", true);	// expanding it later picks it up
	Pump();
	EXPECT_TRUE(Has("f"));
}

TEST_F(SelectionInspectorStateTest, LayerRegisteredOnAnotherGridNeverJoins)
{
	Settle();
	CountingLayerProvider pf;
	pf.values = { 1, 2, 3 };
	AddLayer("f", &otherGrid, pf, "G1");
	Pump();
	EXPECT_EQ(2u, state.ShownLayerCount());
	EXPECT_EQ(0u, pf.readCalls);
}

TEST_F(SelectionInspectorStateTest, UnregisteredShownLayerLeavesTheShownSet)
{
	Settle();
	ASSERT_TRUE(Has("b"));
	ASSERT_TRUE(registry.Unregister(StringCRC("b")));
	Pump();
	EXPECT_EQ(1u, state.ShownLayerCount());
	EXPECT_FALSE(Has("b"));
	EXPECT_TRUE(Has("a"));
}

TEST_F(SelectionInspectorStateTest, UnregisteredBaseLeavesNoDanglingBase)
{
	Settle();
	ASSERT_TRUE(registry.Unregister(StringCRC("a")));
	Pump();	// must not crash or read the removed layer
	EXPECT_FALSE(Has("a"));
	EXPECT_EQ(StringCRC("b"), state.ShownLayerAt(0));	// the group is still expanded, so b stays shown
	EXPECT_EQ(nullptr, state.BaseHistogram());
}

TEST_F(SelectionInspectorStateTest, BaseStaysShownWhenItsGroupIsCollapsed)
{
	Settle();
	state.SetGroupExpanded("G1", false);
	Pump();
	EXPECT_EQ(1u, state.ShownLayerCount());
	EXPECT_EQ(StringCRC("a"), state.ShownLayerAt(0));
	EXPECT_TRUE(Has("a"));
	EXPECT_FALSE(Has("b"));
}

TEST_F(SelectionInspectorStateTest, BaseChangeInsideSameGroupKeepsTheExpandedSet)
{
	state.SetGroupExpanded("G2", true);
	Settle();
	state.SetActive(registry, &grid, StringCRC("b"));	// same group G1
	Pump();
	EXPECT_TRUE(state.IsGroupExpanded("G1"));
	EXPECT_TRUE(state.IsGroupExpanded("G2"));
	EXPECT_EQ(StringCRC("b"), state.ShownLayerAt(0));
	EXPECT_EQ(4u, state.ShownLayerCount());
}

TEST_F(SelectionInspectorStateTest, BaseChangeToAnotherGroupResetsTheExpandedSet)
{
	state.SetGroupExpanded("G2", true);
	Settle();
	state.SetActive(registry, &grid, StringCRC("c"));	// group G2
	Pump();
	EXPECT_EQ(1u, state.ExpandedGroupCount());
	EXPECT_TRUE(state.IsGroupExpanded("G2"));
	EXPECT_FALSE(state.IsGroupExpanded("G1"));
	EXPECT_EQ(StringCRC("c"), state.ShownLayerAt(0));
	EXPECT_EQ(2u, state.ShownLayerCount());	// c, d
	EXPECT_FALSE(Has("a"));
}

TEST_F(SelectionInspectorStateTest, ExpandedGroupCapIgnoresFurtherGroups)
{
	Settle();
	char name[16];
	for (uint32_t i = 0; i < InspectorState::kMaxGroups + 4; ++i)
	{
		snprintf(name, sizeof(name), "X%u", i);
		state.SetGroupExpanded(name, true);
	}
	EXPECT_EQ(InspectorState::kMaxGroups, state.ExpandedGroupCount());
	EXPECT_TRUE(state.IsGroupExpanded("G1"));
	EXPECT_TRUE(state.IsGroupExpanded("X14"));	// G1 took slot 0, so X0..X14 fit
	EXPECT_FALSE(state.IsGroupExpanded("X15"));
}

TEST_F(SelectionInspectorStateTest, NullOrRedundantExpandCallsDoNotMarkPending)
{
	Settle();
	ASSERT_FALSE(state.IsUpdating());
	state.SetGroupExpanded(nullptr, true);
	state.SetGroupExpanded("G1", true);		// already expanded
	state.SetGroupExpanded("Nope", false);	// never expanded
	EXPECT_FALSE(state.IsUpdating());
	EXPECT_FALSE(state.IsGroupExpanded(nullptr));
	EXPECT_STREQ("", state.ExpandedGroupAt(99));
}

TEST_F(SelectionInspectorStateTest, ShownSetIsCappedAtMaxShownBaseFirstThenRegistrationOrder)
{
	CountingLayerProvider many;
	many.values = { 1, 2, 3 };
	char name[16];
	for (uint32_t i = 0; i < InspectorState::kMaxShown + 8; ++i)
	{
		snprintf(name, sizeof(name), "m%u", i);
		AddLayer(name, &grid, many, "G1");
	}
	Settle();
	EXPECT_EQ(InspectorState::kMaxShown, state.ShownLayerCount());
	EXPECT_EQ(StringCRC("a"), state.ShownLayerAt(0));
	EXPECT_EQ(StringCRC("b"), state.ShownLayerAt(1));
	EXPECT_EQ(StringCRC("m0"), state.ShownLayerAt(2));
	EXPECT_EQ(StringCRC("m29"), state.ShownLayerAt(InspectorState::kMaxShown - 1));	// a, b + m0..m29 = 32
	EXPECT_EQ(StringCRC::kZero, state.ShownLayerAt(InspectorState::kMaxShown));
	EXPECT_TRUE(Has("m29"));
	EXPECT_FALSE(Has("m30"));
}

TEST_F(SelectionInspectorStateTest, PinForANonActiveGridFailsWithNoChange)
{
	Settle();
	EXPECT_FALSE(state.Pin(registry, &otherGrid, &sel));
	EXPECT_FALSE(state.IsPinned());
	EXPECT_FALSE(state.Pin(registry, nullptr, &sel));
	EXPECT_FALSE(state.IsPinned());
	EXPECT_FALSE(state.PinFreeze(registry, Reader(), nullptr, &otherGrid, &sel));
	EXPECT_FALSE(state.IsFrozen());
	EXPECT_EQ(0u, state.FrozenLayerCount());
}

TEST_F(SelectionInspectorStateTest, FailedPinKeepsAnExistingA)
{
	Settle();
	ASSERT_TRUE(state.Pin(registry, &grid, &sel));
	CellSelection empty;
	empty.Reset(&grid);
	EXPECT_FALSE(state.Pin(registry, &grid, &empty));
	EXPECT_FALSE(state.Pin(registry, &grid, nullptr));
	EXPECT_TRUE(state.IsPinned());
	EXPECT_EQ(2u, state.PinnedCount());
}

TEST_F(SelectionInspectorStateTest, PinCopiesTheSetSoLaterSelectionChangesDoNotMoveA)
{
	Settle();
	ASSERT_TRUE(state.Pin(registry, &grid, &sel));
	sel.Clear();
	sel.Set(7, true);
	sel.Set(8, true);
	sel.Set(9, true);
	Settle();
	EXPECT_EQ(2u, state.PinnedCount());
	ASSERT_NE(nullptr, state.StatsForA(StringCRC("a")));
	EXPECT_DOUBLE_EQ(2.5, state.StatsForA(StringCRC("a"))->mean);	// still cells 1, 2
	EXPECT_DOUBLE_EQ(9.0, state.StatsFor(StringCRC("a"))->mean);
}

TEST_F(SelectionInspectorStateTest, WholeGridSelectionCountsEveryCell)
{
	for (uint32_t i = 0; i < 12; ++i) { sel.Set(i, true); }
	Settle();
	const LayerStats* s = state.StatsFor(StringCRC("a"));
	ASSERT_NE(nullptr, s);
	EXPECT_EQ(12u, s->count);
	EXPECT_FLOAT_EQ(1.0f, s->min);
	EXPECT_FLOAT_EQ(12.0f, s->max);
	EXPECT_DOUBLE_EQ(6.5, s->mean);
	ASSERT_TRUE(state.Pin(registry, &grid, &sel));
	EXPECT_EQ(12u, state.PinnedCount());
}

TEST_F(SelectionInspectorStateTest, SingleCellSelectionHasMinEqualMeanEqualMax)
{
	sel.Clear();
	sel.Set(6, true);
	Settle();
	const LayerStats* s = state.StatsFor(StringCRC("a"));
	ASSERT_NE(nullptr, s);
	EXPECT_EQ(1u, s->count);
	EXPECT_FLOAT_EQ(7.0f, s->min);
	EXPECT_FLOAT_EQ(7.0f, s->max);
	EXPECT_DOUBLE_EQ(7.0, s->mean);
}

TEST_F(SelectionInspectorStateTest, AllNaNLayerReportsNoRangeOthersUnaffected)
{
	const float nan = std::numeric_limits<float>::quiet_NaN();
	pa.values = std::vector<float>(12, nan);
	pa.Bump();
	Settle();
	const LayerStats* s = state.StatsFor(StringCRC("a"));
	ASSERT_NE(nullptr, s);
	EXPECT_EQ(0u, s->count);
	EXPECT_EQ(2u, s->nanCount);
	EXPECT_FALSE(s->hasRange);
	EXPECT_EQ(2u, state.StatsFor(StringCRC("b"))->count);
}

TEST_F(SelectionInspectorStateTest, FreezeCoversEveryLayerOfTheGridOnly)
{
	Settle();
	ASSERT_TRUE(state.PinFreeze(registry, Reader(), nullptr, &grid, &sel));
	EXPECT_EQ(4u, state.FrozenLayerCount());	// a, b, c, d (e is on another grid)
	for (const char* id : { "a", "b", "c", "d" }) { EXPECT_TRUE(state.FrozenAvailable(StringCRC(id))) << id; }
	EXPECT_FALSE(state.FrozenAvailable(StringCRC("e")));
	EXPECT_EQ(nullptr, state.StatsForA(StringCRC("e")));
	EXPECT_EQ(nullptr, state.StatsForA(StringCRC("missing")));
	EXPECT_FALSE(state.FrozenAvailable(StringCRC("missing")));
}

TEST_F(SelectionInspectorStateTest, RePinAfterFreezeIsLiveAgain)
{
	Settle();
	ASSERT_TRUE(state.PinFreeze(registry, Reader(), nullptr, &grid, &sel));
	ASSERT_TRUE(state.IsFrozen());
	ASSERT_TRUE(state.Pin(registry, &grid, &sel));
	EXPECT_TRUE(state.IsPinned());
	EXPECT_FALSE(state.IsFrozen());
	EXPECT_EQ(0u, state.FrozenLayerCount());
	EXPECT_FALSE(state.FrozenAvailable(StringCRC("a")));
}

TEST_F(SelectionInspectorStateTest, UnpinWhenNotPinnedIsHarmlessAndUnpinDropsFrozenStats)
{
	state.Unpin();
	EXPECT_FALSE(state.IsPinned());
	Settle();
	ASSERT_TRUE(state.PinFreeze(registry, Reader(), nullptr, &grid, &sel));
	state.Unpin();
	EXPECT_FALSE(state.IsFrozen());
	EXPECT_EQ(0u, state.FrozenLayerCount());
	EXPECT_EQ(nullptr, state.StatsForA(StringCRC("a")));
	EXPECT_EQ(0u, state.PinnedCount());
}

TEST_F(SelectionInspectorStateTest, LiveAOnlyCoversTheShownSet)
{
	Settle();
	ASSERT_TRUE(state.Pin(registry, &grid, &sel));
	Settle();
	ASSERT_NE(nullptr, state.StatsForA(StringCRC("a")));
	EXPECT_TRUE(state.StatsForA(StringCRC("a"))->hasData);
	EXPECT_EQ(nullptr, state.StatsForA(StringCRC("c")));	// c is not shown, so live A has nothing for it
}
