////////////////////////////////////////////////////////////////////////////////
// TestAssetRuntimeDebugDomain.cpp
// AC-15 mandatory test shapes for the AssetRuntime IDebugDomain migration:
//   1. enable/disable gate per drawer
//   2. each drawer emits its expected primitive type
//   3. scale sensitivity (GetDebugScale changes output measurements)
//   4. GetJSONState() round-trip
//   5. OnCommand("toggle", ...) round-trip
//
// Note: DiaAssetRuntimeVisualDebugger::Draw() is a no-op (ImGui-only output).
//       Tests 1 and 2 therefore verify 0 world primitives are emitted.
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaAssetRuntimeVisualDebugger/AssetRuntimeDebugDomain.h>

#include <DiaGraphics/Frame/FrameData.h>
#include <DiaGraphics/Testing/MockVisitors.h>
#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/DebugLayerNames.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>

using namespace Dia::AssetRuntime;
using namespace Dia::Graphics;
using namespace Dia::Graphics::Testing;

namespace
{

RecordingDebugVisitor Inspect(const FrameData& fd)
{
    RecordingDebugVisitor v;
    static_cast<const Dia::Graphics::DebugFrameData&>(fd).AcceptVisitor(v);
    return v;
}

Json::Value ToggleArgs(const char* drawerName)
{
    Json::Value args;
    args["drawer"] = drawerName;
    return args;
}

} // namespace

// ===========================================================================
// Identity (AC-5: description <=80 chars, AC-6: accent from DebugGroupAccents)
// ===========================================================================

TEST(AssetRuntimeDebugDomain_Identity, IdsAndGroupAreCanonical)
{
    AssetRuntimeDebugDomain domain;

    EXPECT_EQ(domain.GetDomainId(), Dia::Core::StringCRC("AssetRuntime"));
    EXPECT_STREQ(domain.GetDisplayName(), "AssetRuntime");
    EXPECT_EQ(domain.GetGroup(), Dia::Core::StringCRC("CoreDebug"));
    EXPECT_TRUE(domain.HasWorldDrawers());
}

TEST(AssetRuntimeDebugDomain_Identity, DescriptionWithin80Chars)
{
    AssetRuntimeDebugDomain domain;

    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(strlen(domain.GetDescription()), 80u);
}

TEST(AssetRuntimeDebugDomain_Identity, AccentIsCoreDebugGroupConstant)
{
    AssetRuntimeDebugDomain domain;

    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kCoreDebug);
}

// ===========================================================================
// Registration lifecycle
// ===========================================================================

TEST(AssetRuntimeDebugDomain_Lifecycle, DrawersOnlyExistAfterRegister)
{
    Dia::Debug::DebugLayerManager mgr;
    AssetRuntimeDebugDomain domain;

    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_EQ(domain.GetDrawer(0), nullptr);

    domain.Register(mgr);

    EXPECT_EQ(domain.GetDrawerCount(), AssetRuntimeDebugDomain::kDrawerCount);
    for (int i = 0; i < AssetRuntimeDebugDomain::kDrawerCount; ++i)
        EXPECT_NE(domain.GetDrawer(i), nullptr) << "drawer index " << i;
    EXPECT_EQ(domain.GetDrawer(AssetRuntimeDebugDomain::kDrawerCount), nullptr);
}

TEST(AssetRuntimeDebugDomain_Lifecycle, RegisterAddsAssetRuntimeLayer)
{
    Dia::Debug::DebugLayerManager mgr;
    AssetRuntimeDebugDomain domain;
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), AssetRuntimeDebugDomain::kDrawerCount);
    EXPECT_TRUE(mgr.HasLayer(Dia::Debug::LayerNames::kAssetRuntime));
}

TEST(AssetRuntimeDebugDomain_Lifecycle, LayersCarryTheAssetRuntimeStageTag)
{
    Dia::Debug::DebugLayerManager mgr;
    AssetRuntimeDebugDomain domain;
    domain.Register(mgr);

    EXPECT_TRUE(mgr.IsStageActive(Dia::Core::StringCRC("AssetRuntime")));
}

TEST(AssetRuntimeDebugDomain_Lifecycle, UnregisterRemovesAllLayersAndDrawers)
{
    Dia::Debug::DebugLayerManager mgr;
    AssetRuntimeDebugDomain domain;
    domain.Register(mgr);
    domain.Unregister(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), 0);
    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_FALSE(mgr.HasLayer(Dia::Debug::LayerNames::kAssetRuntime));
}

