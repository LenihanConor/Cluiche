////////////////////////////////////////////////////////////////////////////////
// TestCalloutRegistryDebugger.cpp
// Mandatory IDebugDomain contract shapes + domain-specific shapes for
// CalloutRegistryDebugger.
//
//   Suite CalloutRegistryDebugger_Identity  — identity, HasWorldDrawers, drawers
//   Suite CalloutRegistryDebugger_JSONState — JSON schema, live/claim stats, entries
//   Suite CalloutRegistryDebugger_OnCommand — toggle, malformed, unknown
//
// Feature spec: docs/specs/applications/dia/systems/diaaibroadcast/diaaibroadcast.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaAICalloutVisualDebugger/CalloutRegistryDebugger.h>
#include <DiaAICallout/CalloutRegistry.h>
#include <DiaAICallout/Callout.h>
#include <DiaDebugDraw/Domain/DebugGroupAccents.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaMaths/Vector/Vector2D.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{

Json::Value GetState(Dia::AICalloutVisualDebugger::CalloutRegistryDebugger& domain)
{
    Json::Value out;
    domain.GetJSONState(out);
    return out;
}

Json::Value ToggleArgs(const char* drawerName)
{
    Json::Value args(Json::objectValue);
    args["drawer"] = drawerName;
    return args;
}

// Emit a live callout to the registry
void EmitTestCallout(Dia::AICallout::CalloutRegistry& registry,
                     Dia::Core::StringCRC kind,
                     Dia::Maths::Vector2D position,
                     float radius = 50.0f,
                     float ttl = 10.0f)
{
    Dia::AICallout::Callout c;
    c.kind     = kind;
    c.position = position;
    c.radius   = radius;
    c.faction  = Dia::Core::StringCRC::kZero;
    c.ttl      = ttl;
    registry.Emit(c);
}

} // anonymous namespace

// ===========================================================================
// Identity
// ===========================================================================

TEST(CalloutRegistryDebugger_Identity, HasWorldDrawers_True)
{
    Dia::AICallout::CalloutRegistry registry;
    Dia::AICalloutVisualDebugger::CalloutRegistryDebugger domain(registry);

    EXPECT_TRUE(domain.HasWorldDrawers());
}

TEST(CalloutRegistryDebugger_Identity, GetDrawerCount_One)
{
    Dia::AICallout::CalloutRegistry registry;
    Dia::AICalloutVisualDebugger::CalloutRegistryDebugger domain(registry);

    EXPECT_EQ(domain.GetDrawerCount(), 1);
}

TEST(CalloutRegistryDebugger_Identity, GetDrawer_Index0_NonNull)
{
    Dia::AICallout::CalloutRegistry registry;
    Dia::AICalloutVisualDebugger::CalloutRegistryDebugger domain(registry);

    EXPECT_NE(domain.GetDrawer(0), nullptr);
}

TEST(CalloutRegistryDebugger_Identity, GetDrawer_OutOfRange_Nullptr)
{
    Dia::AICallout::CalloutRegistry registry;
    Dia::AICalloutVisualDebugger::CalloutRegistryDebugger domain(registry);

    EXPECT_EQ(domain.GetDrawer(1), nullptr);
}

TEST(CalloutRegistryDebugger_Identity, DomainId_And_Group_Canonical)
{
    Dia::AICallout::CalloutRegistry registry;
    Dia::AICalloutVisualDebugger::CalloutRegistryDebugger domain(registry);

    EXPECT_EQ(domain.GetDomainId(), Dia::Core::StringCRC("aicallout"));
    EXPECT_EQ(domain.GetGroup(),    Dia::Core::StringCRC("AIBehavior"));
}

TEST(CalloutRegistryDebugger_Identity, AccentIsAIBehaviorConstant)
{
    Dia::AICallout::CalloutRegistry registry;
    Dia::AICalloutVisualDebugger::CalloutRegistryDebugger domain(registry);

    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kAIBehavior);
}

TEST(CalloutRegistryDebugger_Identity, DescriptionWithin80Chars)
{
    Dia::AICallout::CalloutRegistry registry;
    Dia::AICalloutVisualDebugger::CalloutRegistryDebugger domain(registry);

    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(strlen(domain.GetDescription()), 80u);
}

