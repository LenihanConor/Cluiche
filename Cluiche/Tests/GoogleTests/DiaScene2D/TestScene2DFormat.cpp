#include <gtest/gtest.h>

#include <DiaScene2D/Scene2D.h>
#include <DiaScene2D/DiaScene2DSerializers.h>
#include <DiaScene2D/LayerTable.h>
#include <DiaCore/Reflect/JsonArchive.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

using namespace Dia::Scene2D;
using Dia::Core::StringCRC;

// ===========================================================================
// LayerDef serialization
// ===========================================================================

TEST(DiaScene2D_LayerDef, Roundtrip_AllFields)
{
    LayerDef original;
    original.id              = StringCRC("midground");
    original.sortOrder       = 5;
    original.parallax        = {0.8f, 0.9f};
    original.sortPolicy      = StringCRC("insertion");
    original.enabled         = true;
    original.renderTechnique = StringCRC("additive_bloom");

    Dia::Reflect::JsonWriteArchive writer;
    serialize(writer, original, 1u);

    LayerDef result;
    Dia::Reflect::JsonReadArchive reader(writer.GetRoot());
    serialize(reader, result, 1u);

    EXPECT_EQ(result.id,              original.id);
    EXPECT_EQ(result.sortOrder,       original.sortOrder);
    EXPECT_FLOAT_EQ(result.parallax.x, original.parallax.x);
    EXPECT_FLOAT_EQ(result.parallax.y, original.parallax.y);
    EXPECT_EQ(result.sortPolicy,      original.sortPolicy);
    EXPECT_EQ(result.enabled,         original.enabled);
    EXPECT_EQ(result.renderTechnique, original.renderTechnique);
}

TEST(DiaScene2D_LayerDef, Roundtrip_DisabledLayer)
{
    LayerDef original;
    original.id        = StringCRC("hidden");
    original.sortOrder = 99;
    original.enabled   = false;

    Dia::Reflect::JsonWriteArchive writer;
    serialize(writer, original, 1u);

    LayerDef result;
    Dia::Reflect::JsonReadArchive reader(writer.GetRoot());
    serialize(reader, result, 1u);

    EXPECT_FALSE(result.enabled);
    EXPECT_EQ(result.sortOrder, 99);
}

TEST(DiaScene2D_LayerDef, Defaults_PreservedForMissingOptionals)
{
    Json::Value node(Json::objectValue);
    node["id"]["value"]          = "background";
    node["sort_order"]           = -10;
    node["sort_policy"]["value"] = "insertion";

    LayerDef result;
    Dia::Reflect::JsonReadArchive reader(node);
    serialize(reader, result, 1u);

    EXPECT_FLOAT_EQ(result.parallax.x, 1.0f);
    EXPECT_FLOAT_EQ(result.parallax.y, 1.0f);
    EXPECT_TRUE(result.enabled);
}

TEST(DiaScene2D_LayerDef, NegativeSortOrder_Roundtrips)
{
    LayerDef original;
    original.id        = StringCRC("bg");
    original.sortOrder = -100;
    original.sortPolicy = StringCRC("insertion");

    Dia::Reflect::JsonWriteArchive writer;
    serialize(writer, original, 1u);

    LayerDef result;
    Dia::Reflect::JsonReadArchive reader(writer.GetRoot());
    serialize(reader, result, 1u);

    EXPECT_EQ(result.sortOrder, -100);
}

// ===========================================================================
// CameraEntry serialization
// ===========================================================================

TEST(DiaScene2D_CameraEntry, Roundtrip_BasicFields)
{
    // CameraEntry uses a hand-written serialize — test JSON read via a raw node
    Json::Value node(Json::objectValue);
    node["id"]["value"]        = "gameplay";
    node["active"]             = true;
    node["blueprint"]["value"] = "follow_cam";

    CameraEntry result;
    Dia::Reflect::JsonReadArchive reader(node);
    serialize(reader, result, 1u);

    EXPECT_EQ(result.id,        StringCRC("gameplay"));
    EXPECT_TRUE(result.active);
    EXPECT_EQ(result.blueprint, StringCRC("follow_cam"));
}

TEST(DiaScene2D_CameraEntry, InactiveCamera_DefaultsFalse)
{
    Json::Value node(Json::objectValue);
    node["id"]["value"]        = "cinematic";
    node["blueprint"]["value"] = "pan_cam";
    // active omitted — default false

    CameraEntry result;
    Dia::Reflect::JsonReadArchive reader(node);
    serialize(reader, result, 1u);

    EXPECT_FALSE(result.active);
}

