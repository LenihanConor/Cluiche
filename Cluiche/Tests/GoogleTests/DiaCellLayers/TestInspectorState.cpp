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
