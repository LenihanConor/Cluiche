#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <string>
#include <DiaCellLayers/CellSelection.h>
#include <DiaCellLayers/InspectorModel.h>
#include <DiaCellLayers/LayerReader.h>
#include <DiaCellLayers/Testing/CellDetailTestSupport.h>
#include <DiaCellLayers/Testing/LayerReaderTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>
#include <DiaCellLayers/Testing/ViewValueSourceTestSupport.h>
#include <DiaValueSchema/ValueInfo.h>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;

namespace
{
	struct SelectionInspectorModelTest : ::testing::Test
	{
		UploaderSchemaFixture fx;
		const Dia::ValueSchema::ValueSchemaService* service = nullptr;
		Dia::Graphics::CellGridDesc grid = MakeGrid(4, 3);
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };
		CountingLayerProvider pTemp, pBiome, pRain, pHidden;
		CellSelection sel;
		InspectorState state;
		SingleCellTable table;
		StubDetailProvider detail;
		std::unique_ptr<LayerReader> mReader;

		void AddLayer(const char* id, CountingLayerProvider& p, const char* group, CellValueKind kind = CellValueKind::Float)
		{
			CellLayerDesc d = MakeCountingLayer(id, &grid, p, kind);
			d.group.Clear();
			d.group.Append(group);
			d.schemaKey = StringCRC((std::string("cells/") + id).c_str());
			ASSERT_EQ(CellRegisterResult::Ok, registry.Register(d));
		}

