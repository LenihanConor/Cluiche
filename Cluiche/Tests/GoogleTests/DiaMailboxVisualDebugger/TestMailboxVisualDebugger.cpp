////////////////////////////////////////////////////////////////////////////////
// TestMailboxVisualDebugger.cpp
// Mandatory IDebugDomain contract shapes + domain-specific shapes for
// MailboxVisualDebugger.
//
//   Suite MailboxVisualDebugger_Identity  — identity methods, no-drawers
//   Suite MailboxVisualDebugger_JSONState — JSON schema, drawer gates, type stats
//   Suite MailboxVisualDebugger_OnCommand — toggle, setScale no-op, malformed
//
// Feature spec: docs/specs/applications/dia/systems/diamailboxvisualdebugger/diamailboxvisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaMailboxVisualDebugger/MailboxVisualDebugger.h>
#include <DiaMailbox/Mailbox.h>
#include <DiaMailbox/Testing/TestMessages.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{

Json::Value GetState(Dia::Mailbox::MailboxVisualDebugger& domain)
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

void CauseDrop(Dia::Mailbox::Mailbox& mailbox)
{
    // Register TestMessageA with capacity 1, send 2 to force 1 drop
    mailbox.RegisterType<Dia::Mailbox::Testing::TestMessageA, 1>(Dia::Mailbox::OverflowPolicy::DropOldest);
    const Dia::Mailbox::Address addr;
    mailbox.Send(addr, Dia::Mailbox::Testing::TestMessageA{1});
    mailbox.Send(addr, Dia::Mailbox::Testing::TestMessageA{2}); // overflows → 1 drop
}

} // anonymous namespace

// ===========================================================================
// Identity
// ===========================================================================

TEST(MailboxVisualDebugger_Identity, PrimitiveType_NoWorldDrawers)
{
    Dia::Mailbox::Mailbox mailbox;
    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);

    EXPECT_FALSE(domain.HasWorldDrawers());
    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_EQ(domain.GetDrawer(0), nullptr);
}

TEST(MailboxVisualDebugger_Identity, DomainId_And_Group_Canonical)
{
    Dia::Mailbox::Mailbox mailbox;
    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);

    EXPECT_EQ(domain.GetDomainId(), Dia::Core::StringCRC("mailbox"));
    EXPECT_STREQ(domain.GetDisplayName(), "Mailbox");
    EXPECT_EQ(domain.GetGroup(), Dia::Core::StringCRC("AIBehavior"));
}

TEST(MailboxVisualDebugger_Identity, AccentIsAIBehaviorConstant)
{
    Dia::Mailbox::Mailbox mailbox;
    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);

    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kAIBehavior);
}

TEST(MailboxVisualDebugger_Identity, DescriptionWithin80Chars)
{
    Dia::Mailbox::Mailbox mailbox;
    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);

    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(strlen(domain.GetDescription()), 80u);
}

// ===========================================================================
// JSON state — mandatory contract shapes
// ===========================================================================

TEST(MailboxVisualDebugger_JSONState, JSONRoundTrip_HasRequiredKeys)
{
    Dia::Mailbox::Mailbox mailbox;
    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);

    const Json::Value state = GetState(domain);

    ASSERT_TRUE(state.isMember("drawers")) << "drawers key required";
    ASSERT_TRUE(state.isMember("stats"))   << "stats key required";

    EXPECT_TRUE(state["drawers"].isArray());
    EXPECT_TRUE(state["stats"].isObject());
}

TEST(MailboxVisualDebugger_JSONState, DrawersArray_HasBothEntries)
{
    Dia::Mailbox::Mailbox mailbox;
    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);

    const Json::Value state = GetState(domain);

    ASSERT_EQ(state["drawers"].size(), 2u);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "QueueTable");
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
    EXPECT_STREQ(state["drawers"][1u]["name"].asCString(), "DropAlerts");
    EXPECT_TRUE(state["drawers"][1u]["enabled"].asBool());
}

TEST(MailboxVisualDebugger_JSONState, DrawerGate_DisableQueueTable_RemovesTypesKey)
{
    Dia::Mailbox::Mailbox mailbox;
    mailbox.RegisterType<Dia::Mailbox::Testing::TestMessageA, 4>();
    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("QueueTable"));

    const Json::Value state = GetState(domain);
    EXPECT_FALSE(state.isMember("types")) << "types key must be absent when QueueTable disabled";
    EXPECT_FALSE(state["drawers"][0u]["enabled"].asBool());
}

// ===========================================================================
// OnCommand — contract shapes
// ===========================================================================

TEST(MailboxVisualDebugger_OnCommand, OnCommandRoundTrip_ToggleTwiceRestores)
{
    Dia::Mailbox::Mailbox mailbox;
    mailbox.RegisterType<Dia::Mailbox::Testing::TestMessageA, 4>();
    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("QueueTable"));
    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("QueueTable"));

    const Json::Value state = GetState(domain);
    EXPECT_TRUE(state.isMember("types")) << "types key must return after toggling twice";
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
}

