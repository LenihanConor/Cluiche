////////////////////////////////////////////////////////////////////////////////
// TestMesh3DDebugDomain.cpp
// AC-15 mandatory test shapes for the Mesh3D IDebugDomain migration:
//   1. enable/disable gate per drawer
//   2. each drawer emits its expected primitive type
//   3. scale sensitivity (OnCommand setScale)
//   4. GetJSONState() round-trip
//   5. OnCommand("toggle", ...) round-trip
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaMesh3DVisualDebugger/Mesh3DDebugDomain.h>

#include <DiaGraphics3D/Mesh3DFrameData.h>
#include <DiaMesh3D/Mesh3DAssetHandler.h>
#include <DiaMesh3D/Mesh3DAsset.h>
#include <DiaGeometry3D/Shapes/AABB.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <DiaGraphics3D/Mesh3DDrawCommand.h>
#include <DiaMaths/Matrix/Matrix44.h>

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaDebugDraw/Domain/DebugGroupAccents.h>

#include <DiaGraphics/Frame/FrameData.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/CRC/CRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>

using namespace Dia::Mesh3D;

namespace
{

Json::Value ToggleArgs(const char* drawerName)
{
    Json::Value args;
    args["drawer"] = drawerName;
    return args;
}

void RegisterReadyAsset(Dia::Mesh3D::Mesh3DAssetHandler& handler, const char* id)
{
    auto* asset = new Dia::Mesh3D::Mesh3DAsset(Dia::Core::StringCRC(id));
    Dia::Mesh3D::Vertex3D v{};
    v.position = Dia::Maths::Vector3D(0.5f, 0.5f, 0.5f);
    v.colour   = 0xFFFFFFFF;
    Dia::Mesh3D::Vertex3D verts[3] = { v, v, v };
    uint16_t idxs[3] = { 0, 1, 2 };
    Dia::Mesh3D::Submesh sub;
    sub.indexStart = 0;
    sub.indexCount = 3;
    sub.materialId = Dia::Core::CRC("mat.default");
    Dia::Geometry3D::AABB bounds(
        Dia::Maths::Vector3D(-0.5f, -0.5f, -0.5f),
        Dia::Maths::Vector3D( 0.5f,  0.5f,  0.5f));
    asset->Populate(verts, 3, idxs, 3, &sub, 1, bounds);
    handler.RegisterMesh(asset);
}

Dia::Graphics3D::Mesh3DDrawCommand MakeCmd(const char* meshId)
{
    Dia::Graphics3D::Mesh3DDrawCommand cmd;
    cmd.meshId               = Dia::Core::StringCRC(meshId);
    cmd.materialId           = Dia::Core::StringCRC("mat.default");
    cmd.transform            = Dia::Maths::Matrix44::Identity();
    cmd.skinningPaletteIndex = 0;
    cmd.layer                = 0;
    return cmd;
}

} // namespace

// ===========================================================================
// Identity
// ===========================================================================

TEST(Mesh3DDebugDomain_Identity, IdsAndGroupAreCanonical)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    Mesh3DDebugDomain domain(frameData, handler);

    EXPECT_EQ(domain.GetDomainId(), Dia::Core::StringCRC("Mesh3D"));
    EXPECT_STREQ(domain.GetDisplayName(), "Mesh3D");
    EXPECT_EQ(domain.GetGroup(), Dia::Core::StringCRC("Rendering"));
    EXPECT_TRUE(domain.HasWorldDrawers());
}

TEST(Mesh3DDebugDomain_Identity, DescriptionWithin80Chars)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    Mesh3DDebugDomain domain(frameData, handler);

    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(strlen(domain.GetDescription()), 80u);
}

TEST(Mesh3DDebugDomain_Identity, AccentIsRenderingGroupConstant)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    Mesh3DDebugDomain domain(frameData, handler);

    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kRendering);
}

// ===========================================================================
// Registration lifecycle
// ===========================================================================

TEST(Mesh3DDebugDomain_Lifecycle, DrawersOnlyExistAfterRegister)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    Dia::Debug::DebugLayerManager    mgr;
    Mesh3DDebugDomain domain(frameData, handler);

    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_EQ(domain.GetDrawer(0), nullptr);

    domain.Register(mgr);

    EXPECT_EQ(domain.GetDrawerCount(), Mesh3DDebugDomain::kDrawerCount);
    for (int i = 0; i < Mesh3DDebugDomain::kDrawerCount; ++i)
        EXPECT_NE(domain.GetDrawer(i), nullptr) << "drawer " << i;
    EXPECT_EQ(domain.GetDrawer(Mesh3DDebugDomain::kDrawerCount), nullptr);
}

TEST(Mesh3DDebugDomain_Lifecycle, RegisterAddsAllThreeLayers)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    Dia::Debug::DebugLayerManager    mgr;
    Mesh3DDebugDomain domain(frameData, handler);
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), Mesh3DDebugDomain::kDrawerCount);
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kMesh3DBounds));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kMesh3DOrigins));
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kMesh3DStats));
}