TEST(DiaScene2D_CameraEntry, InstanceData_CapturedOpaque)
{
    Json::Value node(Json::objectValue);
    node["id"]["value"]        = "cam";
    node["blueprint"]["value"] = "b";
    node["active"]             = true;
    node["instance_data"]["Camera2D.position"][0] = 10.0f;
    node["instance_data"]["Camera2D.position"][1] = 20.0f;

    CameraEntry result;
    Dia::Reflect::JsonReadArchive reader(node);
    serialize(reader, result, 1u);

    EXPECT_TRUE(result.instanceData.isObject());
    EXPECT_TRUE(result.instanceData.isMember("Camera2D.position"));
}

// ===========================================================================
// LightEntry serialization
// ===========================================================================

TEST(DiaScene2D_LightEntry, Roundtrip_BasicFields)
{
    Json::Value node(Json::objectValue);
    node["id"]["value"]        = "torch";
    node["enabled"]            = true;
    node["blueprint"]["value"] = "warm_light";
    node["affects_layers"][0]["value"] = "foreground";
    node["affects_layers"][1]["value"] = "midground";

    LightEntry result;
    Dia::Reflect::JsonReadArchive reader(node);
    serialize(reader, result, 1u);

    EXPECT_EQ(result.id,        StringCRC("torch"));
    EXPECT_TRUE(result.enabled);
    EXPECT_EQ(result.blueprint, StringCRC("warm_light"));
    EXPECT_EQ(result.affectsLayers.Size(), 2u);
    EXPECT_EQ(result.affectsLayers.At(0), StringCRC("foreground"));
    EXPECT_EQ(result.affectsLayers.At(1), StringCRC("midground"));
}

TEST(DiaScene2D_LightEntry, DisabledLight_DefaultsFalse)
{
    Json::Value node(Json::objectValue);
    node["id"]["value"]        = "secret";
    node["enabled"]            = false;
    node["blueprint"]["value"] = "cool_light";

    LightEntry result;
    Dia::Reflect::JsonReadArchive reader(node);
    serialize(reader, result, 1u);

    EXPECT_FALSE(result.enabled);
}

TEST(DiaScene2D_LightEntry, EmptyAffectsLayers_ZeroEntries)
{
    Json::Value node(Json::objectValue);
    node["id"]["value"]        = "ambient";
    node["blueprint"]["value"] = "ambient";
    node["enabled"]            = true;
    node["affects_layers"]     = Json::Value(Json::arrayValue);

    LightEntry result;
    Dia::Reflect::JsonReadArchive reader(node);
    serialize(reader, result, 1u);

    EXPECT_EQ(result.affectsLayers.Size(), 0u);
}

TEST(DiaScene2D_LightEntry, InstanceData_CapturedOpaque)
{
    Json::Value node(Json::objectValue);
    node["id"]["value"]        = "l";
    node["blueprint"]["value"] = "b";
    node["instance_data"]["PointLight2D.radius"] = 120.0f;

    LightEntry result;
    Dia::Reflect::JsonReadArchive reader(node);
    serialize(reader, result, 1u);

    EXPECT_TRUE(result.instanceData.isObject());
    EXPECT_TRUE(result.instanceData.isMember("PointLight2D.radius"));
}

// ===========================================================================
// EntityInstance serialization
// ===========================================================================

TEST(DiaScene2D_EntityInstance, Roundtrip_BasicFields)
{
    Json::Value node(Json::objectValue);
    node["id"]["value"]        = "player_01";
    node["name"]["value"]      = "player_spawn";
    node["blueprint"]["value"] = "hero";
    node["enabled"]            = true;

    EntityInstance result;
    Dia::Reflect::JsonReadArchive reader(node);
    serialize(reader, result, 1u);

    EXPECT_EQ(result.id,        StringCRC("player_01"));
    EXPECT_EQ(result.name,      StringCRC("player_spawn"));
    EXPECT_EQ(result.blueprint, StringCRC("hero"));
    EXPECT_TRUE(result.enabled);
}

TEST(DiaScene2D_EntityInstance, DisabledEntity_FlagPreserved)
{
    Json::Value node(Json::objectValue);
    node["id"]["value"]        = "dormant";
    node["blueprint"]["value"] = "prop";
    node["enabled"]            = false;

    EntityInstance result;
    Dia::Reflect::JsonReadArchive reader(node);
    serialize(reader, result, 1u);

    EXPECT_FALSE(result.enabled);
}

