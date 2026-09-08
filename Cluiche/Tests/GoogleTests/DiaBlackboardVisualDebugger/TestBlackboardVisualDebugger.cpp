////////////////////////////////////////////////////////////////////////////////
// TestBlackboardVisualDebugger.cpp
// Tests for BlackboardVisualDebugger IDebugDomain:
//   - Identity: domain id, display name, group, accent colour, drawer count
//   - JSON state: drawers array, stats.slotCount, slots array, empty blackboard
//   - Formatters: hex fallback, formatted value, formatter gating, multi-dispatch
//   - OnCommand: toggle SlotTable, double-toggle restore, setScale no-op, unknown
//
// Feature spec: docs/specs/applications/dia/systems/diablackboardvisualdebugger/diablackboardvisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaBlackboardVisualDebugger/BlackboardVisualDebugger.h>
#include <DiaBlackboard/Blackboard.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>
#include <cstdio>

using Dia::Blackboard::Blackboard;
using Dia::Blackboard::BlackboardVisualDebugger;
using Dia::Core::StringCRC;

// ===========================================================================
// Helpers
// ===========================================================================

namespace
{

Json::Value ToggleSlotTable()
{
    Json::Value args(Json::objectValue);
    args["drawer"] = "SlotTable";
    return args;
}

Json::Value GetState(BlackboardVisualDebugger& domain)
{
    Json::Value out;
    domain.GetJSONState(out);
    return out;
}

// Simple type tags used as identity tokens in formatter tests
struct TypeA { int value; };
struct TypeB { float value; };

const char* FormatTypeA(const void* data, char* buf, int bufSize)
{
    const TypeA* a = static_cast<const TypeA*>(data);
    std::snprintf(buf, static_cast<size_t>(bufSize), "TypeA:%d", a->value);
    return buf;
}

const char* FormatTypeB(const void* data, char* buf, int bufSize)
{
    const TypeB* b = static_cast<const TypeB*>(data);
    std::snprintf(buf, static_cast<size_t>(bufSize), "TypeB:%.2f", static_cast<double>(b->value));
    return buf;
}

// Formatter that sets a flag to detect invocation
static bool gFormatterCalled = false;

const char* FlagFormatter(const void* /*data*/, char* buf, int bufSize)
{
    gFormatterCalled = true;
    std::snprintf(buf, static_cast<size_t>(bufSize), "flagged");
    return buf;
}

} // namespace

// ===========================================================================
// Suite: BlackboardVisualDebugger_Identity
// ===========================================================================

TEST(BlackboardVisualDebugger_Identity, IdsAndGroupAreCanonical)
{
    Blackboard board;
    BlackboardVisualDebugger domain(board);

    EXPECT_EQ(domain.GetDomainId(), StringCRC("blackboard"));
    EXPECT_STREQ(domain.GetDisplayName(), "Blackboard");
    EXPECT_EQ(domain.GetGroup(), StringCRC("AIBehavior"));
    EXPECT_FALSE(domain.HasWorldDrawers());
}

TEST(BlackboardVisualDebugger_Identity, DescriptionWithin80Chars)
{
    Blackboard board;
    BlackboardVisualDebugger domain(board);

    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(std::strlen(domain.GetDescription()), 80u);
}

TEST(BlackboardVisualDebugger_Identity, AccentIsAIBehaviorConstant)
{
    Blackboard board;
    BlackboardVisualDebugger domain(board);

    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kAIBehavior);
}

TEST(BlackboardVisualDebugger_Identity, GetDrawerCount_ReturnsZero)
{
    Blackboard board;
    BlackboardVisualDebugger domain(board);

    EXPECT_EQ(domain.GetDrawerCount(), 0);
    EXPECT_EQ(domain.GetDrawer(0), nullptr);
}

// ===========================================================================
// Suite: BlackboardVisualDebugger_JSONState
// ===========================================================================