TEST(Mesh3DDebugDomain_Lifecycle, LayersCarryTheMesh3DStageTag)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    Dia::Debug::DebugLayerManager    mgr;
    Mesh3DDebugDomain domain(frameData, handler);
    domain.Register(mgr);

    EXPECT_TRUE(mgr.IsStageActive(Dia::Core::StringCRC("Mesh3D")));
}

TEST(Mesh3DDebugDomain_Lifecycle, UnregisterRemovesAllLayersAndDrawers)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    Dia::Debug::DebugLayerManager    mgr;
    Mesh3DDebugDomain domain(frameData, handler);
    domain.Register(mgr);
    domain.Unregister(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), 0);
    EXPECT_EQ(domain.GetDrawerCount(), 0);
}

TEST(Mesh3DDebugDomain_Lifecycle, DoubleRegisterIsIdempotent)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    Dia::Debug::DebugLayerManager    mgr;
    Mesh3DDebugDomain domain(frameData, handler);
    domain.Register(mgr);
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), Mesh3DDebugDomain::kDrawerCount);
}

// ===========================================================================
// AC-15 #1 — enable/disable gate
// ===========================================================================

TEST(Mesh3DDebugDomain_DrawerGate, DisabledBoundsDrawerEmitsNoPrimitives)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    RegisterReadyAsset(handler, "cube");
    frameData.RequestDrawMesh(MakeCmd("cube"));

    Dia::Debug::DebugLayerManager mgr;
    Mesh3DDebugDomain domain(frameData, handler);
    domain.Register(mgr);

    Dia::Graphics::FrameData enabled;
    mgr.Draw(enabled);
    ASSERT_GE(enabled.GetDebug3DPrimitiveCount(), 1u) << "bounds drawer emits when enabled";

    mgr.DisableLayer(Dia::Debug::LayerNames::kMesh3DBounds);
    mgr.DisableLayer(Dia::Debug::LayerNames::kMesh3DOrigins);
    mgr.DisableLayer(Dia::Debug::LayerNames::kMesh3DStats);

    Dia::Graphics::FrameData disabled;
    mgr.Draw(disabled);
    EXPECT_EQ(disabled.GetDebug3DPrimitiveCount(), 0u);
}

// ===========================================================================
// AC-15 #2 — each drawer emits its expected primitive type
// ===========================================================================

TEST(Mesh3DDebugDomain_Primitives, BoundsDrawerEmits3DLinesForReadyMesh)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    RegisterReadyAsset(handler, "cube.bounds");
    frameData.RequestDrawMesh(MakeCmd("cube.bounds"));

    Dia::Debug::DebugLayerManager mgr;
    Mesh3DDebugDomain domain(frameData, handler);
    domain.Register(mgr);

    // Isolate bounds drawer.
    mgr.DisableLayer(Dia::Debug::LayerNames::kMesh3DOrigins);
    mgr.DisableLayer(Dia::Debug::LayerNames::kMesh3DStats);

    Dia::Graphics::FrameData fd;
    mgr.Draw(fd);
    EXPECT_EQ(fd.GetDebug3DPrimitiveCount(), 12u) << "AABB = 12 edges";
}

TEST(Mesh3DDebugDomain_Primitives, EmptyFrameDataEmitsNoPrimitives)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;  // no draw commands
    Dia::Mesh3D::Mesh3DAssetHandler  handler;

    Dia::Debug::DebugLayerManager mgr;
    Mesh3DDebugDomain domain(frameData, handler);
    domain.Register(mgr);

    Dia::Graphics::FrameData fd;
    mgr.Draw(fd);
    EXPECT_EQ(fd.GetDebug3DPrimitiveCount(), 0u);
}

// ===========================================================================
// AC-15 #3 — scale sensitivity
// ===========================================================================

TEST(Mesh3DDebugDomain_Scale, SetScaleUpdatesManagerDebugScale)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    Dia::Debug::DebugLayerManager    mgr;
    Mesh3DDebugDomain domain(frameData, handler);
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 4.0;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 4.0f);
}

TEST(Mesh3DDebugDomain_Scale, SetScaleWithUnknownKeyIsIgnored)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    Dia::Debug::DebugLayerManager    mgr;
    Mesh3DDebugDomain domain(frameData, handler);
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "notAKnownKey";
    args["value"] = 3.0;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 1.0f);
}

// ===========================================================================
// AC-15 #4 — GetJSONState round-trip
// ===========================================================================

TEST(Mesh3DDebugDomain_JSONState, ReportsThreeDrawersAndAStatsObject)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    Dia::Debug::DebugLayerManager    mgr;
    Mesh3DDebugDomain domain(frameData, handler);
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(),
              static_cast<Json::ArrayIndex>(Mesh3DDebugDomain::kDrawerCount));
    ASSERT_TRUE(state.isMember("stats"));
    EXPECT_TRUE(state["stats"].isObject());

    for (Json::ArrayIndex i = 0; i < state["drawers"].size(); ++i)
    {
        EXPECT_FALSE(state["drawers"][i]["name"].asString().empty());
        EXPECT_TRUE(state["drawers"][i]["enabled"].asBool()) << "drawers start enabled";
    }
}