// ===========================================================================
// JSON state — mandatory contract shapes
// ===========================================================================

TEST(CalloutRegistryDebugger_JSONState, JSONRoundTrip_HasRequiredKeys)
{
    Dia::AICallout::CalloutRegistry registry;
    Dia::AICalloutVisualDebugger::CalloutRegistryDebugger domain(registry);

    const Json::Value state = GetState(domain);

    ASSERT_TRUE(state.isMember("drawers"))  << "drawers key required";
    ASSERT_TRUE(state.isMember("stats"))    << "stats key required";
    ASSERT_TRUE(state.isMember("callouts")) << "callouts key required";

    EXPECT_TRUE(state["drawers"].isArray());
    EXPECT_TRUE(state["stats"].isObject());
    EXPECT_TRUE(state["callouts"].isArray());
}

TEST(CalloutRegistryDebugger_JSONState, DrawersArray_OneEntry_CalloutRadii)
{
    Dia::AICallout::CalloutRegistry registry;
    Dia::AICalloutVisualDebugger::CalloutRegistryDebugger domain(registry);

    const Json::Value state = GetState(domain);

    ASSERT_EQ(state["drawers"].size(), 1u);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "CalloutRadii");
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
}

TEST(CalloutRegistryDebugger_JSONState, Stats_EmptyRegistry_AllZero)
{
    Dia::AICallout::CalloutRegistry registry;
    Dia::AICalloutVisualDebugger::CalloutRegistryDebugger domain(registry);

    const Json::Value state = GetState(domain);

    EXPECT_EQ(state["stats"]["liveCount"].asInt(),    0);
    EXPECT_EQ(state["stats"]["claimedCount"].asInt(), 0);
}

TEST(CalloutRegistryDebugger_JSONState, Stats_LiveCount_AfterEmit)
{
    Dia::AICallout::CalloutRegistry registry;
    Dia::AICalloutVisualDebugger::CalloutRegistryDebugger domain(registry);

    EmitTestCallout(registry, Dia::Core::StringCRC("HelpNeeded"),
                    Dia::Maths::Vector2D(0.f, 0.f));

    const Json::Value state = GetState(domain);

    EXPECT_EQ(state["stats"]["liveCount"].asInt(), 1);
}

TEST(CalloutRegistryDebugger_JSONState, Stats_ClaimedCount_AfterClaim)
{
    Dia::AICallout::CalloutRegistry registry;
    Dia::AICalloutVisualDebugger::CalloutRegistryDebugger domain(registry);

    Dia::AICallout::Callout c;
    c.kind     = Dia::Core::StringCRC("HelpNeeded");
    c.position = Dia::Maths::Vector2D(0.f, 0.f);
    c.radius   = 50.f;
    c.faction  = Dia::Core::StringCRC::kZero;
    c.ttl      = 10.f;
    Dia::AICallout::CalloutHandle handle = registry.Emit(c);
    registry.Claim(handle, Dia::Core::StringCRC("entityA"));

    const Json::Value state = GetState(domain);

    EXPECT_EQ(state["stats"]["claimedCount"].asInt(), 1);
}

TEST(CalloutRegistryDebugger_JSONState, Callouts_EmptyRegistry_EmptyArray)
{
    Dia::AICallout::CalloutRegistry registry;
    Dia::AICalloutVisualDebugger::CalloutRegistryDebugger domain(registry);

    const Json::Value state = GetState(domain);

    EXPECT_EQ(state["callouts"].size(), 0u);
}

