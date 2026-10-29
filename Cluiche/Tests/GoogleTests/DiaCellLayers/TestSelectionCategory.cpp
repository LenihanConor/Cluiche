#include <gtest/gtest.h>
#include <DiaCellLayers/CellSelection.h>
#include <DiaCellLayers/LayerReader.h>
#include <DiaCellLayers/SelectionTools.h>
#include <DiaCellLayers/Testing/LayerReaderTestSupport.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaCellLayers/Testing/SelectionTestSupport.h>
#include <DiaCellLayers/Testing/ViewValueSourceTestSupport.h>

#include <limits>
#include <math.h>
#include <vector>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
namespace Tools = Dia::CellLayers::SelectionTools;

namespace
{
	struct Dim { uint16_t w; uint16_t h; };
	const Dim kDims[] = { { 66, 42 }, { 104, 64 } };

	// Schema full keys are "cells/<key>"; enum value numeric = index: grass 0, water 1, rock 2, sand 3, unused 4.
	struct CategoryFixture
	{
		UploaderSchemaFixture	fx;
		const Dia::ValueSchema::ValueSchemaService*	service = nullptr;

		CategoryFixture()
		{
			Dia::ValueSchema::ValueInfo terrain = Dia::ValueSchema::Testing::MakeValueInfo("cells", "terrain");
			terrain.mType = Dia::ValueSchema::ValueType::Enum;
			terrain.mEnumValues.Reserve(5);
			for (const char* id : { "grass", "water", "rock", "sand", "unused" })
			{
				Dia::ValueSchema::EnumValue v;
				Dia::ValueSchema::SetText(v.mId, id);
				terrain.mEnumValues.Add(v);
			}
			fx.Add(terrain);
			fx.Add(UploaderSchemaFixture::FloatEntry("temp", 0, 10));
			service = &fx.Publish();
		}
	};

	CellLayerDesc Layer(const char* id, const Dia::Graphics::CellGridDesc* grid, CountingLayerProvider& p, CellValueKind kind, const char* key)
	{
		CellLayerDesc d = MakeCountingLayer(id, grid, p, kind);
		d.schemaKey = StringCRC(key);
		return d;
	}

	std::vector<float> Pattern(const Dim& d)
	{
		std::vector<float> v(static_cast<size_t>(d.w) * d.h);
		for (size_t i = 0; i < v.size(); ++i) { v[i] = static_cast<float>(i % 4); }	// grass,water,rock,sand
		return v;
	}

	std::vector<uint32_t> Expected(const std::vector<float>& v, float target)
	{
		std::vector<uint32_t> out;
		for (uint32_t i = 0; i < v.size(); ++i)
		{
			if (v[i] == v[i] && floorf(v[i] + 0.5f) == target) { out.push_back(i); }
		}
		return out;
	}

	struct SelectionCategoryTest : ::testing::Test
	{
		CategoryFixture cf;
		CellLayerRegistry registry{ nullptr, &FakePuIdFn };
		CountingLayerProvider terrain, other, temp;

		void SetUp() override { FakePuId() = 1; }
	};
}

TEST_F(SelectionCategoryTest, EnumIdSelectsHandComputedCells)
{
	for (const Dim& d : kDims)
	{
		CellLayerRegistry reg{ nullptr, &FakePuIdFn };
		auto g = MakeGrid(d.w, d.h);
		terrain.values = Pattern(d);
		reg.Register(Layer("terrain", &g, terrain, CellValueKind::Category, "cells/terrain"));
		LayerReader reader(reg);
		CellSelection s;
		s.Reset(&g);
		s.Set(0, true);
		const uint32_t v = s.Version();
		auto res = Tools::SelectCategory(s, reader, reg, cf.service, StringCRC("terrain"), "water");
		EXPECT_TRUE(res.ok);
		EXPECT_TRUE(res.changed);
		EXPECT_EQ(Tools::CategoryError::None, res.error);
		EXPECT_EQ(ToVector(s), Expected(terrain.values, 1.0f));
		EXPECT_EQ(res.count, s.Count());
		EXPECT_EQ(v + 1, s.Version());
		EXPECT_EQ(ToVector(s).front(), 1u);
	}
}

TEST_F(SelectionCategoryTest, NumberSelectsSameCellsAsEnumId)
{
	auto g = MakeGrid(66, 42);
	terrain.values = Pattern({ 66, 42 });
	registry.Register(Layer("terrain", &g, terrain, CellValueKind::Category, "cells/terrain"));
	LayerReader reader(registry);
	CellSelection s;
	s.Reset(&g);
	Tools::SelectCategory(s, reader, registry, cf.service, StringCRC("terrain"), "rock");
	const auto byId = ToVector(s);
	for (const char* num : { "2", "2.0" })
	{
		Tools::SelectCategory(s, reader, registry, cf.service, StringCRC("terrain"), "sand");	// change first
		auto res = Tools::SelectCategory(s, reader, registry, cf.service, StringCRC("terrain"), num);
		EXPECT_TRUE(res.ok);
		EXPECT_EQ(byId, ToVector(s));
	}
	EXPECT_EQ(Expected(terrain.values, 2.0f), byId);
}

