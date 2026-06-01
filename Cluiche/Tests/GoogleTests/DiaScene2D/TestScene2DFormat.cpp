#include <gtest/gtest.h>

#include <DiaScene2D/Scene2D.h>
#include <DiaScene2D/DiaScene2DSerializers.h>
#include <DiaScene2D/LayerTable.h>
#include <DiaCore/Reflect/JsonArchive.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::Scene2D;
using Dia::Core::StringCRC;

// ---------------------------------------------------------------------------
// LayerDef serialization roundtrip
// ---------------------------------------------------------------------------

TEST(DiaScene2D_Format, LayerDef_Roundtrip)
{
    LayerDef original;
    original.id        = StringCRC("midground");
    original.sortOrder = 5;
    original.parallax  = {0.8f, 0.9f};
    original.sortPolicy = StringCRC("insertion");
    original.enabled   = true;

    Dia::Reflect::JsonWriteArchive writer;
    serialize(writer, original, 1u);

    LayerDef result;
    Dia::Reflect::JsonReadArchive reader(writer.GetRoot());
    serialize(reader, result, 1u);

    EXPECT_EQ(result.id, original.id);
    EXPECT_EQ(result.sortOrder, original.sortOrder);
    EXPECT_FLOAT_EQ(result.parallax.x, original.parallax.x);
    EXPECT_FLOAT_EQ(result.parallax.y, original.parallax.y);
    EXPECT_EQ(result.sortPolicy, original.sortPolicy);
    EXPECT_EQ(result.enabled, original.enabled);
}

TEST(DiaScene2D_Format, LayerDef_DefaultsPreservedOnMissingOptionals)
{
    Json::Value node(Json::objectValue);
    node["id"]["value"]        = "background";
    node["sort_order"]         = -10;
    node["sort_policy"]["value"] = "insertion";

    LayerDef result;
    Dia::Reflect::JsonReadArchive reader(node);
    serialize(reader, result, 1u);

    EXPECT_FLOAT_EQ(result.parallax.x, 1.0f);
    EXPECT_FLOAT_EQ(result.parallax.y, 1.0f);
    EXPECT_TRUE(result.enabled);
}

// ---------------------------------------------------------------------------
// LayerTable — Build and query
// ---------------------------------------------------------------------------

TEST(DiaScene2D_LayerTable, Build_InjectsDefaultAtBit0_WhenAbsent)
{
    Dia::Core::Containers::DynamicArrayC<LayerDef, 32> layers;

    LayerDef bg;
    bg.id = StringCRC("background");
    bg.sortOrder = -10;
    layers.Add(bg);

    LayerTable table;
    table.Build(layers);

    EXPECT_TRUE(table.Has(StringCRC("default")));
    EXPECT_EQ(table.GetBitIndex(StringCRC("default")), 0u);
    EXPECT_EQ(table.GetBitIndex(StringCRC("background")), 1u);
    EXPECT_EQ(table.GetCount(), 2u);
}

TEST(DiaScene2D_LayerTable, Build_DoesNotDuplicateDefault_WhenPresent)
{
    Dia::Core::Containers::DynamicArrayC<LayerDef, 32> layers;

    LayerDef def;
    def.id = StringCRC("default");
    def.sortOrder = 0;
    layers.Add(def);

    LayerDef fg;
    fg.id = StringCRC("foreground");
    fg.sortOrder = 10;
    layers.Add(fg);

    LayerTable table;
    table.Build(layers);

    EXPECT_EQ(table.GetCount(), 2u);
    EXPECT_EQ(table.GetBitIndex(StringCRC("default")), 0u);
    EXPECT_EQ(table.GetBitIndex(StringCRC("foreground")), 1u);
}

TEST(DiaScene2D_LayerTable, ResolveMask_MatchesBitIndices)
{
    Dia::Core::Containers::DynamicArrayC<LayerDef, 32> layers;

    LayerDef a; a.id = StringCRC("background"); a.sortOrder = -10; layers.Add(a);
    LayerDef b; b.id = StringCRC("midground");  b.sortOrder =   0; layers.Add(b);
    LayerDef c; c.id = StringCRC("foreground"); c.sortOrder =  10; layers.Add(c);

    LayerTable table;
    table.Build(layers);  // default injected at 0; background=1, mid=2, fore=3

    Dia::Core::Containers::DynamicArrayC<StringCRC, 32> names;
    names.Add(StringCRC("midground"));
    names.Add(StringCRC("foreground"));

    uint32_t mask = table.ResolveMask(names);
    EXPECT_TRUE(mask & (1u << table.GetBitIndex(StringCRC("midground"))));
    EXPECT_TRUE(mask & (1u << table.GetBitIndex(StringCRC("foreground"))));
    EXPECT_FALSE(mask & (1u << table.GetBitIndex(StringCRC("background"))));
}

TEST(DiaScene2D_LayerTable, UnknownLayerName_FallsBackToBit0)
{
    Dia::Core::Containers::DynamicArrayC<LayerDef, 32> layers;
    LayerTable table;
    table.Build(layers);  // only "default" at bit 0

    EXPECT_EQ(table.GetBitIndex(StringCRC("nonexistent")), 0u);
}

TEST(DiaScene2D_LayerTable, Has_ReturnsFalseForUnknown)
{
    Dia::Core::Containers::DynamicArrayC<LayerDef, 32> layers;
    LayerTable table;
    table.Build(layers);
    EXPECT_FALSE(table.Has(StringCRC("ghost_layer")));
}

TEST(DiaScene2D_LayerTable, GetByIndex_ReturnsCorrectLayer)
{
    Dia::Core::Containers::DynamicArrayC<LayerDef, 32> layers;
    LayerDef fg; fg.id = StringCRC("foreground"); fg.sortOrder = 10; layers.Add(fg);

    LayerTable table;
    table.Build(layers);

    // slot 0 = injected "default", slot 1 = "foreground"
    EXPECT_EQ(table.GetByIndex(1u).id, StringCRC("foreground"));
    EXPECT_EQ(table.GetByIndex(1u).sortOrder, 10);
}

// ---------------------------------------------------------------------------
// Empty layers — just default
// ---------------------------------------------------------------------------

TEST(DiaScene2D_LayerTable, EmptyLayers_JustDefault)
{
    Dia::Core::Containers::DynamicArrayC<LayerDef, 32> layers;
    LayerTable table;
    table.Build(layers);
    EXPECT_EQ(table.GetCount(), 1u);
    EXPECT_EQ(table.GetBitIndex(StringCRC("default")), 0u);
}