TEST(MailboxVisualDebugger_OnCommand, ScaleSensitivity_SetScale_NoOp)
{
    Dia::Mailbox::Mailbox mailbox;
    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);

    Json::Value args(Json::objectValue);
    args["key"]   = "debugScale";
    args["value"] = 2.5;

    EXPECT_NO_FATAL_FAILURE(
        domain.OnCommand(Dia::Core::StringCRC("setScale"), args));
}

TEST(MailboxVisualDebugger_OnCommand, UnknownCommand_NoOp)
{
    Dia::Mailbox::Mailbox mailbox;
    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);

    Json::Value empty(Json::objectValue);
    EXPECT_NO_FATAL_FAILURE(
        domain.OnCommand(Dia::Core::StringCRC("notACommand"), empty));
}

TEST(MailboxVisualDebugger_OnCommand, MalformedToggle_NoOp_NoCrash)
{
    Dia::Mailbox::Mailbox mailbox;
    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);

    Json::Value empty(Json::objectValue);
    EXPECT_NO_FATAL_FAILURE(
        domain.OnCommand(Dia::Core::StringCRC("toggle"), empty));

    // State must be unchanged — types key still present (default empty mailbox has types:[])
    const Json::Value state = GetState(domain);
    EXPECT_TRUE(state.isMember("types"));
}

// ===========================================================================
// Domain-specific shapes
// ===========================================================================

TEST(MailboxVisualDebugger_JSONState, Types_CountMatchesGetRegisteredTypeCount)
{
    Dia::Mailbox::Mailbox mailbox;
    mailbox.RegisterType<Dia::Mailbox::Testing::TestMessageA, 4>();
    mailbox.RegisterType<Dia::Mailbox::Testing::TestMessageB, 8>();
    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);

    const Json::Value state = GetState(domain);
    EXPECT_EQ(state["stats"]["typeCount"].asUInt(), mailbox.GetRegisteredTypeCount());
}

TEST(MailboxVisualDebugger_JSONState, Types_ArrayLengthMatchesTypeCount)
{
    Dia::Mailbox::Mailbox mailbox;
    mailbox.RegisterType<Dia::Mailbox::Testing::TestMessageA, 4>();
    mailbox.RegisterType<Dia::Mailbox::Testing::TestMessageB, 8>();
    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);

    const Json::Value state = GetState(domain);
    EXPECT_EQ(state["types"].size(), state["stats"]["typeCount"].asUInt());
}

TEST(MailboxVisualDebugger_JSONState, Types_CapacityFromTypeStats)
{
    Dia::Mailbox::Mailbox mailbox;
    mailbox.RegisterType<Dia::Mailbox::Testing::TestMessageA, 4>();
    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);

    const Json::Value state = GetState(domain);
    const Dia::Mailbox::Mailbox::TypeStats ts = mailbox.GetTypeStatsByIndex(0);

    EXPECT_EQ(state["types"][0u]["capacity"].asUInt(), ts.capacity);
    EXPECT_EQ(state["types"][0u]["capacity"].asUInt(), 4u);
}

TEST(MailboxVisualDebugger_JSONState, Types_CurrentCountFromTypeStats)
{
    Dia::Mailbox::Mailbox mailbox;
    mailbox.RegisterType<Dia::Mailbox::Testing::TestMessageA, 4>();
    const Dia::Mailbox::Address addr;
    mailbox.Send(addr, Dia::Mailbox::Testing::TestMessageA{1});
    mailbox.Send(addr, Dia::Mailbox::Testing::TestMessageA{2});

    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);
    const Json::Value state = GetState(domain);
    const Dia::Mailbox::Mailbox::TypeStats ts = mailbox.GetTypeStatsByIndex(0);

    EXPECT_EQ(state["types"][0u]["currentCount"].asUInt(), ts.currentCount);
    EXPECT_EQ(state["types"][0u]["currentCount"].asUInt(), 2u);
}

TEST(MailboxVisualDebugger_JSONState, Types_FillPct_Computed)
{
    Dia::Mailbox::Mailbox mailbox;
    mailbox.RegisterType<Dia::Mailbox::Testing::TestMessageA, 4>();
    const Dia::Mailbox::Address addr;
    mailbox.Send(addr, Dia::Mailbox::Testing::TestMessageA{1}); // 1/4 = 25%

    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);
    const Json::Value state = GetState(domain);

    EXPECT_EQ(state["types"][0u]["fillPct"].asUInt(), 25u);
}

TEST(MailboxVisualDebugger_JSONState, Types_FillPct_ClampedAt100)
{
    Dia::Mailbox::Mailbox mailbox;
    mailbox.RegisterType<Dia::Mailbox::Testing::TestMessageA, 4>();
    const Dia::Mailbox::Address addr;
    mailbox.Send(addr, Dia::Mailbox::Testing::TestMessageA{1});
    mailbox.Send(addr, Dia::Mailbox::Testing::TestMessageA{2});
    mailbox.Send(addr, Dia::Mailbox::Testing::TestMessageA{3});
    mailbox.Send(addr, Dia::Mailbox::Testing::TestMessageA{4}); // 4/4 = 100%

    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);
    const Json::Value state = GetState(domain);

    EXPECT_EQ(state["types"][0u]["fillPct"].asUInt(), 100u);
}