TEST_F(SelectionCategoryTest, NullSchemaNumericOnly)
{
	auto g = MakeGrid(66, 42);
	terrain.values = Pattern({ 66, 42 });
	registry.Register(Layer("terrain", &g, terrain, CellValueKind::Category, "cells/terrain"));
	LayerReader reader(registry);
	CellSelection s;
	s.Reset(&g);
	auto bad = Tools::SelectCategory(s, reader, registry, nullptr, StringCRC("terrain"), "water");
	EXPECT_FALSE(bad.ok);
	EXPECT_EQ(Tools::CategoryError::UnknownValue, bad.error);
	auto good = Tools::SelectCategory(s, reader, registry, nullptr, StringCRC("terrain"), "1");
	EXPECT_TRUE(good.ok);
	EXPECT_EQ(Expected(terrain.values, 1.0f), ToVector(s));
}

TEST_F(SelectionCategoryTest, WorksOnNonBaseLayer)
{
	auto g = MakeGrid(104, 64);
	terrain.values = Pattern({ 104, 64 });
	other.values = std::vector<float>(104u * 64u, 0.0f);
	for (size_t i = 0; i < other.values.size(); i += 7) { other.values[i] = 3.0f; }
	registry.Register(Layer("terrain", &g, terrain, CellValueKind::Category, "cells/terrain"));	// base
	registry.Register(Layer("zones", &g, other, CellValueKind::Category, "cells/terrain"));
	LayerReader reader(registry);
	CellSelection s;
	s.Reset(&g);
	auto res = Tools::SelectCategory(s, reader, registry, cf.service, StringCRC("zones"), "sand");
	EXPECT_TRUE(res.ok);
	EXPECT_EQ(Expected(other.values, 3.0f), ToVector(s));
	EXPECT_EQ(0u, terrain.readCalls);
}

TEST_F(SelectionCategoryTest, UnknownLayerRejected)
{
	auto g = MakeGrid(66, 42);
	LayerReader reader(registry);
	CellSelection s;
	s.Reset(&g);
	s.Set(3, true);
	const uint32_t v = s.Version();
	auto res = Tools::SelectCategory(s, reader, registry, cf.service, StringCRC("nope"), "1");
	EXPECT_FALSE(res.ok);
	EXPECT_FALSE(res.changed);
	EXPECT_EQ(Tools::CategoryError::UnknownLayer, res.error);
	EXPECT_EQ(1u, s.Count());
	EXPECT_EQ(v, s.Version());
}

TEST_F(SelectionCategoryTest, FloatLayerIsNotCategory)
{
	auto g = MakeGrid(66, 42);
	temp.values = Pattern({ 66, 42 });
	registry.Register(Layer("temp", &g, temp, CellValueKind::Float, "cells/temp"));
	LayerReader reader(registry);
	CellSelection s;
	s.Reset(&g);
	s.Set(3, true);
	const uint32_t v = s.Version();
	auto res = Tools::SelectCategory(s, reader, registry, cf.service, StringCRC("temp"), "1");
	EXPECT_FALSE(res.ok);
	EXPECT_EQ(Tools::CategoryError::NotCategoryLayer, res.error);
	EXPECT_EQ(1u, s.Count());
	EXPECT_EQ(v, s.Version());
	EXPECT_EQ(0u, temp.readCalls);
}

TEST_F(SelectionCategoryTest, UnknownAndMalformedValuesRejected)
{
	auto g = MakeGrid(66, 42);
	terrain.values = Pattern({ 66, 42 });
	registry.Register(Layer("terrain", &g, terrain, CellValueKind::Category, "cells/terrain"));
	LayerReader reader(registry);
	CellSelection s;
	s.Reset(&g);
	s.Set(3, true);
	const uint32_t v = s.Version();
	for (const char* bad : { "lava", "Water", "WATER", "", "1x", "abc", "nan", "inf", "-inf", " ", "1 2" })
	{
		auto res = Tools::SelectCategory(s, reader, registry, cf.service, StringCRC("terrain"), bad);
		EXPECT_FALSE(res.ok) << bad;
		EXPECT_EQ(Tools::CategoryError::UnknownValue, res.error) << bad;
		EXPECT_EQ(1u, s.Count()) << bad;
		EXPECT_EQ(v, s.Version()) << bad;
	}
	auto nul = Tools::SelectCategory(s, reader, registry, cf.service, StringCRC("terrain"), nullptr);
	EXPECT_EQ(Tools::CategoryError::UnknownValue, nul.error);
}

TEST_F(SelectionCategoryTest, NaNCellsNeverMatch)
{
	auto g = MakeGrid(66, 42);
	terrain.values = Pattern({ 66, 42 });
	terrain.values[5] = std::numeric_limits<float>::quiet_NaN();	// would be water
	terrain.values[9] = 1.4f;										// rounds to 1: water
	terrain.values[13] = 1.6f;										// rounds to 2: rock
	registry.Register(Layer("terrain", &g, terrain, CellValueKind::Category, "cells/terrain"));
	LayerReader reader(registry);
	CellSelection s;
	s.Reset(&g);
	Tools::SelectCategory(s, reader, registry, cf.service, StringCRC("terrain"), "water");
	EXPECT_FALSE(s.Contains(5));
	EXPECT_TRUE(s.Contains(9));
	EXPECT_FALSE(s.Contains(13));
	EXPECT_EQ(Expected(terrain.values, 1.0f), ToVector(s));
}