TEST(CalloutRegistryDebugger_JSONState, Callouts_OneEntry_HasRequiredFields)
{
    Dia::AICallout::CalloutRegistry registry;
    Dia::AICalloutVisualDebugger::CalloutRegistryDebugger domain(registry);

    EmitTestCallout(registry, Dia::Core::StringCRC("HelpNeeded"),
                    Dia::Maths::Vector2D(1.f, 2.f));

    const Json::Value state = GetState(domain);

    ASSERT_EQ(state["callouts"].size(), 1u);
    const Json::Value& entry = state["callouts"][0u];

    EXPECT_TRUE(entry.isMember("kind"))    << "kind field required";
    EXPECT_TRUE(entry.isMember("x"))       << "x field required";
    EXPECT_TRUE(entry.isMember("y"))       << "y field required";
    EXPECT_TRUE(entry.isMember("radius"))  << "radius field required";
    EXPECT_TRUE(entry.isMember("faction")) << "faction field required";
    EXPECT_TRUE(entry.isMember("ttl"))     << "ttl field required";
    EXPECT_TRUE(entry.isMember("claimed")) << "claimed field required";
}

TEST(CalloutRegistryDebugger_JSONState, Callouts_Claimed_ReflectsClaimState)
{
    Dia::AICallout::CalloutRegistry registry;
    Dia::AICalloutVisualDebugger::CalloutRegistryDebugger domain(registry);

    Dia::AICallout::Callout c;
    c.kind     = Dia::Core::StringCRC("HelpNeeded");
    c.position = Dia::Maths::Vector2D(0.f, 0.f);
    c.radius   = 50.f;
    c.faction  = Dia::Core::StringCRC::kZero;
    c.ttl      = 10.f;
    Dia::AICallout::CalloutHandle handle = registry.Emit(c);
    registry.Claim(handle, Dia::Core::StringCRC("entityA"));

    const Json::Value state = GetState(domain);

    ASSERT_EQ(state["callouts"].size(), 1u);
    EXPECT_TRUE(state["callouts"][0u]["claimed"].asBool());
}

TEST(CalloutRegistryDebugger_JSONState, Callouts_OnlyLiveEntries)
{
    Dia::AICallout::CalloutRegistry registry;
    Dia::AICalloutVisualDebugger::CalloutRegistryDebugger domain(registry);

    // Emit with ttl=1, then advance by dt=2 — slot expires
    EmitTestCallout(registry, Dia::Core::StringCRC("HelpNeeded"),
                    Dia::Maths::Vector2D(0.f, 0.f), 50.f, 1.f);
    registry.Update(2.f);

    const Json::Value state = GetState(domain);

    EXPECT_EQ(state["callouts"].size(), 0u);
}

// ===========================================================================
// OnCommand
// ===========================================================================

TEST(CalloutRegistryDebugger_OnCommand, Toggle_CalloutRadii_DisablesDrawer)
{
    Dia::AICallout::CalloutRegistry registry;
    Dia::AICalloutVisualDebugger::CalloutRegistryDebugger domain(registry);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("CalloutRadii"));

    const Json::Value state = GetState(domain);
    EXPECT_FALSE(state["drawers"][0u]["enabled"].asBool());
}

TEST(CalloutRegistryDebugger_OnCommand, Toggle_CalloutRadii_Twice_Restores)
{
    Dia::AICallout::CalloutRegistry registry;
    Dia::AICalloutVisualDebugger::CalloutRegistryDebugger domain(registry);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("CalloutRadii"));
    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("CalloutRadii"));

    const Json::Value state = GetState(domain);
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
}

TEST(CalloutRegistryDebugger_OnCommand, UnknownCommand_NoOp_NoCrash)
{
    Dia::AICallout::CalloutRegistry registry;
    Dia::AICalloutVisualDebugger::CalloutRegistryDebugger domain(registry);

    Json::Value empty(Json::objectValue);
    EXPECT_NO_FATAL_FAILURE(
        domain.OnCommand(Dia::Core::StringCRC("unknown"), empty));
}

TEST(CalloutRegistryDebugger_OnCommand, MalformedToggle_NoOp_NoCrash)
{
    Dia::AICallout::CalloutRegistry registry;
    Dia::AICalloutVisualDebugger::CalloutRegistryDebugger domain(registry);

    Json::Value empty(Json::objectValue);
    EXPECT_NO_FATAL_FAILURE(
        domain.OnCommand(Dia::Core::StringCRC("toggle"), empty));

    // State must be unchanged — drawer still enabled
    const Json::Value state = GetState(domain);
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
}

#endif // DIA_DEBUG
