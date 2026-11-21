#include <gtest/gtest.h>
#include <DiaCellLayers/CellLayerRegistry.h>
#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaValueSchema/ValueSchemaRegistry.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaValueSchema/Testing/ValueInfoBuilder.h>
#include <stdio.h>

using namespace Dia::CellLayers;
using namespace Dia::CellLayers::Testing;
using Dia::Core::StringCRC;
using Dia::Graphics::CellGridDesc;

namespace
{
	struct CellLayerRegistryTest : ::testing::Test
	{
		CellGridDesc gridA, gridB;
		void SetUp() override { FakePuId() = 1; }
	};
}

TEST_F(CellLayerRegistryTest, RegisterFindUnregisterLayer)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	EXPECT_EQ(reg.Register(MakeLayer("a", &gridA)), CellRegisterResult::Ok);
	ASSERT_NE(reg.Find(StringCRC("a")), nullptr);
	EXPECT_EQ(reg.Find(StringCRC("a"))->grid, &gridA);
	EXPECT_EQ(reg.Find(StringCRC("zzz")), nullptr);
	EXPECT_TRUE(reg.Unregister(StringCRC("a")));
	EXPECT_EQ(reg.Find(StringCRC("a")), nullptr);
	EXPECT_EQ(reg.LayerCount(), 0u);
}

TEST_F(CellLayerRegistryTest, EnumerationIsRegistrationOrder)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	reg.Register(MakeLayer("c", &gridA));
	reg.Register(MakeLayer("a", &gridA));
	reg.Register(MakeLayer("b", &gridA));
	ASSERT_EQ(reg.LayerCount(), 3u);
	EXPECT_EQ(reg.LayerAt(0)->id, StringCRC("c"));
	EXPECT_EQ(reg.LayerAt(1)->id, StringCRC("a"));
	EXPECT_EQ(reg.LayerAt(2)->id, StringCRC("b"));
	EXPECT_EQ(reg.LayerAt(3), nullptr);
	reg.Unregister(StringCRC("a"));
	EXPECT_EQ(reg.LayerAt(1)->id, StringCRC("b"));
}

TEST_F(CellLayerRegistryTest, DuplicateLayerRejectedRegistryUnchanged)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	reg.Register(MakeLayer("a", &gridA));
	EXPECT_EQ(reg.Register(MakeLayer("a", &gridB)), CellRegisterResult::Duplicate);
	EXPECT_EQ(reg.LayerCount(), 1u);
	EXPECT_EQ(reg.Find(StringCRC("a"))->grid, &gridA);
}

TEST_F(CellLayerRegistryTest, DuplicateOverlayRejectedRegistryUnchanged)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	reg.RegisterOverlay(MakeOverlay("o", "First"));
	EXPECT_EQ(reg.RegisterOverlay(MakeOverlay("o", "Second")), CellRegisterResult::Duplicate);
	EXPECT_EQ(reg.OverlayCount(), 1u);
	EXPECT_STREQ(reg.FindOverlay(StringCRC("o"))->label.AsCStr(), "First");
}

TEST_F(CellLayerRegistryTest, LayerCapacity128)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	char name[16];
	for (int i = 0; i < 128; ++i)
	{
		snprintf(name, sizeof(name), "l%d", i);
		ASSERT_EQ(reg.Register(MakeLayer(name, &gridA)), CellRegisterResult::Ok) << i;
	}
	EXPECT_EQ(reg.Register(MakeLayer("l128", &gridA)), CellRegisterResult::CapacityReached);
	EXPECT_EQ(reg.LayerCount(), 128u);
	EXPECT_EQ(reg.Find(StringCRC("l128")), nullptr);
}

TEST_F(CellLayerRegistryTest, OverlayCapacity64)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	char name[16];
	for (int i = 0; i < 64; ++i)
	{
		snprintf(name, sizeof(name), "o%d", i);
		ASSERT_EQ(reg.RegisterOverlay(MakeOverlay(name)), CellRegisterResult::Ok) << i;
	}
	EXPECT_EQ(reg.RegisterOverlay(MakeOverlay("o64")), CellRegisterResult::CapacityReached);
	EXPECT_EQ(reg.OverlayCount(), 64u);
}

TEST_F(CellLayerRegistryTest, OtherProcessingUnitRejected)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	FakePuId() = 2;
	EXPECT_EQ(reg.Register(MakeLayer("a", &gridA)), CellRegisterResult::WrongProcessingUnit);
	EXPECT_EQ(reg.RegisterOverlay(MakeOverlay("o")), CellRegisterResult::WrongProcessingUnit);
	EXPECT_EQ(reg.LayerCount(), 0u);
	EXPECT_EQ(reg.OverlayCount(), 0u);
	FakePuId() = 1;
	EXPECT_EQ(reg.Register(MakeLayer("a", &gridA)), CellRegisterResult::Ok);
}

TEST_F(CellLayerRegistryTest, DefaultOwnerIsConstructingThread)
{
	CellLayerRegistry reg;
	EXPECT_EQ(reg.Register(MakeLayer("a", &gridA)), CellRegisterResult::Ok);
}

