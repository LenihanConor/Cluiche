////////////////////////////////////////////////////////////////////////////////
// TestBlackboardInspectorSource.cpp
// Unit tests for BlackboardInspectorSource covering ACs 1-10 from the
// DiaBlackboardInspector feature spec.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#include <DiaBlackboard/Blackboard.h>
#include <DiaBlackboard/BlackboardRegistry.h>
#include <DiaBlackboard/IBlackboardObserver.h>
#include <DiaBlackboard/Testing/BlackboardTestHelpers.h>
#include <DiaCore/Json/external/json/json.h>
#include <Modules/InspectorSources/BlackboardInspectorSource.h>

using namespace Dia::Blackboard;
using namespace Dia::Blackboard::Testing;

// ---------------------------------------------------------------------------
// Test data structs
// ---------------------------------------------------------------------------

struct HealthBoard
{
    float mCurrentHp = 100.0f;
    float mMaxHp     = 100.0f;
};

struct ThreatBoard
{
    int mThreatLevel = 0;
};

// ---------------------------------------------------------------------------
// Testable subclass — exposes protected CollectAndHash for direct calls
// ---------------------------------------------------------------------------

class TestableBlackboardInspectorSource
    : public Cluiche::AppFlow::BlackboardInspectorSource
{
public:
    using BlackboardInspectorSource::BlackboardInspectorSource;

    unsigned int CallCollectAndHash(Json::Value& payload)
    {
        return CollectAndHash(payload);
    }
};

// ===========================================================================
// DiaBlackboardInspectorSource suite
// ===========================================================================

// AC1 — GetTopic() returns StringCRC{"blackboard.state"}
TEST(DiaBlackboardInspectorSource, GetTopic_ReturnsBlackboardState)
{
    BlackboardRegistry registry;
    TestableBlackboardInspectorSource src(registry);

    EXPECT_EQ(src.GetTopic(), Dia::Core::StringCRC{"blackboard.state"});
}

// AC2 — CollectAndHash emits a "boards" JSON array with one entry per
//        registered board
TEST(DiaBlackboardInspectorSource, CollectAndHash_OneBoardEntry_BoardsArrayHasOneEntry)
{
    BlackboardRegistry registry;
    Blackboard board;
    registry.Register(Dia::Core::StringCRC{"PlayerBoard"}, "Player", board);

    TestableBlackboardInspectorSource src(registry);
    Json::Value payload;
    src.CallCollectAndHash(payload);

    ASSERT_TRUE(payload.isMember("boards"));
    EXPECT_EQ(payload["boards"].size(), 1u);
}

// AC3 — Each board entry contains id, label, slots, and observers keys
TEST(DiaBlackboardInspectorSource, CollectAndHash_BoardEntry_HasRequiredFields)
{
    BlackboardRegistry registry;
    Blackboard board;
    registry.Register(Dia::Core::StringCRC{"PlayerBoard"}, "Player", board);

    TestableBlackboardInspectorSource src(registry);
    Json::Value payload;
    src.CallCollectAndHash(payload);

    const Json::Value& entry = payload["boards"][0];
    EXPECT_TRUE(entry.isMember("id"));
    EXPECT_TRUE(entry.isMember("label"));
    EXPECT_TRUE(entry.isMember("slots"));
    EXPECT_TRUE(entry.isMember("observers"));
}

// AC4 — Slot entries with a registered serializer include a populated "value"
//        object
TEST(DiaBlackboardInspectorSource, CollectAndHash_SlotWithSerializer_HasPopulatedValue)
{
    BlackboardRegistry registry;

    registry.RegisterSerializer<HealthBoard>([](const void* data, Json::Value& out)
    {
        const HealthBoard* hb = static_cast<const HealthBoard*>(data);
        out["hp"] = hb->mCurrentHp;
    });

    Blackboard board;
    HealthBoard& h = board.Register<HealthBoard>(Dia::Core::StringCRC{"Health"});
    h.mCurrentHp = 99.0f;

    registry.Register(Dia::Core::StringCRC{"PlayerBoard"}, "Player", board);

    TestableBlackboardInspectorSource src(registry);
    Json::Value payload;
    src.CallCollectAndHash(payload);

    const Json::Value& slots = payload["boards"][0]["slots"];
    ASSERT_GE(slots.size(), 1u);

    // Find the Health slot
    bool found = false;
    for (const auto& slot : slots)
    {
        if (std::string(slot["key"].asCString()) == "Health")
        {
            EXPECT_TRUE(slot["value"].isObject());
            EXPECT_FLOAT_EQ(slot["value"]["hp"].asFloat(), 99.0f);
            found = true;
        }
    }
    EXPECT_TRUE(found) << "Expected a 'Health' slot in the payload";
}

