#include <gtest/gtest.h>
#include <DiaCellLayers/CellSelection.h>
#include <DiaCellLayers/LayerReader.h>
#include <DiaCellLayers/SelectionOutline.h>
#include <DiaCellLayers/SelectionTools.h>
#include <DiaCellLayers/Testing/LayerReaderTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>
#include <DiaCellLayers/Testing/ViewValueSourceTestSupport.h>

#include <chrono>
#include <stdio.h>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
namespace Tools = Dia::CellLayers::SelectionTools;

// Guards against pathological behaviour only (AC 14): bounds are generous, measured times are printed.
namespace
{
	const uint16_t kW = 104;
	const uint16_t kH = 64;
	const uint32_t kCells = uint32_t(kW) * kH;	// 6,656
	const double kBoundMs = 8.0;

	double MsSince(std::chrono::steady_clock::time_point t0)
	{
		return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
	}
}

TEST(SelectionPerfTest, SelectBoxWholeGridThenOutline)
{
	auto grid = MakeGrid(kW, kH, 10.0f);
	CellSelection sel;
	sel.Reset(&grid);
	SelectionOutline outline;

	auto t0 = std::chrono::steady_clock::now();
	const auto res = Tools::SelectBox(sel, kW - 1, kH - 1, 0, 0);
	const double boxMs = MsSince(t0);

	t0 = std::chrono::steady_clock::now();
	outline.Update(sel);
	const double outlineMs = MsSince(t0);

	printf("[ perf ] SelectBox 104x64: %.3f ms (%u cells); outline rebuild: %.3f ms (%u edges)\n",
		boxMs, res.count, outlineMs, outline.EdgeCount());
	EXPECT_EQ(kCells, sel.Count());
	EXPECT_FALSE(outline.TooLarge());
	EXPECT_LT(boxMs, kBoundMs);
	EXPECT_LT(outlineMs, kBoundMs);
}

TEST(SelectionPerfTest, SelectCategoryWholeGridThenOutline)
{
	// Every cell holds value 0 ("grass"), so the category selection covers the whole grid.
	Dia::ValueSchema::ValueInfo terrain = Dia::ValueSchema::Testing::MakeValueInfo("cells", "terrain");
	terrain.mType = Dia::ValueSchema::ValueType::Enum;
	terrain.mEnumValues.Reserve(2);
	for (const char* id : { "grass", "water" })
	{
		Dia::ValueSchema::EnumValue v;
		Dia::ValueSchema::SetText(v.mId, id);
		terrain.mEnumValues.Add(v);
	}
	UploaderSchemaFixture fx;
	fx.Add(terrain);
	const Dia::ValueSchema::ValueSchemaService& service = fx.Publish();

	FakePuId() = 1;
	CellLayerRegistry registry{ nullptr, &FakePuIdFn };
	CountingLayerProvider provider;
	provider.values.assign(kCells, 0.0f);
	auto grid = MakeGrid(kW, kH, 10.0f);
	CellLayerDesc desc = MakeCountingLayer("terrain", &grid, provider, CellValueKind::Category);
	desc.schemaKey = StringCRC("cells/terrain");
	ASSERT_EQ(CellRegisterResult::Ok, registry.Register(desc));
	LayerReader reader(registry);

	CellSelection sel;
	sel.Reset(&grid);
	SelectionOutline outline;

	auto t0 = std::chrono::steady_clock::now();
	const auto res = Tools::SelectCategory(sel, reader, registry, &service, StringCRC("terrain"), "grass");
	const double catMs = MsSince(t0);

	t0 = std::chrono::steady_clock::now();
	outline.Update(sel);
	const double outlineMs = MsSince(t0);

	printf("[ perf ] SelectCategory 104x64: %.3f ms (%u cells); outline rebuild: %.3f ms (%u edges)\n",
		catMs, res.count, outlineMs, outline.EdgeCount());
	ASSERT_TRUE(res.ok);
	EXPECT_EQ(kCells, sel.Count());
	EXPECT_LT(catMs, kBoundMs);
	EXPECT_LT(outlineMs, kBoundMs);
}
