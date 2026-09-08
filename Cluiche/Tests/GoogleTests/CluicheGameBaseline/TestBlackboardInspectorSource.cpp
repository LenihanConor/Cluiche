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