// AC5 — Slot entries without a registered serializer emit "value": "[no serializer]"
TEST(DiaBlackboardInspectorSource, CollectAndHash_SlotWithoutSerializer_HasNoSerializerValue)
{
    BlackboardRegistry registry;
    // ThreatBoard deliberately has no serializer registered

    Blackboard board;
    board.Register<ThreatBoard>(Dia::Core::StringCRC{"Threat"});
    registry.Register(Dia::Core::StringCRC{"EnemyBoard"}, "Enemy", board);

    TestableBlackboardInspectorSource src(registry);
    Json::Value payload;
    src.CallCollectAndHash(payload);

    const Json::Value& slots = payload["boards"][0]["slots"];
    ASSERT_GE(slots.size(), 1u);

    bool found = false;
    for (const auto& slot : slots)
    {
        if (std::string(slot["key"].asCString()) == "Threat")
        {
            EXPECT_TRUE(slot["value"].isString());
            EXPECT_STREQ(slot["value"].asCString(), "[no serializer]");
            found = true;
        }
    }
    EXPECT_TRUE(found) << "Expected a 'Threat' slot in the payload";
}

// AC6 — observers array contains GetId().AsChar() strings for all attached
//        observers
TEST(DiaBlackboardInspectorSource, CollectAndHash_ObserversArray_ContainsObserverIds)
{
    BlackboardRegistry registry;

    Blackboard board;
    MockBlackboardObserver obs;
    board.AddObserver(obs);

    registry.Register(Dia::Core::StringCRC{"PlayerBoard"}, "Player", board);

    TestableBlackboardInspectorSource src(registry);
    Json::Value payload;
    src.CallCollectAndHash(payload);

    const Json::Value& observers = payload["boards"][0]["observers"];
    ASSERT_EQ(observers.size(), 1u);
    EXPECT_STREQ(observers[0].asCString(), "MockObserver");
}

// AC7 — Hash changes when a slot is added to a registered board
TEST(DiaBlackboardInspectorSource, CollectAndHash_HashChanges_WhenSlotAdded)
{
    BlackboardRegistry registry;

    Blackboard board;
    registry.Register(Dia::Core::StringCRC{"PlayerBoard"}, "Player", board);

    TestableBlackboardInspectorSource src(registry);

    Json::Value payload1;
    unsigned int hash1 = src.CallCollectAndHash(payload1);

    board.Register<HealthBoard>(Dia::Core::StringCRC{"Health"});

    Json::Value payload2;
    unsigned int hash2 = src.CallCollectAndHash(payload2);

    EXPECT_NE(hash1, hash2);
}

// AC8 — Hash changes when an observer is attached/detached
TEST(DiaBlackboardInspectorSource, CollectAndHash_HashChanges_WhenObserverAttached)
{
    BlackboardRegistry registry;

    Blackboard board;
    registry.Register(Dia::Core::StringCRC{"PlayerBoard"}, "Player", board);

    TestableBlackboardInspectorSource src(registry);

    Json::Value payload1;
    unsigned int hash1 = src.CallCollectAndHash(payload1);

    MockBlackboardObserver obs;
    board.AddObserver(obs);

    Json::Value payload2;
    unsigned int hash2 = src.CallCollectAndHash(payload2);

    EXPECT_NE(hash1, hash2);
}

// AC9 — Hash does not change when boards and slots are stable across two
//        consecutive calls
TEST(DiaBlackboardInspectorSource, CollectAndHash_HashStable_WhenNoChange)
{
    BlackboardRegistry registry;

    Blackboard board;
    board.Register<HealthBoard>(Dia::Core::StringCRC{"Health"});
    registry.Register(Dia::Core::StringCRC{"PlayerBoard"}, "Player", board);

    TestableBlackboardInspectorSource src(registry);

    Json::Value payload1;
    unsigned int hash1 = src.CallCollectAndHash(payload1);

    Json::Value payload2;
    unsigned int hash2 = src.CallCollectAndHash(payload2);

    EXPECT_EQ(hash1, hash2);
}