TEST(DiaScene2D_EntityInstance, OptionalName_DefaultsEmpty)
{
    Json::Value node(Json::objectValue);
    node["id"]["value"]        = "crate";
    node["blueprint"]["value"] = "prop";

    EntityInstance result;
    Dia::Reflect::JsonReadArchive reader(node);
    serialize(reader, result, 1u);

    EXPECT_EQ(result.name, StringCRC(""));
}

TEST(DiaScene2D_EntityInstance, InstanceData_CapturedOpaque)
{
    Json::Value node(Json::objectValue);
    node["id"]["value"]        = "e";
    node["blueprint"]["value"] = "b";
    node["instance_data"]["Transform2D.position"][0] = 100.0f;
    node["instance_data"]["Transform2D.position"][1] = 200.0f;

    EntityInstance result;
    Dia::Reflect::JsonReadArchive reader(node);
    serialize(reader, result, 1u);

    EXPECT_TRUE(result.instanceData.isObject());
    EXPECT_TRUE(result.instanceData.isMember("Transform2D.position"));
}

// ===========================================================================
// Scene2D struct roundtrip
// ===========================================================================

TEST(DiaScene2D_Scene2D, Roundtrip_EmptyScene_NoErrors)
{
    Scene2D original;
    // worldBounds default = zero-area, all arrays empty

    Dia::Reflect::JsonWriteArchive writer;
    serialize(writer, original, 1u);

    Scene2D result;
    Dia::Reflect::JsonReadArchive reader(writer.GetRoot());
    serialize(reader, result, 1u);
    EXPECT_FALSE(reader.GetResult().HasErrors());
    EXPECT_EQ(result.layers.Size(),   0u);
    EXPECT_EQ(result.cameras.Size(),  0u);
    EXPECT_EQ(result.lights.Size(),   0u);
    EXPECT_EQ(result.entities.Size(), 0u);
}

TEST(DiaScene2D_Scene2D, Roundtrip_LayersPreserved)
{
    Scene2D original;
    LayerDef bg; bg.id = StringCRC("bg"); bg.sortOrder = -10; bg.sortPolicy = StringCRC("insertion");
    LayerDef fg; fg.id = StringCRC("fg"); fg.sortOrder =  10; fg.sortPolicy = StringCRC("insertion");
    original.layers.Add(bg);
    original.layers.Add(fg);

    Dia::Reflect::JsonWriteArchive writer;
    serialize(writer, original, 1u);

    Scene2D result;
    Dia::Reflect::JsonReadArchive reader(writer.GetRoot());
    serialize(reader, result, 1u);

    EXPECT_EQ(result.layers.Size(), 2u);
    EXPECT_EQ(result.layers.At(0).id, StringCRC("bg"));
    EXPECT_EQ(result.layers.At(1).id, StringCRC("fg"));
}

// ===========================================================================
// LayerTable
// ===========================================================================

TEST(DiaScene2D_LayerTable, Build_InjectsDefaultAtBit0_WhenAbsent)
{
    Dia::Core::Containers::DynamicArrayC<LayerDef, 32> layers;
    LayerDef bg; bg.id = StringCRC("background"); bg.sortOrder = -10; layers.Add(bg);

    LayerTable table;
    table.Build(layers);

    EXPECT_TRUE(table.Has(StringCRC("default")));
    EXPECT_EQ(table.GetBitIndex(StringCRC("default")),    0u);
    EXPECT_EQ(table.GetBitIndex(StringCRC("background")), 1u);
    EXPECT_EQ(table.GetCount(), 2u);
}

TEST(DiaScene2D_LayerTable, Build_DoesNotDuplicateDefault_WhenPresent)
{
    Dia::Core::Containers::DynamicArrayC<LayerDef, 32> layers;
    LayerDef def; def.id = StringCRC("default");    def.sortOrder =  0; layers.Add(def);
    LayerDef fg;  fg.id  = StringCRC("foreground"); fg.sortOrder  = 10; layers.Add(fg);

    LayerTable table;
    table.Build(layers);

    EXPECT_EQ(table.GetCount(), 2u);
    EXPECT_EQ(table.GetBitIndex(StringCRC("default")),    0u);
    EXPECT_EQ(table.GetBitIndex(StringCRC("foreground")), 1u);
}