TEST_F(CellLayerRegistryTest, GridGroupingAndMultiGrid)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	EXPECT_EQ(reg.GridCount(), 0u);
	EXPECT_FALSE(reg.IsMultiGrid());
	reg.Register(MakeLayer("a1", &gridA));
	EXPECT_EQ(reg.GridCount(), 1u);
	EXPECT_FALSE(reg.IsMultiGrid());
	reg.Register(MakeLayer("b1", &gridB));
	reg.Register(MakeLayer("a2", &gridA));
	EXPECT_EQ(reg.GridCount(), 2u);
	EXPECT_TRUE(reg.IsMultiGrid());
	EXPECT_EQ(reg.GridAt(0), &gridA);
	EXPECT_EQ(reg.GridAt(1), &gridB);
	EXPECT_EQ(reg.GridAt(2), nullptr);
	ASSERT_EQ(reg.LayerCountForGrid(&gridA), 2u);
	EXPECT_EQ(reg.LayerOfGrid(&gridA, 0)->id, StringCRC("a1"));
	EXPECT_EQ(reg.LayerOfGrid(&gridA, 1)->id, StringCRC("a2"));
	EXPECT_EQ(reg.LayerOfGrid(&gridA, 2), nullptr);
	EXPECT_EQ(reg.LayerCountForGrid(&gridB), 1u);
	reg.Unregister(StringCRC("b1"));
	EXPECT_FALSE(reg.IsMultiGrid());
}

TEST_F(CellLayerRegistryTest, DefaultBaseFallsBackToFirstWithoutSchema)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	EXPECT_EQ(reg.DefaultBase(&gridA), nullptr);
	reg.Register(MakeLayer("a1", &gridA));
	reg.Register(MakeLayer("a2", &gridA));
	EXPECT_EQ(reg.DefaultBase(&gridA)->id, StringCRC("a1"));
}

TEST_F(CellLayerRegistryTest, DefaultBaseUsesSchemaFlagPerGrid)
{
	using namespace Dia::ValueSchema;
	ValueSchemaRegistry schema;
	ValueInfo plain = Dia::ValueSchema::Testing::MakeValueInfo("cow.cell", "plain");
	ValueInfo base = Dia::ValueSchema::Testing::MakeValueInfo("cow.cell", "base");
	base.mUi["default_base"] = true;
	ValueInfo off = Dia::ValueSchema::Testing::MakeValueInfo("cow.cell", "off");
	off.mUi["default_base"] = false;
	schema.Register(plain);
	schema.Register(base);
	schema.Register(off);
	schema.Publish();
	ValueSchemaService svc(schema);

	CellLayerRegistry reg(&svc, &FakePuIdFn);
	reg.Register(MakeLayer("a1", &gridA, "cow.cell/plain"));
	reg.Register(MakeLayer("a2", &gridA, "cow.cell/base"));
	reg.Register(MakeLayer("b1", &gridB, "cow.cell/off"));
	reg.Register(MakeLayer("b2", &gridB, "cow.cell/unknown"));
	EXPECT_EQ(reg.DefaultBase(&gridA)->id, StringCRC("a2"));
	EXPECT_EQ(reg.DefaultBase(&gridB)->id, StringCRC("b1"));
}

TEST_F(CellLayerRegistryTest, UnregisterUnknownChangesNothing)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	reg.Register(MakeLayer("a", &gridA));
	reg.RegisterOverlay(MakeOverlay("o"));
	EXPECT_FALSE(reg.Unregister(StringCRC("nope")));
	EXPECT_FALSE(reg.UnregisterOverlay(StringCRC("nope")));
	EXPECT_EQ(reg.LayerCount(), 1u);
	EXPECT_EQ(reg.OverlayCount(), 1u);
}

TEST_F(CellLayerRegistryTest, OverlayRegisterFindUnregisterOrder)
{
	CellLayerRegistry reg(nullptr, &FakePuIdFn);
	EXPECT_EQ(reg.RegisterOverlay(MakeOverlay("o1", "Wind arrows", "Weather")), CellRegisterResult::Ok);
	reg.RegisterOverlay(MakeOverlay("o2"));
	ASSERT_NE(reg.FindOverlay(StringCRC("o1")), nullptr);
	EXPECT_STREQ(reg.FindOverlay(StringCRC("o1"))->label.AsCStr(), "Wind arrows");
	EXPECT_STREQ(reg.FindOverlay(StringCRC("o1"))->group.AsCStr(), "Weather");
	EXPECT_EQ(reg.OverlayAt(1)->id, StringCRC("o2"));
	EXPECT_TRUE(reg.UnregisterOverlay(StringCRC("o1")));
	EXPECT_EQ(reg.FindOverlay(StringCRC("o1")), nullptr);
	EXPECT_EQ(reg.OverlayAt(0)->id, StringCRC("o2"));
	EXPECT_EQ(reg.OverlayAt(1), nullptr);
}
