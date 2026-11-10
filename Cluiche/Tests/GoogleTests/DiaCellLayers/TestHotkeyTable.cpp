#include <gtest/gtest.h>
#include <DiaCellLayers/HotkeyTable.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/Testing/ValueInfoBuilder.h>

#include <stdio.h>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::ValueSchema::ValueInfo;
using Dia::ValueSchema::ValueSchemaRegistry;
using Dia::ValueSchema::ValueSchemaService;

namespace
{
	void AddKey(ValueSchemaRegistry& schema, const char* name, const char* hotkey)
	{
		ValueInfo v = Dia::ValueSchema::Testing::MakeValueInfo("cow.cell", name);
		if (hotkey != nullptr) { v.mUi["hotkey"] = hotkey; }
		schema.Register(v);
	}
}

TEST(HotkeyTable, UniqueKeysMap)
{
	ValueSchemaRegistry schema;
	AddKey(schema, "a", "t");
	AddKey(schema, "b", "m");
	AddKey(schema, "c", nullptr);
	schema.Publish();
	ValueSchemaService svc(schema);
	Dia::Graphics::CellGridDesc grid;
	CellLayerRegistry reg(&svc, &FakePuIdFn);
	reg.Register(MakeLayer("a", &grid, "cow.cell/a"));
	reg.Register(MakeLayer("b", &grid, "cow.cell/b"));
	reg.Register(MakeLayer("c", &grid, "cow.cell/c"));

	HotkeyTable table(reg, &svc);
	EXPECT_EQ(table.Rebuild(&grid), 2u);
	ASSERT_NE(table.Lookup('t'), nullptr);
	EXPECT_EQ(table.Lookup('t')->id, StringCRC("a"));
	EXPECT_EQ(table.Lookup('m')->id, StringCRC("b"));
	EXPECT_EQ(table.HotkeyOf(StringCRC("b")), 'm');
	EXPECT_EQ(table.HotkeyOf(StringCRC("c")), 0);
	EXPECT_EQ(table.ClashCount(), 0u);
}

TEST(HotkeyTable, UnknownKeyReturnsNull)
{
	ValueSchemaRegistry schema;
	AddKey(schema, "a", "t");
	schema.Publish();
	ValueSchemaService svc(schema);
	Dia::Graphics::CellGridDesc grid;
	CellLayerRegistry reg(&svc, &FakePuIdFn);
	reg.Register(MakeLayer("a", &grid, "cow.cell/a"));
	HotkeyTable table(reg, &svc);
	table.Rebuild(&grid);
	EXPECT_EQ(table.Lookup('z'), nullptr);
}

TEST(HotkeyTable, ClashGivesLaterLayerNoHotkey)
{
	ValueSchemaRegistry schema;
	AddKey(schema, "a", "t");
	AddKey(schema, "b", "t");
	schema.Publish();
	ValueSchemaService svc(schema);
	Dia::Graphics::CellGridDesc grid;
	CellLayerRegistry reg(&svc, &FakePuIdFn);
	reg.Register(MakeLayer("a", &grid, "cow.cell/a"));
	reg.Register(MakeLayer("b", &grid, "cow.cell/b"));
	HotkeyTable table(reg, &svc);
	EXPECT_EQ(table.Rebuild(&grid), 1u);
	EXPECT_EQ(table.Lookup('t')->id, StringCRC("a"));
	EXPECT_EQ(table.HotkeyOf(StringCRC("b")), 0);
	EXPECT_EQ(table.ClashCount(), 1u);
}

TEST(HotkeyTable, KeysAreIndependentPerGrid)
{
	ValueSchemaRegistry schema;
	AddKey(schema, "a", "t");
	AddKey(schema, "b", "t");
	schema.Publish();
	ValueSchemaService svc(schema);
	Dia::Graphics::CellGridDesc gridA, gridB;
	CellLayerRegistry reg(&svc, &FakePuIdFn);
	reg.Register(MakeLayer("a", &gridA, "cow.cell/a"));
	reg.Register(MakeLayer("b", &gridB, "cow.cell/b"));
	HotkeyTable table(reg, &svc);
	table.Rebuild(&gridA);
	EXPECT_EQ(table.Lookup('t')->id, StringCRC("a"));
	table.Rebuild(&gridB);
	EXPECT_EQ(table.Lookup('t')->id, StringCRC("b"));
	EXPECT_EQ(table.ClashCount(), 0u);
}

TEST(HotkeyTable, NullSchemaBindsNothing)
{
	Dia::Graphics::CellGridDesc grid;
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	reg.Register(MakeLayer("a", &grid));
	HotkeyTable table(reg, nullptr);
	EXPECT_EQ(table.Rebuild(&grid), 0u);
}

TEST(HotkeyTable, GroupNumberingOneToNinePerGroup)
{
	Dia::Graphics::CellGridDesc grid;
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	char id[16];
	for (int i = 0; i < 11; ++i)
	{
		snprintf(id, sizeof(id), "w%d", i);
		reg.Register(MakeLayer(id, &grid, nullptr, CellValueKind::Float, "Weather"));
	}
	reg.Register(MakeLayer("t0", &grid, nullptr, CellValueKind::Float, "Terrain"));
	reg.Register(MakeLayer("t1", &grid, nullptr, CellValueKind::Float, "Terrain"));

	EXPECT_EQ(HotkeyTable::GroupNumber(reg, &grid, StringCRC("w0")), 1u);
	EXPECT_EQ(HotkeyTable::GroupNumber(reg, &grid, StringCRC("w8")), 9u);
	EXPECT_EQ(HotkeyTable::GroupNumber(reg, &grid, StringCRC("w9")), 0u);	// 10th in group
	EXPECT_EQ(HotkeyTable::GroupNumber(reg, &grid, StringCRC("w10")), 0u);
	EXPECT_EQ(HotkeyTable::GroupNumber(reg, &grid, StringCRC("t0")), 1u);	// groups independent
	EXPECT_EQ(HotkeyTable::GroupNumber(reg, &grid, StringCRC("t1")), 2u);
	EXPECT_EQ(HotkeyTable::GroupNumber(reg, &grid, StringCRC("nope")), 0u);
}