TEST(AssetRuntimeDebugDomain_Lifecycle, DoubleRegisterIsIdempotent)
{
    Dia::Debug::DebugLayerManager mgr;
    AssetRuntimeDebugDomain domain;
    domain.Register(mgr);
    domain.Register(mgr);

    EXPECT_EQ(mgr.GetLayerCount(), AssetRuntimeDebugDomain::kDrawerCount);
}

// ===========================================================================
// AC-15 #1 — enable/disable gate
// (DiaAssetRuntimeVisualDebugger::Draw() is a no-op — no world primitives.)
// ===========================================================================

TEST(AssetRuntimeDebugDomain_DrawerGate, EnabledDrawerEmitsNoWorldPrimitives)
{
    Dia::Debug::DebugLayerManager mgr;
    AssetRuntimeDebugDomain domain;
    domain.Register(mgr);

    FrameData enabledFrame;
    mgr.Draw(enabledFrame);
    EXPECT_EQ(Inspect(enabledFrame).TotalCount(), 0)
        << "DiaAssetRuntimeVisualDebugger is ImGui-only: no world primitives when enabled";
}

TEST(AssetRuntimeDebugDomain_DrawerGate, DisabledDrawerEmitsNoWorldPrimitives)
{
    Dia::Debug::DebugLayerManager mgr;
    AssetRuntimeDebugDomain domain;
    domain.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kAssetRuntime);

    FrameData disabledFrame;
    mgr.Draw(disabledFrame);
    EXPECT_EQ(Inspect(disabledFrame).TotalCount(), 0);
}

// ===========================================================================
// AC-15 #2 — each drawer emits its expected primitive type
// (ImGui-only: expected type count is 0 for all world-space primitive types.)
// ===========================================================================

TEST(AssetRuntimeDebugDomain_Primitives, DrawerEmitsNoWorldPrimitives)
{
    Dia::Debug::DebugLayerManager mgr;
    AssetRuntimeDebugDomain domain;
    domain.Register(mgr);

    FrameData fd;
    mgr.Draw(fd);
    auto v = Inspect(fd);

    EXPECT_EQ(v.CircleCount(), 0) << "DiaAssetRuntimeVisualDebugger has no world-space circles";
    EXPECT_EQ(v.LineCount(),   0) << "DiaAssetRuntimeVisualDebugger has no world-space lines";
    EXPECT_EQ(v.RectCount(),   0) << "DiaAssetRuntimeVisualDebugger has no world-space rects";
    EXPECT_EQ(v.RayCount(),    0) << "DiaAssetRuntimeVisualDebugger has no world-space rays";
}

// ===========================================================================
// AC-15 #3 — scale sensitivity
// (No geometric output to measure; verify setScale is forwarded without crash.)
// ===========================================================================

TEST(AssetRuntimeDebugDomain_Scale, SetScaleUpdatesManagerDebugScale)
{
    Dia::Debug::DebugLayerManager mgr;
    AssetRuntimeDebugDomain domain;
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 2.5;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 2.5f);
}

TEST(AssetRuntimeDebugDomain_Scale, SetScaleWithUnknownKeyIsIgnored)
{
    Dia::Debug::DebugLayerManager mgr;
    AssetRuntimeDebugDomain domain;
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

TEST(AssetRuntimeDebugDomain_JSONState, ReportsDrawerAndStatsObject)
{
    Dia::Debug::DebugLayerManager mgr;
    AssetRuntimeDebugDomain domain;
    domain.Register(mgr);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(),
              static_cast<Json::ArrayIndex>(AssetRuntimeDebugDomain::kDrawerCount));
    ASSERT_TRUE(state.isMember("stats"));
    EXPECT_TRUE(state["stats"].isObject());

    for (Json::ArrayIndex i = 0; i < state["drawers"].size(); ++i)
    {
        const Json::Value& entry = state["drawers"][i];
        EXPECT_TRUE(entry.isMember("name"));
        EXPECT_FALSE(entry["name"].asString().empty());
        ASSERT_TRUE(entry.isMember("enabled"));
        EXPECT_TRUE(entry["enabled"].asBool()) << "drawers start enabled";
    }
}