TEST(MailboxVisualDebugger_JSONState, Types_TotalDropped_FromTypeStats)
{
    Dia::Mailbox::Mailbox mailbox;
    CauseDrop(mailbox);

    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);
    const Json::Value state = GetState(domain);
    const Dia::Mailbox::Mailbox::TypeStats ts = mailbox.GetTypeStatsByIndex(0);

    EXPECT_EQ(state["types"][0u]["totalDropped"].asUInt64(), ts.totalDropped);
    EXPECT_GT(ts.totalDropped, 0u);
}

TEST(MailboxVisualDebugger_JSONState, Types_HasDrops_TrueWhenDropped)
{
    Dia::Mailbox::Mailbox mailbox;
    CauseDrop(mailbox);
    mailbox.RegisterType<Dia::Mailbox::Testing::TestMessageB, 8>(); // no drops

    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);
    const Json::Value state = GetState(domain);

    EXPECT_TRUE(state["types"][0u]["hasDrops"].asBool())  << "type A overflowed — hasDrops must be true";
    EXPECT_FALSE(state["types"][1u]["hasDrops"].asBool()) << "type B never overflowed — hasDrops must be false";
}

TEST(MailboxVisualDebugger_JSONState, Stats_TotalDropped_SumOfAll)
{
    Dia::Mailbox::Mailbox mailbox;
    CauseDrop(mailbox);
    mailbox.RegisterType<Dia::Mailbox::Testing::TestMessageB, 8>(); // no drops

    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);
    const Json::Value state = GetState(domain);

    uint64_t expected = 0;
    for (const Json::Value& t : state["types"])
        expected += t["totalDropped"].asUInt64();

    EXPECT_EQ(state["stats"]["totalDropped"].asUInt64(), expected);
}

TEST(MailboxVisualDebugger_JSONState, DrawerGate_DropAlerts_RemovesDropAlertsKey)
{
    Dia::Mailbox::Mailbox mailbox;
    CauseDrop(mailbox);
    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);

    // Enabled by default — key present
    {
        const Json::Value state = GetState(domain);
        EXPECT_TRUE(state.isMember("dropAlerts")) << "dropAlerts key must be present when enabled";
    }

    // Disable — key absent
    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("DropAlerts"));
    {
        const Json::Value state = GetState(domain);
        EXPECT_FALSE(state.isMember("dropAlerts")) << "dropAlerts key must be absent when disabled";
    }
}

TEST(MailboxVisualDebugger_OnCommand, Toggle_DropAlerts_TogglesDrawer)
{
    Dia::Mailbox::Mailbox mailbox;
    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("DropAlerts"));
    {
        const Json::Value state = GetState(domain);
        EXPECT_FALSE(state["drawers"][1u]["enabled"].asBool());
        EXPECT_FALSE(state.isMember("dropAlerts"));
    }

    // Toggle back
    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("DropAlerts"));
    {
        const Json::Value state = GetState(domain);
        EXPECT_TRUE(state["drawers"][1u]["enabled"].asBool());
        EXPECT_TRUE(state.isMember("dropAlerts"));
    }
}

TEST(MailboxVisualDebugger_JSONState, NoTypes_EmptyArray_NoAssert)
{
    Dia::Mailbox::Mailbox mailbox; // no types registered
    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);

    ASSERT_NO_FATAL_FAILURE({
        const Json::Value state = GetState(domain);
        EXPECT_TRUE(state.isMember("types"));
        EXPECT_TRUE(state["types"].isArray());
        EXPECT_EQ(state["types"].size(), 0u);
        EXPECT_EQ(state["stats"]["typeCount"].asUInt(), 0u);
        EXPECT_EQ(state["stats"]["totalDropped"].asUInt64(), 0u);
    });
}

TEST(MailboxVisualDebugger_JSONState, CapacityZero_NoAssert)
{
    // A type registered with capacity=0 must not cause a divide-by-zero;
    // fillPct must be 0.
    Dia::Mailbox::Mailbox mailbox;
    mailbox.RegisterType<Dia::Mailbox::Testing::TestMessageA, 0>();
    Dia::Mailbox::MailboxVisualDebugger domain(mailbox);

    ASSERT_NO_FATAL_FAILURE({
        const Json::Value state = GetState(domain);
        ASSERT_EQ(state["types"].size(), 1u);
        EXPECT_EQ(state["types"][0u]["fillPct"].asUInt(), 0u);
        EXPECT_EQ(state["types"][0u]["capacity"].asUInt(), 0u);
    });
}

#endif // DIA_DEBUG