TEST(DiaScene2D_LayerTable, ResolveMask_MatchesBitIndices)
{
    Dia::Core::Containers::DynamicArrayC<LayerDef, 32> layers;
    LayerDef a; a.id = StringCRC("background"); a.sortOrder = -10; layers.Add(a);
    LayerDef b; b.id = StringCRC("midground");  b.sortOrder =   0; layers.Add(b);
    LayerDef c; c.id = StringCRC("foreground"); c.sortOrder =  10; layers.Add(c);

    LayerTable table;
    table.Build(layers);

    Dia::Core::Containers::DynamicArrayC<StringCRC, 32> names;
    names.Add(StringCRC("midground"));
    names.Add(StringCRC("foreground"));

    uint32_t mask = table.ResolveMask(names);
    EXPECT_TRUE (mask & (1u << table.GetBitIndex(StringCRC("midground"))));
    EXPECT_TRUE (mask & (1u << table.GetBitIndex(StringCRC("foreground"))));
    EXPECT_FALSE(mask & (1u << table.GetBitIndex(StringCRC("background"))));
}

TEST(DiaScene2D_LayerTable, ResolveMask_EmptyNames_ZeroMask)
{
    Dia::Core::Containers::DynamicArrayC<LayerDef, 32> layers;
    LayerDef a; a.id = StringCRC("bg"); a.sortOrder = -10; layers.Add(a);
    LayerTable table;
    table.Build(layers);

    Dia::Core::Containers::DynamicArrayC<StringCRC, 32> empty;
    EXPECT_EQ(table.ResolveMask(empty), 0u);
}

TEST(DiaScene2D_LayerTable, ResolveMask_AllLayersIncluded)
{
    Dia::Core::Containers::DynamicArrayC<LayerDef, 32> layers;
    LayerDef a; a.id = StringCRC("a"); layers.Add(a);
    LayerDef b; b.id = StringCRC("b"); layers.Add(b);
    LayerTable table;
    table.Build(layers);  // default + a + b = 3 layers at bits 0,1,2

    Dia::Core::Containers::DynamicArrayC<StringCRC, 32> all;
    all.Add(StringCRC("default")); all.Add(StringCRC("a")); all.Add(StringCRC("b"));
    uint32_t mask = table.ResolveMask(all);
    EXPECT_EQ(mask, 0b111u);
}

TEST(DiaScene2D_LayerTable, UnknownLayerName_FallsBackToBit0)
{
    Dia::Core::Containers::DynamicArrayC<LayerDef, 32> layers;
    LayerTable table;
    table.Build(layers);

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

    EXPECT_EQ(table.GetByIndex(1u).id, StringCRC("foreground"));
    EXPECT_EQ(table.GetByIndex(1u).sortOrder, 10);
}

TEST(DiaScene2D_LayerTable, GetById_ReturnsCorrectLayer)
{
    Dia::Core::Containers::DynamicArrayC<LayerDef, 32> layers;
    LayerDef mg; mg.id = StringCRC("midground"); mg.sortOrder = 5; layers.Add(mg);
    LayerTable table;
    table.Build(layers);

    const LayerDef& found = table.GetById(StringCRC("midground"));
    EXPECT_EQ(found.id, StringCRC("midground"));
    EXPECT_EQ(found.sortOrder, 5);
}

TEST(DiaScene2D_LayerTable, EmptyLayers_JustDefault)
{
    Dia::Core::Containers::DynamicArrayC<LayerDef, 32> layers;
    LayerTable table;
    table.Build(layers);
    EXPECT_EQ(table.GetCount(), 1u);
    EXPECT_EQ(table.GetBitIndex(StringCRC("default")), 0u);
}

TEST(DiaScene2D_LayerTable, MaxCapacity_32Layers_NoOverflow)
{
    Dia::Core::Containers::DynamicArrayC<LayerDef, 32> layers;
    // Fill all 32 slots — first is "default" so add 31 named layers
    LayerDef def; def.id = StringCRC("default"); layers.Add(def);
    for (int i = 1; i < 32; ++i)
    {
        char name[32];
        snprintf(name, sizeof(name), "layer_%d", i);
        LayerDef l; l.id = StringCRC(name); layers.Add(l);
    }

    LayerTable table;
    table.Build(layers);
    EXPECT_EQ(table.GetCount(), 32u);
}