TEST(AssetRuntimeDebugDomain_JSONState, EnabledFlagTracksLayerManager)
{
    Dia::Debug::DebugLayerManager mgr;
    AssetRuntimeDebugDomain domain;
    domain.Register(mgr);

    mgr.DisableLayer(Dia::Debug::LayerNames::kAssetRuntime);

    Json::Value state;
    domain.GetJSONState(state);
    ASSERT_EQ(state["drawers"].size(), 1u);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "AssetRuntime");
    EXPECT_FALSE(state["drawers"][0u]["enabled"].asBool());
}

TEST(AssetRuntimeDebugDomain_JSONState, BeforeRegisterAllDrawersReportDisabled)
{
    AssetRuntimeDebugDomain domain;

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(), 1u);
    for (Json::ArrayIndex i = 0; i < state["drawers"].size(); ++i)
        EXPECT_FALSE(state["drawers"][i]["enabled"].asBool());
}

// ===========================================================================
// AC-15 #5 — OnCommand("toggle") round-trip
// ===========================================================================

TEST(AssetRuntimeDebugDomain_OnCommand, TogglePanelLabelFlipsLayerTwice)
{
    Dia::Debug::DebugLayerManager mgr;
    AssetRuntimeDebugDomain domain;
    domain.Register(mgr);

    const Dia::Core::StringCRC assetLayer = Dia::Debug::LayerNames::kAssetRuntime;
    ASSERT_TRUE(mgr.IsLayerEnabled(assetLayer));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("AssetRuntime"));
    EXPECT_FALSE(mgr.IsLayerEnabled(assetLayer));

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("AssetRuntime"));
    EXPECT_TRUE(mgr.IsLayerEnabled(assetLayer));
}

TEST(AssetRuntimeDebugDomain_OnCommand, ToggleAcceptsRawLayerName)
{
    Dia::Debug::DebugLayerManager mgr;
    AssetRuntimeDebugDomain domain;
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("asset.runtime"));
    EXPECT_FALSE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kAssetRuntime));
}

TEST(AssetRuntimeDebugDomain_OnCommand, UnknownDrawerNameIsIgnored)
{
    Dia::Debug::DebugLayerManager mgr;
    AssetRuntimeDebugDomain domain;
    domain.Register(mgr);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("NoSuchDrawer"));

    for (int i = 0; i < AssetRuntimeDebugDomain::kDrawerCount; ++i)
        EXPECT_TRUE(mgr.IsLayerEnabled(domain.GetDrawer(i)->GetLayerName()));
}

TEST(AssetRuntimeDebugDomain_OnCommand, MalformedAndUnknownCommandsAreIgnored)
{
    Dia::Debug::DebugLayerManager mgr;
    AssetRuntimeDebugDomain domain;
    domain.Register(mgr);

    Json::Value empty(Json::objectValue);
    domain.OnCommand(Dia::Core::StringCRC("toggle"), empty);
    domain.OnCommand(Dia::Core::StringCRC("notACommand"), empty);

    Json::Value wrongType(Json::objectValue);
    wrongType["drawer"] = 42;
    domain.OnCommand(Dia::Core::StringCRC("toggle"), wrongType);

    EXPECT_TRUE(mgr.IsLayerEnabled(Dia::Debug::LayerNames::kAssetRuntime));
    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 1.0f);
}

TEST(AssetRuntimeDebugDomain_OnCommand, BeforeRegisterCommandsAreNoOps)
{
    Dia::Debug::DebugLayerManager mgr;
    AssetRuntimeDebugDomain domain;

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("AssetRuntime"));
    EXPECT_EQ(mgr.GetLayerCount(), 0);
}

TEST(AssetRuntimeDebugDomain_OnCommand, SetScaleUpdatesSharedDebugScale)
{
    Dia::Debug::DebugLayerManager mgr;
    AssetRuntimeDebugDomain domain;
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "debugScale";
    args["value"] = 2.5;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 2.5f);
}

TEST(AssetRuntimeDebugDomain_OnCommand, SetScaleWithUnknownKeyIsIgnored)
{
    Dia::Debug::DebugLayerManager mgr;
    AssetRuntimeDebugDomain domain;
    domain.Register(mgr);

    Json::Value args;
    args["key"]   = "notAKnownKey";
    args["value"] = 3.0;
    domain.OnCommand(Dia::Core::StringCRC("setScale"), args);

    EXPECT_FLOAT_EQ(mgr.GetDebugScale(), 1.0f);
}

#endif // DIA_DEBUG