TEST(Mesh3DDebugDomain_JSONState, EnabledFlagTracksLayerManager)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    Dia::Debug::DebugLayerManager    mgr;
    Mesh3DDebugDomain domain(frameData, handler);
    domain.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kMesh3DBounds);

    Json::Value state;
    domain.GetJSONState(state);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "Bounds");
    EXPECT_FALSE(state["drawers"][0u]["enabled"].asBool());
    EXPECT_TRUE(state["drawers"][1u]["enabled"].asBool());
}

TEST(Mesh3DDebugDomain_JSONState, BeforeRegisterAllDrawersReportDisabled)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    Mesh3DDebugDomain domain(frameData, handler);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_EQ(state["drawers"].size(), 3u);
    for (Json::ArrayIndex i = 0; i < state["drawers"].size(); ++i)
        EXPECT_FALSE(state["drawers"][i]["enabled"].asBool());
}

// ===========================================================================
// AC-15 #5 — OnCommand("toggle") round-trip
// ===========================================================================

TEST(Mesh3DDebugDomain_OnCommand, TogglePanelLabelFlipsLayerTwice)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    Dia::Debug::DebugLayerManager    mgr;
    Mesh3DDebugDomain domain(frameData, handler);
    domain.Register(mgr);

    const Dia::Core::StringCRC bounds = Dia::Debug::LayerNames::kMesh3DBounds;
    ASSERT_TRUE(mgr.IsLayerEnabled(bounds));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Bounds"));
    EXPECT_FALSE(mgr.IsLayerEnabled(bounds));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Bounds"));
    EXPECT_TRUE(mgr.IsLayerEnabled(bounds));
}

TEST(Mesh3DDebugDomain_OnCommand, ToggleAcceptsRawLayerName)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    Dia::Debug::DebugLayerManager    mgr;
    Mesh3DDebugDomain domain(frameData, handler);
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("mesh3d.origins"));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kMesh3DOrigins));
}

TEST(Mesh3DDebugDomain_OnCommand, ToggleTouchesOnlyTheNamedDrawer)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    Dia::Debug::DebugLayerManager    mgr;
    Mesh3DDebugDomain domain(frameData, handler);
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Stats"));

    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kMesh3DStats));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kMesh3DBounds));
    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kMesh3DOrigins));
}

TEST(Mesh3DDebugDomain_OnCommand, BeforeRegisterCommandsAreNoOps)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    Dia::Debug::DebugLayerManager    mgr;
    Mesh3DDebugDomain domain(frameData, handler);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Bounds"));
    EXPECT_EQ(mgr.GetLayerCount(), 0);
}

// ===========================================================================
// Stats fields — GetJSONState() mesh draw stat assertions
// ===========================================================================

TEST(Mesh3DDebugDomain_JSONState_Stats, DrawStatFieldsPresent)
{
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    Dia::Debug::DebugLayerManager    mgr;
    Mesh3DDebugDomain domain(frameData, handler);
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    const Json::Value& stats = state["stats"];
    ASSERT_TRUE(stats.isObject());
    EXPECT_TRUE(stats.isMember("draws"))    << "stats.draws missing";
    EXPECT_TRUE(stats.isMember("dropped"))  << "stats.dropped missing";
    EXPECT_TRUE(stats.isMember("loaded"))   << "stats.loaded missing";
    EXPECT_TRUE(stats.isMember("skinned"))  << "stats.skinned missing";
    EXPECT_TRUE(stats.isMember("static"))   << "stats.static missing";
    EXPECT_TRUE(stats.isMember("ready"))    << "stats.ready missing";
    EXPECT_TRUE(stats.isMember("pending"))  << "stats.pending missing";
    EXPECT_TRUE(stats.isMember("failed"))   << "stats.failed missing";
    EXPECT_TRUE(stats.isMember("notFound")) << "stats.notFound missing";
    EXPECT_TRUE(stats.isMember("layers"))   << "stats.layers missing";
    EXPECT_TRUE(stats["layers"].isArray())  << "stats.layers must be array";
}

TEST(Mesh3DDebugDomain_JSONState_Stats, EmptyFrameDataReportsAllZeros)
{
    // No meshes queued — all counts zero, layers array empty.
    Dia::Graphics3D::Mesh3DFrameData frameData;
    Dia::Mesh3D::Mesh3DAssetHandler  handler;
    Dia::Debug::DebugLayerManager    mgr;
    Mesh3DDebugDomain domain(frameData, handler);
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    const Json::Value& stats = state["stats"];
    EXPECT_EQ(stats["draws"].asInt(),    0);
    EXPECT_EQ(stats["dropped"].asInt(),  0);
    EXPECT_EQ(stats["loaded"].asInt(),   0);
    EXPECT_EQ(stats["layers"].size(),    0u);
}

#endif // DIA_DEBUG