TEST(BlackboardVisualDebugger_JSONState, ReportsDrawerAndStats)
{
    Blackboard board;
    BlackboardVisualDebugger domain(board);

    Json::Value state = GetState(domain);

    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_TRUE(state["drawers"].isArray());
    ASSERT_EQ(state["drawers"].size(), 1u);

    const Json::Value& drawer = state["drawers"][0u];
    EXPECT_TRUE(drawer.isMember("name"));
    EXPECT_STREQ(drawer["name"].asCString(), "SlotTable");
    ASSERT_TRUE(drawer.isMember("enabled"));
    EXPECT_TRUE(drawer["enabled"].asBool());

    ASSERT_TRUE(state.isMember("stats"));
    EXPECT_TRUE(state["stats"].isObject());
    EXPECT_TRUE(state["stats"].isMember("slotCount"));
}

TEST(BlackboardVisualDebugger_JSONState, SlotCount_MatchesRegistered)
{
    Blackboard board;
    board.Register<int>(StringCRC("health"));
    board.Register<float>(StringCRC("speed"));
    BlackboardVisualDebugger domain(board);

    Json::Value state = GetState(domain);

    ASSERT_TRUE(state["stats"].isMember("slotCount"));
    EXPECT_EQ(state["stats"]["slotCount"].asInt(), 2);
}

TEST(BlackboardVisualDebugger_JSONState, SlotsArray_HasEntryPerSlot)
{
    Blackboard board;
    board.Register<int>(StringCRC("health"));
    board.Register<float>(StringCRC("speed"));
    BlackboardVisualDebugger domain(board);

    Json::Value state = GetState(domain);

    ASSERT_TRUE(state.isMember("slots"));
    ASSERT_TRUE(state["slots"].isArray());
    EXPECT_EQ(state["slots"].size(), 2u);

    for (Json::ArrayIndex i = 0; i < state["slots"].size(); ++i)
    {
        const Json::Value& slot = state["slots"][i];
        EXPECT_TRUE(slot.isMember("key"))   << "slot[" << i << "] missing key";
        EXPECT_TRUE(slot.isMember("type"))  << "slot[" << i << "] missing type";
        EXPECT_TRUE(slot.isMember("value")) << "slot[" << i << "] missing value";
    }
}

TEST(BlackboardVisualDebugger_JSONState, EmptyBlackboard_EmptySlots)
{
    Blackboard board;
    BlackboardVisualDebugger domain(board);

    Json::Value state = GetState(domain);

    ASSERT_TRUE(state["stats"].isMember("slotCount"));
    EXPECT_EQ(state["stats"]["slotCount"].asInt(), 0);

    ASSERT_TRUE(state.isMember("slots"));
    ASSERT_TRUE(state["slots"].isArray());
    EXPECT_EQ(state["slots"].size(), 0u);
}

TEST(BlackboardVisualDebugger_JSONState, SlotKey_PresentForEachSlot)
{
    Blackboard board;
    board.Register<int>(StringCRC("patrol_index"));
    board.Register<float>(StringCRC("health_pct"));
    BlackboardVisualDebugger domain(board);

    Json::Value state = GetState(domain);

    ASSERT_TRUE(state.isMember("slots"));
    for (Json::ArrayIndex i = 0; i < state["slots"].size(); ++i)
    {
        const Json::Value& slot = state["slots"][i];
        ASSERT_TRUE(slot.isMember("key"));
        // key must be a non-null string
        EXPECT_TRUE(slot["key"].isString()) << "slot[" << i << "] key is not a string";
    }
}

// ===========================================================================
// Suite: BlackboardVisualDebugger_Formatters
// ===========================================================================