		void SetUp() override
		{
			FakePuId() = 1;
			auto temp = UploaderSchemaFixture::FloatEntry("temp", -50, 50);
			Dia::ValueSchema::SetText(temp.mLabel, "Temperature");
			Dia::ValueSchema::SetText(temp.mUnit, "C");
			fx.Add(temp);
			auto biome = UploaderSchemaFixture::EnumEntry("biome", 3);
			Dia::ValueSchema::SetText(biome.mLabel, "Biome");
			Dia::ValueSchema::SetText(biome.mEnumValues[0].mLabel, "Forest");
			Dia::ValueSchema::SetText(biome.mEnumValues[1].mLabel, "Desert");
			Dia::ValueSchema::SetText(biome.mEnumValues[2].mLabel, "Tundra");
			fx.Add(biome);
			service = &fx.Publish();

			pTemp.values = { 0, 0.1f, 1e-7f, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			pBiome.values = { 0, 2, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			pRain.values = { 0, 3.25f, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			pHidden.values = { 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 };
			AddLayer("temp", pTemp, "Climate");
			AddLayer("biome", pBiome, "Climate", CellValueKind::Category);
			AddLayer("rain", pRain, "Climate");
			AddLayer("hidden", pHidden, "Other");
			sel.Reset(&grid);
			state.SetActive(registry, &grid, StringCRC("temp"));
			registry.Details().Register(&grid, "Ecology", detail.Delegate());
			detail.rowsToEmit = 1;
		}

		LayerReader& Reader()
		{
			if (!mReader) { mReader = std::make_unique<LayerReader>(registry); }
			return *mReader;
		}
		void Settle() { for (int i = 0; i < 4; ++i) { state.Tick(registry, Reader(), service, &sel); } }
		bool Build(bool open = true) { return InspectorModel::BuildSingleCell(registry, Reader(), service, state, &sel, open, table); }
		const SingleCellRow* Find(const char* id)
		{
			for (uint32_t i = 0; i < table.RowCount(); ++i) { if (table.Row(i).layerId == StringCRC(id)) { return &table.Row(i); } }
			return nullptr;
		}
		uint32_t Reads() const { return pTemp.readCalls + pBiome.readCalls + pRain.readCalls + pHidden.readCalls; }
	};
}

TEST_F(SelectionInspectorModelTest, ExactFloatValuesForShownLayers)
{
	sel.Set(1, true);
	Settle();
	ASSERT_TRUE(Build());
	EXPECT_EQ(3u, table.RowCount());
	EXPECT_EQ(StringCRC("temp"), table.Row(0).layerId);	// active base first
	const SingleCellRow* t = Find("temp");
	ASSERT_NE(nullptr, t);
	EXPECT_TRUE(t->hasData);
	EXPECT_FALSE(t->isCategory);
	EXPECT_EQ(0.1f, t->value);	// bit-exact, no rounding
	ASSERT_NE(nullptr, Find("rain"));
	EXPECT_EQ(3.25f, Find("rain")->value);
}

TEST_F(SelectionInspectorModelTest, TinyValueNotRounded)
{
	sel.Set(2, true);
	Settle();
	ASSERT_TRUE(Build());
	ASSERT_NE(nullptr, Find("temp"));
	EXPECT_EQ(1e-7f, Find("temp")->value);
}

TEST_F(SelectionInspectorModelTest, UnitAndLabelFromSchema)
{
	sel.Set(1, true);
	Settle();
	ASSERT_TRUE(Build());
	ASSERT_NE(nullptr, Find("temp"));
	EXPECT_STREQ("Temperature", Find("temp")->label.AsCStr());
	EXPECT_STREQ("C", Find("temp")->unit.AsCStr());
}

TEST_F(SelectionInspectorModelTest, CategoryGivesEnumIdAndLabel)
{
	sel.Set(1, true);
	Settle();
	ASSERT_TRUE(Build());
	const SingleCellRow* b = Find("biome");
	ASSERT_NE(nullptr, b);
	EXPECT_TRUE(b->isCategory);
	EXPECT_FALSE(b->otherValue);
	EXPECT_EQ(2.0f, b->value);
	EXPECT_STREQ("v2", b->enumId.AsCStr());
	EXPECT_STREQ("Tundra", b->enumLabel.AsCStr());
}

TEST_F(SelectionInspectorModelTest, CategoryOutsideEnumIsOther)
{
	sel.Set(2, true);	// biome value 7, enum has 3 values
	Settle();
	ASSERT_TRUE(Build());
	const SingleCellRow* b = Find("biome");
	ASSERT_NE(nullptr, b);
	EXPECT_TRUE(b->otherValue);
	EXPECT_STREQ("", b->enumId.AsCStr());
	EXPECT_STREQ("", b->enumLabel.AsCStr());
	EXPECT_EQ(7.0f, b->value);
}

TEST_F(SelectionInspectorModelTest, NaNValueFlagged)
{
	pTemp.values[1] = std::numeric_limits<float>::quiet_NaN();
	sel.Set(1, true);
	Settle();
	ASSERT_TRUE(Build());
	ASSERT_NE(nullptr, Find("temp"));
	EXPECT_TRUE(Find("temp")->isNaN);
	EXPECT_TRUE(Find("temp")->hasData);
	EXPECT_FALSE(Find("rain")->isNaN);
}

TEST_F(SelectionInspectorModelTest, LayerWithNoDataFlagged)
{
	StubViewValueSource src;
	src.live = false;
	src.hasData = false;
	ASSERT_TRUE(registry.SetViewValueSource(&src));
	sel.Set(1, true);
	Settle();
	ASSERT_TRUE(Build());
	ASSERT_NE(nullptr, Find("temp"));
	EXPECT_FALSE(Find("temp")->hasData);
	EXPECT_FALSE(Find("temp")->isNaN);
	registry.ClearViewValueSource();
}

TEST_F(SelectionInspectorModelTest, CollapsedGroupLayersAbsent)
{
	sel.Set(1, true);
	Settle();
	ASSERT_TRUE(Build());
	EXPECT_EQ(nullptr, Find("hidden"));
	EXPECT_EQ(0u, pHidden.readCalls);
	state.SetGroupExpanded("Other", true);
	Settle();
	ASSERT_TRUE(Build());
	ASSERT_NE(nullptr, Find("hidden"));
	EXPECT_EQ(9.0f, Find("hidden")->value);
}

TEST_F(SelectionInspectorModelTest, ProviderDetailsAppendedAndCalledOnce)
{
	StubDetailProvider second; second.rowsToEmit = 1;
	registry.Details().Register(&grid, "Second", second.Delegate());
	sel.Set(5, true);
	Settle();
	ASSERT_TRUE(Build());
	EXPECT_EQ(1u, detail.calls);
	EXPECT_EQ(1u, second.calls);
	EXPECT_EQ(5u, detail.lastCell);
	ASSERT_EQ(4u, table.details.Count());
	EXPECT_STREQ("Ecology", table.details.At(0).label.AsCStr());
	EXPECT_STREQ("Second", table.details.At(2).label.AsCStr());
}

TEST_F(SelectionInspectorModelTest, CoordinatesFromGridWidth)
{
	sel.Set(6, true);	// width 4: r=1, q=2
	Settle();
	ASSERT_TRUE(Build());
	EXPECT_EQ(6u, table.cellIndex);
	EXPECT_EQ(2u, table.q);
	EXPECT_EQ(1u, table.r);
}

TEST_F(SelectionInspectorModelTest, ZeroCellsReturnsFalseNoCallsNoReads)
{
	Settle();
	const uint32_t reads = Reads();
	EXPECT_FALSE(Build());
	EXPECT_EQ(0u, table.RowCount());
	EXPECT_EQ(0u, detail.calls);
	EXPECT_EQ(reads, Reads());
}

TEST_F(SelectionInspectorModelTest, TwoCellsReturnsFalseAndClearsPreviousTable)
{
	sel.Set(1, true);
	Settle();
	ASSERT_TRUE(Build());
	ASSERT_GT(table.RowCount(), 0u);
	sel.Set(2, true);
	Settle();
	const uint32_t reads = Reads();
	const uint32_t calls = detail.calls;
	EXPECT_FALSE(Build());
	EXPECT_EQ(0u, table.RowCount());
	EXPECT_EQ(0u, table.details.Count());
	EXPECT_EQ(calls, detail.calls);
	EXPECT_EQ(reads, Reads());
}

TEST_F(SelectionInspectorModelTest, CardClosedReturnsFalseNoCallsNoReads)
{
	sel.Set(1, true);
	Settle();
	const uint32_t reads = Reads();
	EXPECT_FALSE(Build(false));
	EXPECT_EQ(0u, table.RowCount());
	EXPECT_EQ(0u, detail.calls);
	EXPECT_EQ(reads, Reads());
}

TEST_F(SelectionInspectorModelTest, RepeatedBuildReadsEachLayerOncePerVersion)
{
	sel.Set(1, true);
	Settle();
	ASSERT_TRUE(Build());
	const uint32_t reads = Reads();
	ASSERT_TRUE(Build());
	EXPECT_EQ(reads, Reads());
}
