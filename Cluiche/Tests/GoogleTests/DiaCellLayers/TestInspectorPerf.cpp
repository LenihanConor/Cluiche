#include <gtest/gtest.h>
#include <DiaCellLayers/CellSelection.h>
#include <DiaCellLayers/LayerReader.h>
#include <DiaCellLayers/SelectionStats.h>
#include <DiaCellLayers/SelectionTools.h>
#include <DiaCellLayers/Testing/LayerReaderTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>

#include <chrono>
#include <stdio.h>
#include <string>
#include <vector>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;

// Selection Inspector AC 13: aggregating all cells of a Large grid (104x64) over 20 layers takes at most 2 ms in Release.
// The bound is enforced in Release builds only (NDEBUG); Debug builds print the measured time and assert correctness.
namespace
{
	const uint16_t kW = 104;
	const uint16_t kH = 64;
	const uint32_t kCells = uint32_t(kW) * kH;
	const uint32_t kLayers = 20;
	const double kBoundMs = 2.0;
}

TEST(SelectionInspectorPerfTest, AggregateAllCellsOverTwentyLayers)
{
	UploaderSchemaFixture fx;
	std::vector<std::string> ids;
	for (uint32_t i = 0; i < kLayers; ++i) { ids.push_back("layer" + std::to_string(i)); }
	for (const std::string& id : ids) { fx.Add(UploaderSchemaFixture::FloatEntry(id.c_str(), 0, 1000)); }
	const Dia::ValueSchema::ValueSchemaService& service = fx.Publish();

	FakePuId() = 1;
	CellLayerRegistry registry{ nullptr, &FakePuIdFn };
	auto grid = MakeGrid(kW, kH, 10.0f);
	std::vector<CountingLayerProvider> providers(kLayers);
	for (uint32_t i = 0; i < kLayers; ++i)
	{
		providers[i].values.resize(kCells);
		for (uint32_t c = 0; c < kCells; ++c) { providers[i].values[c] = static_cast<float>((c + i) % 1000); }
		CellLayerDesc d = MakeCountingLayer(ids[i].c_str(), &grid, providers[i]);
		d.schemaKey = StringCRC(("cells/" + ids[i]).c_str());
		ASSERT_EQ(CellRegisterResult::Ok, registry.Register(d));
	}
	LayerReader reader(registry);

	CellSelection sel;
	sel.Reset(&grid);
	ASSERT_TRUE(SelectionTools::SelectBox(sel, 0, 0, kW - 1, kH - 1).ok);
	ASSERT_EQ(kCells, sel.Count());

	LayerStats stats;
	uint32_t counted = 0;
	const auto t0 = std::chrono::steady_clock::now();
	for (uint32_t i = 0; i < kLayers; ++i)
	{
		ASSERT_TRUE(SelectionStats::Compute(registry, reader, &service, sel, StringCRC(ids[i].c_str()), stats));
		counted += stats.count;
	}
	const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();

	printf("[ perf ] aggregate %u layers over all %u cells (104x64): %.3f ms\n", kLayers, kCells, ms);
	EXPECT_EQ(kCells * kLayers, counted);
#ifdef NDEBUG
	EXPECT_LE(ms, kBoundMs);
#else
	(void)kBoundMs;
#endif
}