TEST(BlackboardVisualDebugger_Formatters, HexFallback_NoFormatter)
{
    Blackboard board;
    board.Register<int>(StringCRC("score"));
    board.Get<int>(StringCRC("score")) = 0x12345678;
    BlackboardVisualDebugger domain(board);

    Json::Value state = GetState(domain);

    ASSERT_TRUE(state.isMember("slots"));
    ASSERT_GE(state["slots"].size(), 1u);

    bool found = false;
    for (Json::ArrayIndex i = 0; i < state["slots"].size(); ++i)
    {
        const std::string val = state["slots"][i]["value"].asString();
        // Should start with "0x" (hex fallback)
        if (val.size() >= 2 && val[0] == '0' && (val[1] == 'x' || val[1] == 'X'))
        {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found) << "Expected at least one slot with hex fallback value";
}

TEST(BlackboardVisualDebugger_Formatters, FormatterValue_WithFormatter)
{
    Blackboard board;
    board.Register<TypeA>(StringCRC("my_a"));
    board.Get<TypeA>(StringCRC("my_a")).value = 99;

    BlackboardVisualDebugger domain(board);

    // Register formatter keyed on the TypeA type tag from VisitSlots.
    // We find the typeTag by doing a visit ourselves.
    const void* typeTagA = nullptr;
    board.VisitSlots([&](StringCRC /*key*/, const void* tag, const void* /*data*/)
    {
        typeTagA = tag;
    });
    ASSERT_NE(typeTagA, nullptr);

    domain.RegisterFormatter(typeTagA, FormatTypeA);

    Json::Value state = GetState(domain);

    ASSERT_TRUE(state.isMember("slots"));
    ASSERT_GE(state["slots"].size(), 1u);

    bool foundFormatted = false;
    for (Json::ArrayIndex i = 0; i < state["slots"].size(); ++i)
    {
        const std::string val = state["slots"][i]["value"].asString();
        if (val.find("TypeA:99") != std::string::npos)
        {
            foundFormatted = true;
            break;
        }
    }
    EXPECT_TRUE(foundFormatted) << "Expected formatter output 'TypeA:99' in slots";
}

TEST(BlackboardVisualDebugger_Formatters, FormatterNotCalled_WhenDrawerDisabled)
{
    Blackboard board;
    board.Register<TypeA>(StringCRC("my_a"));
    board.Get<TypeA>(StringCRC("my_a")).value = 7;

    BlackboardVisualDebugger domain(board);

    const void* typeTagA = nullptr;
    board.VisitSlots([&](StringCRC /*key*/, const void* tag, const void* /*data*/)
    {
        typeTagA = tag;
    });
    ASSERT_NE(typeTagA, nullptr);

    gFormatterCalled = false;
    domain.RegisterFormatter(typeTagA, FlagFormatter);

    // Disable the SlotTable
    domain.OnCommand(StringCRC("toggle"), ToggleSlotTable());

    gFormatterCalled = false;
    Json::Value state = GetState(domain);

    EXPECT_FALSE(gFormatterCalled)
        << "Formatter should not be called when SlotTable is disabled";
    EXPECT_FALSE(state.isMember("slots"))
        << "slots key should be absent when SlotTable is disabled";
}

TEST(BlackboardVisualDebugger_Formatters, MultipleFormatters_CorrectDispatch)
{
    Blackboard board;
    board.Register<TypeA>(StringCRC("my_a"));
    board.Register<TypeB>(StringCRC("my_b"));
    board.Get<TypeA>(StringCRC("my_a")).value   = 42;
    board.Get<TypeB>(StringCRC("my_b")).value   = 3.14f;

    const void* typeTagA = nullptr;
    const void* typeTagB = nullptr;
    board.VisitSlots([&](StringCRC key, const void* tag, const void* /*data*/)
    {
        if (key == StringCRC("my_a")) typeTagA = tag;
        if (key == StringCRC("my_b")) typeTagB = tag;
    });
    ASSERT_NE(typeTagA, nullptr);
    ASSERT_NE(typeTagB, nullptr);
    ASSERT_NE(typeTagA, typeTagB);

    BlackboardVisualDebugger domain(board);
    domain.RegisterFormatter(typeTagA, FormatTypeA);
    domain.RegisterFormatter(typeTagB, FormatTypeB);

    Json::Value state = GetState(domain);
    ASSERT_TRUE(state.isMember("slots"));
    ASSERT_EQ(state["slots"].size(), 2u);

    bool foundA = false, foundB = false;
    for (Json::ArrayIndex i = 0; i < state["slots"].size(); ++i)
    {
        const std::string val = state["slots"][i]["value"].asString();
        if (val.find("TypeA:42") != std::string::npos) foundA = true;
        if (val.find("TypeB:") != std::string::npos)   foundB = true;
    }
    EXPECT_TRUE(foundA) << "Expected TypeA formatter output";
    EXPECT_TRUE(foundB) << "Expected TypeB formatter output";
}

// ===========================================================================
// Suite: BlackboardVisualDebugger_OnCommand
// ===========================================================================

TEST(BlackboardVisualDebugger_OnCommand, Toggle_SlotTable_RemovesSlotsKey)
{
    Blackboard board;
    board.Register<int>(StringCRC("x"));
    BlackboardVisualDebugger domain(board);

    // Verify slots present before toggle
    {
        Json::Value state = GetState(domain);
        ASSERT_TRUE(state.isMember("slots"));
    }

    domain.OnCommand(StringCRC("toggle"), ToggleSlotTable());

    {
        Json::Value state = GetState(domain);
        EXPECT_FALSE(state.isMember("slots"))
            << "slots key should be absent after toggle";
    }
}

TEST(BlackboardVisualDebugger_OnCommand, Toggle_SlotTable_Twice_Restores)
{
    Blackboard board;
    board.Register<int>(StringCRC("x"));
    BlackboardVisualDebugger domain(board);

    domain.OnCommand(StringCRC("toggle"), ToggleSlotTable());
    domain.OnCommand(StringCRC("toggle"), ToggleSlotTable());

    Json::Value state = GetState(domain);
    EXPECT_TRUE(state.isMember("slots"))
        << "slots key should be restored after double toggle";
}

TEST(BlackboardVisualDebugger_OnCommand, SetScale_NoOp_NoCrash)
{
    Blackboard board;
    BlackboardVisualDebugger domain(board);

    Json::Value args(Json::objectValue);
    args["key"]   = "debugScale";
    args["value"] = 2.5;
    EXPECT_NO_FATAL_FAILURE(domain.OnCommand(StringCRC("setScale"), args));
}

TEST(BlackboardVisualDebugger_OnCommand, UnknownCommand_NoOp)
{
    Blackboard board;
    BlackboardVisualDebugger domain(board);

    Json::Value empty(Json::objectValue);
    EXPECT_NO_FATAL_FAILURE(domain.OnCommand(StringCRC("notACommand"), empty));

    // State should be unaffected
    Json::Value state = GetState(domain);
    EXPECT_TRUE(state.isMember("slots"));
}

TEST(BlackboardVisualDebugger_OnCommand, MalformedToggle_NoOp)
{
    Blackboard board;
    board.Register<int>(StringCRC("x"));
    BlackboardVisualDebugger domain(board);

    // Missing "drawer" key — should be a no-op, no crash
    Json::Value empty(Json::objectValue);
    EXPECT_NO_FATAL_FAILURE(domain.OnCommand(StringCRC("toggle"), empty));

    // slots should still be present (toggle was not applied)
    Json::Value state = GetState(domain);
    EXPECT_TRUE(state.isMember("slots"));
}

// ===========================================================================
// Additional domain-specific shapes (plan task 6)
// ===========================================================================

TEST(BlackboardVisualDebugger_JSONState, SlotCount_ReportedEvenWhenDrawerDisabled)
{
    Blackboard board;
    board.Register<int>(StringCRC("a"));
    board.Register<int>(StringCRC("b"));
    board.Register<int>(StringCRC("c"));
    BlackboardVisualDebugger domain(board);

    // Disable the drawer
    domain.OnCommand(StringCRC("toggle"), ToggleSlotTable());

    Json::Value state = GetState(domain);
    ASSERT_TRUE(state["stats"].isMember("slotCount"));
    EXPECT_EQ(state["stats"]["slotCount"].asInt(), 3)
        << "slotCount should reflect all slots even when SlotTable is disabled";
    EXPECT_FALSE(state.isMember("slots"))
        << "slots array should be absent when SlotTable is disabled";
}

TEST(BlackboardVisualDebugger_JSONState, DrawerEnabledFlag_MatchesToggleState)
{
    Blackboard board;
    BlackboardVisualDebugger domain(board);

    {
        Json::Value state = GetState(domain);
        EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool())
            << "SlotTable should start enabled";
    }

    domain.OnCommand(StringCRC("toggle"), ToggleSlotTable());

    {
        Json::Value state = GetState(domain);
        EXPECT_FALSE(state["drawers"][0u]["enabled"].asBool())
            << "SlotTable should report disabled after toggle";
    }
}

#endif // DIA_DEBUG