// AC10 — Source produces empty "boards" array when no boards are registered
TEST(DiaBlackboardInspectorSource, CollectAndHash_EmptyRegistry_BoardsArrayEmpty)
{
    BlackboardRegistry registry;

    TestableBlackboardInspectorSource src(registry);
    Json::Value payload;
    src.CallCollectAndHash(payload);

    ASSERT_TRUE(payload.isMember("boards"));
    EXPECT_EQ(payload["boards"].size(), 0u);
}

// Test 9 — Two registered boards → payload["boards"] has 2 entries with correct ids
TEST(DiaBlackboardInspectorSource, CollectAndHash_TwoBoards_PayloadHasTwoEntries)
{
    BlackboardRegistry registry;
    Blackboard boardA, boardB;
    registry.Register(Dia::Core::StringCRC{"board_a"}, "BoardA", boardA);
    registry.Register(Dia::Core::StringCRC{"board_b"}, "BoardB", boardB);

    TestableBlackboardInspectorSource src(registry);
    Json::Value payload;
    src.CallCollectAndHash(payload);

    ASSERT_TRUE(payload.isMember("boards"));
    EXPECT_EQ(payload["boards"].size(), 2u);

    bool foundA = false;
    bool foundB = false;
    for (const auto& entry : payload["boards"])
    {
        if (std::string(entry["id"].asCString()) == "board_a") foundA = true;
        if (std::string(entry["id"].asCString()) == "board_b") foundB = true;
    }
    EXPECT_TRUE(foundA);
    EXPECT_TRUE(foundB);
}

// Test 10 — Hash changes when a new board is added between calls
TEST(DiaBlackboardInspectorSource, CollectAndHash_HashChanges_WhenBoardAdded)
{
    BlackboardRegistry registry;

    TestableBlackboardInspectorSource src(registry);

    Json::Value payload1;
    unsigned int hash1 = src.CallCollectAndHash(payload1);

    Blackboard board;
    registry.Register(Dia::Core::StringCRC{"new_board"}, "NewBoard", board);

    Json::Value payload2;
    unsigned int hash2 = src.CallCollectAndHash(payload2);

    EXPECT_NE(hash1, hash2);
}

// Test 11 — Hash changes when a board is removed from the registry
TEST(DiaBlackboardInspectorSource, CollectAndHash_HashChanges_WhenBoardRemoved)
{
    BlackboardRegistry registry;
    Blackboard boardA, boardB;
    registry.Register(Dia::Core::StringCRC{"board_a"}, "BoardA", boardA);
    registry.Register(Dia::Core::StringCRC{"board_b"}, "BoardB", boardB);

    TestableBlackboardInspectorSource src(registry);

    Json::Value payload1;
    unsigned int hash1 = src.CallCollectAndHash(payload1);

    registry.Unregister(Dia::Core::StringCRC{"board_b"});

    Json::Value payload2;
    unsigned int hash2 = src.CallCollectAndHash(payload2);

    EXPECT_NE(hash1, hash2);
}

// Test 12 — Mixed slots: one with serializer → object; one without → "[no serializer]"
TEST(DiaBlackboardInspectorSource, CollectAndHash_MixedSlots_OneSerializedOneNot)
{
    BlackboardRegistry registry;

    registry.RegisterSerializer<HealthBoard>([](const void* data, Json::Value& out)
    {
        const HealthBoard* hb = static_cast<const HealthBoard*>(data);
        out["hp"] = hb->mCurrentHp;
    });
    // ThreatBoard deliberately has no serializer

    Blackboard board;
    board.Register<HealthBoard>(Dia::Core::StringCRC{"health"});
    board.Register<ThreatBoard>(Dia::Core::StringCRC{"threat"});
    registry.Register(Dia::Core::StringCRC{"mixed_board"}, "Mixed", board);

    TestableBlackboardInspectorSource src(registry);
    Json::Value payload;
    src.CallCollectAndHash(payload);

    ASSERT_EQ(payload["boards"].size(), 1u);
    const Json::Value& slots = payload["boards"][0]["slots"];
    ASSERT_GE(slots.size(), 2u);

    bool foundHealth = false;
    bool foundThreat = false;
    for (const auto& slot : slots)
    {
        std::string key = slot["key"].asCString();
        if (key == "health")
        {
            EXPECT_TRUE(slot["value"].isObject()) << "health slot should have a serialized object";
            foundHealth = true;
        }
        if (key == "threat")
        {
            EXPECT_TRUE(slot["value"].isString()) << "threat slot should be a string";
            EXPECT_STREQ(slot["value"].asCString(), "[no serializer]");
            foundThreat = true;
        }
    }
    EXPECT_TRUE(foundHealth);
    EXPECT_TRUE(foundThreat);
}