TEST_F(SelectionCategoryTest, NoDataFromNonLiveSourceLeavesSelection)
{
	auto g = MakeGrid(66, 42);
	terrain.values = Pattern({ 66, 42 });
	registry.Register(Layer("terrain", &g, terrain, CellValueKind::Category, "cells/terrain"));
	StubViewValueSource src;
	src.live = false;
	src.hasData = false;
	registry.SetViewValueSource(&src);
	LayerReader reader(registry);
	CellSelection s;
	s.Reset(&g);
	s.Set(3, true);
	const uint32_t v = s.Version();
	auto res = Tools::SelectCategory(s, reader, registry, cf.service, StringCRC("terrain"), "water");
	EXPECT_FALSE(res.ok);
	EXPECT_EQ(Tools::CategoryError::NoData, res.error);
	EXPECT_EQ(1u, s.Count());
	EXPECT_EQ(v, s.Version());

	src.hasData = true;
	src.epoch = 2;
	src.values = Pattern({ 66, 42 });
	res = Tools::SelectCategory(s, reader, registry, cf.service, StringCRC("terrain"), "water");
	EXPECT_TRUE(res.ok);
	EXPECT_EQ(Expected(src.values, 1.0f), ToVector(s));
}

TEST_F(SelectionCategoryTest, LayerOnDifferentGridIsMismatch)
{
	auto g = MakeGrid(66, 42);
	auto g2 = MakeGrid(104, 64);
	terrain.values = Pattern({ 104, 64 });
	registry.Register(Layer("terrain", &g2, terrain, CellValueKind::Category, "cells/terrain"));
	LayerReader reader(registry);
	CellSelection s;
	s.Reset(&g);
	s.Set(3, true);
	const uint32_t v = s.Version();
	auto res = Tools::SelectCategory(s, reader, registry, cf.service, StringCRC("terrain"), "water");
	EXPECT_FALSE(res.ok);
	EXPECT_EQ(Tools::CategoryError::GridMismatch, res.error);
	EXPECT_EQ(1u, s.Count());
	EXPECT_EQ(v, s.Version());
}

TEST_F(SelectionCategoryTest, ValueInEnumButNoCellClearsSelection)
{
	auto g = MakeGrid(66, 42);
	terrain.values = Pattern({ 66, 42 });
	registry.Register(Layer("terrain", &g, terrain, CellValueKind::Category, "cells/terrain"));
	LayerReader reader(registry);
	CellSelection s;
	s.Reset(&g);
	s.Set(3, true);
	const uint32_t v = s.Version();
	auto res = Tools::SelectCategory(s, reader, registry, cf.service, StringCRC("terrain"), "unused");
	EXPECT_TRUE(res.ok);
	EXPECT_TRUE(res.changed);
	EXPECT_EQ(0u, res.count);
	EXPECT_EQ(0u, s.Count());
	EXPECT_EQ(v + 1, s.Version());
	// Already empty: ok, no change, no bump.
	res = Tools::SelectCategory(s, reader, registry, cf.service, StringCRC("terrain"), "unused");
	EXPECT_TRUE(res.ok);
	EXPECT_FALSE(res.changed);
	EXPECT_EQ(v + 1, s.Version());
}

TEST_F(SelectionCategoryTest, FractionalTargetSelectsNothing)
{
	auto g = MakeGrid(66, 42);
	terrain.values = Pattern({ 66, 42 });
	registry.Register(Layer("terrain", &g, terrain, CellValueKind::Category, "cells/terrain"));
	LayerReader reader(registry);
	CellSelection s;
	s.Reset(&g);
	s.Set(3, true);
	auto res = Tools::SelectCategory(s, reader, registry, cf.service, StringCRC("terrain"), "2.5");
	EXPECT_TRUE(res.ok);
	EXPECT_EQ(0u, s.Count());
}

TEST_F(SelectionCategoryTest, ReaderReadsLayerOncePerVersion)
{
	auto g = MakeGrid(66, 42);
	terrain.values = Pattern({ 66, 42 });
	registry.Register(Layer("terrain", &g, terrain, CellValueKind::Category, "cells/terrain"));
	LayerReader reader(registry);
	CellSelection s;
	s.Reset(&g);
	Tools::SelectCategory(s, reader, registry, cf.service, StringCRC("terrain"), "water");
	Tools::SelectCategory(s, reader, registry, cf.service, StringCRC("terrain"), "rock");
	EXPECT_EQ(1u, reader.Reads());
	EXPECT_EQ(1u, terrain.readCalls);
	terrain.Bump();
	Tools::SelectCategory(s, reader, registry, cf.service, StringCRC("terrain"), "rock");
	EXPECT_EQ(2u, reader.Reads());
}