// Test 13 — Board with no slots or observers produces empty "slots" and "observers" arrays
TEST(DiaBlackboardInspectorSource, CollectAndHash_BoardWithNoSlotsOrObservers_EmptyArrays)
{
    BlackboardRegistry registry;
    Blackboard board; // empty board — no slots, no observers
    registry.Register(Dia::Core::StringCRC{"empty_board"}, "Empty", board);

    TestableBlackboardInspectorSource src(registry);
    Json::Value payload;
    src.CallCollectAndHash(payload);

    ASSERT_EQ(payload["boards"].size(), 1u);
    const Json::Value& entry = payload["boards"][0];

    EXPECT_EQ(entry["slots"].size(), 0u);
    EXPECT_EQ(entry["observers"].size(), 0u);
}

// ===========================================================================
// Scopes (parent chain) — name / parent / inherited
// ===========================================================================

namespace
{
    const Json::Value* FindSlot(const Json::Value& slots, const char* key)
    {
        for (const auto& slot : slots)
        {
            if (std::string(slot["key"].asCString()) == key) return &slot;
        }
        return nullptr;
    }
}

TEST(DiaBlackboardInspectorSource, Scopes_ChainShowsNamesLocalAndInheritedSlots)
{
    BlackboardRegistry registry;
    Blackboard game(32, Dia::Core::StringCRC{"game"});
    Blackboard world(32, Dia::Core::StringCRC{"world"});
    Blackboard agent(32, Dia::Core::StringCRC{"agent"});
    ASSERT_TRUE(world.SetParent(&game));
    ASSERT_TRUE(agent.SetParent(&world));

    game.Register<ThreatBoard>(Dia::Core::StringCRC{"threat"});
    game.Register<HealthBoard>(Dia::Core::StringCRC{"health"});
    world.Register<HealthBoard>(Dia::Core::StringCRC{"health"});   // shadows game.health
    agent.Register<ThreatBoard>(Dia::Core::StringCRC{"own"});

    registry.Register(Dia::Core::StringCRC{"agent_id"}, "Agent", agent);

    TestableBlackboardInspectorSource src(registry);
    Json::Value payload;
    src.CallCollectAndHash(payload);

    const Json::Value& entry = payload["boards"][0];
    EXPECT_STREQ(entry["name"].asCString(), "agent");
    EXPECT_STREQ(entry["parent"].asCString(), "world");
    ASSERT_NE(FindSlot(entry["slots"], "own"), nullptr);

    const Json::Value& inherited = entry["inherited"];
    ASSERT_EQ(inherited.size(), 2u);
    EXPECT_STREQ(inherited[0]["from"].asCString(), "world");
    EXPECT_STREQ(inherited[1]["from"].asCString(), "game");

    const Json::Value* worldHealth = FindSlot(inherited[0]["slots"], "health");
    ASSERT_NE(worldHealth, nullptr);
    EXPECT_FALSE((*worldHealth)["shadowed"].asBool());
    EXPECT_TRUE(worldHealth->isMember("type"));
    EXPECT_TRUE(worldHealth->isMember("value"));

    const Json::Value* gameHealth = FindSlot(inherited[1]["slots"], "health");
    ASSERT_NE(gameHealth, nullptr);
    EXPECT_TRUE((*gameHealth)["shadowed"].asBool());

    const Json::Value* gameThreat = FindSlot(inherited[1]["slots"], "threat");
    ASSERT_NE(gameThreat, nullptr);
    EXPECT_FALSE((*gameThreat)["shadowed"].asBool());
}

TEST(DiaBlackboardInspectorSource, Scopes_LocalKeyShadowsParentKey)
{
    BlackboardRegistry registry;
    Blackboard parent(32, Dia::Core::StringCRC{"p"});
    Blackboard child(32, Dia::Core::StringCRC{"c"});
    ASSERT_TRUE(child.SetParent(&parent));
    parent.Register<ThreatBoard>(Dia::Core::StringCRC{"k"});
    child.Register<ThreatBoard>(Dia::Core::StringCRC{"k"});
    registry.Register(Dia::Core::StringCRC{"c_id"}, "C", child);

    TestableBlackboardInspectorSource src(registry);
    Json::Value payload;
    src.CallCollectAndHash(payload);

    const Json::Value* k = FindSlot(payload["boards"][0]["inherited"][0]["slots"], "k");
    ASSERT_NE(k, nullptr);
    EXPECT_TRUE((*k)["shadowed"].asBool());
}

TEST(DiaBlackboardInspectorSource, Scopes_NoParent_EmptyParentAndInherited)
{
    BlackboardRegistry registry;
    Blackboard board(32, Dia::Core::StringCRC{"solo"});
    registry.Register(Dia::Core::StringCRC{"solo_id"}, "Solo", board);

    TestableBlackboardInspectorSource src(registry);
    Json::Value payload;
    src.CallCollectAndHash(payload);

    const Json::Value& entry = payload["boards"][0];
    EXPECT_STREQ(entry["name"].asCString(), "solo");
    EXPECT_STREQ(entry["parent"].asCString(), "");
    ASSERT_TRUE(entry.isMember("inherited"));
    EXPECT_EQ(entry["inherited"].size(), 0u);
}

TEST(DiaBlackboardInspectorSource, Scopes_HashChanges_WhenParentSet)
{
    BlackboardRegistry registry;
    Blackboard parent(32, Dia::Core::StringCRC{"p"});
    Blackboard child(32, Dia::Core::StringCRC{"c"});
    registry.Register(Dia::Core::StringCRC{"c_id"}, "C", child);

    TestableBlackboardInspectorSource src(registry);
    Json::Value p1, p2, p3;
    unsigned int h1 = src.CallCollectAndHash(p1);
    ASSERT_TRUE(child.SetParent(&parent));
    unsigned int h2 = src.CallCollectAndHash(p2);
    EXPECT_NE(h1, h2);

    parent.Register<ThreatBoard>(Dia::Core::StringCRC{"k"});   // inherited slot count change
    unsigned int h3 = src.CallCollectAndHash(p3);
    EXPECT_NE(h2, h3);
}

TEST(DiaBlackboardInspectorSource, Scopes_UnnamedBoard_EmitsStringNameAndEmptyParent)
{
    BlackboardRegistry registry;
    Blackboard board; // default name
    registry.Register(Dia::Core::StringCRC{"anon_id"}, "Anon", board);

    TestableBlackboardInspectorSource src(registry);
    Json::Value payload;
    src.CallCollectAndHash(payload);

    const Json::Value& entry = payload["boards"][0];
    ASSERT_TRUE(entry.isMember("name"));
    EXPECT_TRUE(entry["name"].isString());
    EXPECT_STREQ(entry["parent"].asCString(), "");
    EXPECT_EQ(entry["inherited"].size(), 0u);
}

TEST(DiaBlackboardInspectorSource, Scopes_UnnamedParent_StillListsInheritedSlots)
{
    BlackboardRegistry registry;
    Blackboard parent;
    Blackboard child(32, Dia::Core::StringCRC{"c"});
    ASSERT_TRUE(child.SetParent(&parent));
    parent.Register<ThreatBoard>(Dia::Core::StringCRC{"k"});
    registry.Register(Dia::Core::StringCRC{"c_id"}, "C", child);

    TestableBlackboardInspectorSource src(registry);
    Json::Value payload;
    src.CallCollectAndHash(payload);

    const Json::Value& inherited = payload["boards"][0]["inherited"];
    ASSERT_EQ(inherited.size(), 1u);
    EXPECT_NE(FindSlot(inherited[0]["slots"], "k"), nullptr);
}
