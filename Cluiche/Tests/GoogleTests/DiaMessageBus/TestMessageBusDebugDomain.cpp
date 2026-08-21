// TestMessageBusDebugDomain.cpp
//
// Covers Dia::MessageBus::MessageBusDebugDomain — the Debug-only IDebugDomain
// exposing Schema / Live / History tabs over a MessageBusModule. Scoped to
// the domain implementation only (AC-2 through AC-8 from
// docs/specs/applications/dia/systems/diamessagebus/visual-debugger.md);
// AC-1 (wiring into a real CluicheTest stage) is a separate, later task.
//
// AC-2: Schema tab — alphabetical, typeId/producerIds/routerId/subscriberIds.
// AC-3: Live tab — last-tick ledger, sorted by count desc, totals row.
// AC-4: History tab — per-tick totals for the last windowTicks snapshots;
//       dropped ticks distinguishable.
// AC-5: OnCommand("selectTab", ...) switches activeTab.
// AC-6: OnCommand("setHistoryWindow", ...) changes historyWindowTicks, and
//       actually truncates the History tab's visible window.
// AC-7: MessageBusDebugDomain.h/.cpp are wrapped #ifdef DIA_DEBUG end to end
//       (verified by code inspection — grep for the guards below — this
//       test file itself only builds under DIA_DEBUG, same as
//       TestDiaMessageBusLedgerHistory.cpp's AC-5).
// AC-8: GetDomainId()/GetGroup()/HasWorldDrawers() identity checks.

#ifdef DIA_DEBUG

#include <gtest/gtest.h>

#include <DiaMessageBus/MessageBusDebugDomain.h>
#include <DiaMessageBus/MessageBusModule.h>
#include <DiaMessageBus/Bus.h>
#include <DiaMailbox/MailboxTypes.h>

namespace Dia::MessageBus::Testing {

    // -------------------------------------------------------------------------
    // Testable module subclass — exposes protected lifecycle hooks, mirroring
    // TestDiaMessageBusCore.cpp / TestDiaMessageBusLedgerHistory.cpp's
    // TestableMessageBusModule.
    // -------------------------------------------------------------------------
    class TestableMessageBusModule : public Dia::MessageBus::MessageBusModule {
    public:
        void Start()          { DoStart(); }
        void Update(float dt) { DoUpdate(dt); }
        void Stop()           { DoStop(); }
    };

    // -------------------------------------------------------------------------
    // Test-only message types.
    // -------------------------------------------------------------------------
    struct SchemaMsgBoth {
        static inline const Dia::Core::StringCRC kTypeId{ "SchemaMsgBoth" };
    };
    struct SchemaMsgBroadcast {
        static inline const Dia::Core::StringCRC kTypeId{ "SchemaMsgBroadcast" };
    };
    struct SchemaMsgEntity {
        static inline const Dia::Core::StringCRC kTypeId{ "SchemaMsgEntity" };
    };

    struct LiveMsgA {
        static inline const Dia::Core::StringCRC kTypeId{ "LiveMsgA" };
    };
    struct LiveMsgB {
        static inline const Dia::Core::StringCRC kTypeId{ "LiveMsgB" };
    };

    struct HistMsg {
        static inline const Dia::Core::StringCRC kTypeId{ "HistMsg" };
    };

    namespace {
        // Finds the schema entry whose "typeId" matches, or a null Json::Value
        // if absent (Json::Value::isNull() true).
        const Json::Value& FindSchemaEntry(const Json::Value& schema, const char* typeId) {
            for (const auto& entry : schema) {
                if (entry["typeId"].asString() == typeId) {
                    return entry;
                }
            }
            static const Json::Value kNull;
            return kNull;
        }
    } // namespace

    // =========================================================================
    // AC-2: Schema tab — alphabetical order; typeId/producerIds/routerId/
    // subscriberIds per row, including a type observed through both routers.
    // =========================================================================
    TEST(MessageBusDebugDomain, SchemaTab_AlphabeticalOrder_CorrectFieldsPerType) {
        TestableMessageBusModule module;
        module.Start();
        Bus& bus = module.GetBus();

        ASSERT_TRUE((bus.RegisterType<SchemaMsgBoth, 8>()));
        ASSERT_TRUE((bus.RegisterType<SchemaMsgBroadcast, 8>()));
        ASSERT_TRUE((bus.RegisterType<SchemaMsgEntity, 8>()));

        bus.RegisterProducer<SchemaMsgBoth>(Dia::Core::StringCRC("ProducerBoth"));
        bus.RegisterProducer<SchemaMsgBroadcast>(Dia::Core::StringCRC("ProducerBroadcast"));
        bus.RegisterProducer<SchemaMsgEntity>(Dia::Core::StringCRC("ProducerEntity"));

        auto hBoth = bus.Subscribe<SchemaMsgBoth>(Dia::Core::StringCRC("SubBoth"), [](const SchemaMsgBoth&) {});
        auto hBroadcast = bus.Subscribe<SchemaMsgBroadcast>(Dia::Core::StringCRC("SubBroadcast"), [](const SchemaMsgBroadcast&) {});
        auto hEntity = bus.Subscribe<SchemaMsgEntity>(Dia::Core::StringCRC("SubEntity"), [](const SchemaMsgEntity&) {});

        // SchemaMsgBroadcast: broadcast only.
        bus.Broadcast(SchemaMsgBroadcast{});
        // SchemaMsgEntity: entity only.
        bus.Post<SchemaMsgEntity>(Dia::Mailbox::Address{ Bus::kEntityRouterId, 0 }, SchemaMsgEntity{});
        // SchemaMsgBoth: both routers, within the same tick.
        bus.Post<SchemaMsgBoth>(Dia::Mailbox::Address{ Bus::kBroadcastRouterId, 0 }, SchemaMsgBoth{});
        bus.Post<SchemaMsgBoth>(Dia::Mailbox::Address{ Bus::kEntityRouterId, 0 }, SchemaMsgBoth{});

        module.Update(0.0f); // dispatch — records observed-router flags

        MessageBusDebugDomain domain(module);
        Json::Value state;
        domain.GetJSONState(state);

        const Json::Value& schema = state["schema"];
        ASSERT_TRUE(schema.isArray());
        ASSERT_EQ(schema.size(), 3u);

        // Alphabetical: SchemaMsgBoth < SchemaMsgBroadcast < SchemaMsgEntity.
        EXPECT_EQ(schema[0]["typeId"].asString(), "SchemaMsgBoth");
        EXPECT_EQ(schema[1]["typeId"].asString(), "SchemaMsgBroadcast");
        EXPECT_EQ(schema[2]["typeId"].asString(), "SchemaMsgEntity");

        const Json::Value& bothEntry = FindSchemaEntry(schema, "SchemaMsgBoth");
        ASSERT_FALSE(bothEntry.isNull());
        EXPECT_EQ(bothEntry["routerId"].asString(), "both");
        ASSERT_EQ(bothEntry["producerIds"].size(), 1u);
        EXPECT_EQ(bothEntry["producerIds"][0].asString(), "ProducerBoth");
        ASSERT_EQ(bothEntry["subscriberIds"].size(), 1u);
        EXPECT_EQ(bothEntry["subscriberIds"][0].asString(), "SubBoth");

        const Json::Value& broadcastEntry = FindSchemaEntry(schema, "SchemaMsgBroadcast");
        ASSERT_FALSE(broadcastEntry.isNull());
        EXPECT_EQ(broadcastEntry["routerId"].asString(), "broadcast");
        ASSERT_EQ(broadcastEntry["producerIds"].size(), 1u);
        EXPECT_EQ(broadcastEntry["producerIds"][0].asString(), "ProducerBroadcast");
        ASSERT_EQ(broadcastEntry["subscriberIds"].size(), 1u);
        EXPECT_EQ(broadcastEntry["subscriberIds"][0].asString(), "SubBroadcast");

        const Json::Value& entityEntry = FindSchemaEntry(schema, "SchemaMsgEntity");
        ASSERT_FALSE(entityEntry.isNull());
        EXPECT_EQ(entityEntry["routerId"].asString(), "entity");
        ASSERT_EQ(entityEntry["producerIds"].size(), 1u);
        EXPECT_EQ(entityEntry["producerIds"][0].asString(), "ProducerEntity");
        ASSERT_EQ(entityEntry["subscriberIds"].size(), 1u);
        EXPECT_EQ(entityEntry["subscriberIds"][0].asString(), "SubEntity");

        module.Stop();
    }

    // =========================================================================
    // AC-3: Live tab — last completed tick's entries sorted by count desc;
    // totals row sums count/deliveries across all entries.
    // =========================================================================
    TEST(MessageBusDebugDomain, LiveTab_SortedByCountDescending_TotalsCorrect) {
        TestableMessageBusModule module;
        module.Start();
        Bus& bus = module.GetBus();

        ASSERT_TRUE((bus.RegisterType<LiveMsgA, 16>()));
        ASSERT_TRUE((bus.RegisterType<LiveMsgB, 16>()));

        int aCount1 = 0, aCount2 = 0, bCount = 0;
        auto hA1 = bus.Subscribe<LiveMsgA>(Dia::Core::StringCRC("a1"), [&aCount1](const LiveMsgA&) { ++aCount1; });
        auto hA2 = bus.Subscribe<LiveMsgA>(Dia::Core::StringCRC("a2"), [&aCount2](const LiveMsgA&) { ++aCount2; });
        auto hB  = bus.Subscribe<LiveMsgB>(Dia::Core::StringCRC("b1"), [&bCount](const LiveMsgB&) { ++bCount; });

        // LiveMsgA: 3 messages x 2 subscribers = 6 deliveries.
        for (int i = 0; i < 3; ++i) bus.Broadcast(LiveMsgA{});
        // LiveMsgB: 5 messages x 1 subscriber = 5 deliveries. Higher count,
        // must sort ahead of LiveMsgA despite fewer deliveries.
        for (int i = 0; i < 5; ++i) bus.Broadcast(LiveMsgB{});

        module.Update(0.0f);

        MessageBusDebugDomain domain(module);
        Json::Value state;
        domain.GetJSONState(state);

        const Json::Value& live = state["live"];
        EXPECT_EQ(live["tickIndex"].asUInt64(), 0u);
        EXPECT_EQ(live["totalMessages"].asUInt64(), 8u);   // 3 + 5
        EXPECT_EQ(live["totalDeliveries"].asUInt64(), 11u); // 6 + 5

        const Json::Value& entries = live["entries"];
        ASSERT_EQ(entries.size(), 2u);
        EXPECT_EQ(entries[0]["typeId"].asString(), "LiveMsgB");
        EXPECT_EQ(entries[0]["count"].asUInt(), 5u);
        EXPECT_EQ(entries[0]["deliveries"].asUInt(), 5u);
        EXPECT_EQ(entries[0]["routerId"].asString(), "broadcast");
        EXPECT_EQ(entries[0]["pass"].asString(), "Primary");

        EXPECT_EQ(entries[1]["typeId"].asString(), "LiveMsgA");
        EXPECT_EQ(entries[1]["count"].asUInt(), 3u);
        EXPECT_EQ(entries[1]["deliveries"].asUInt(), 6u);

        module.Stop();
    }

    // =========================================================================
    // AC-4: History tab — per-tick totals for the last windowTicks snapshots;
    // droppedCount > 0 ticks distinguishable from clean ticks.
    // =========================================================================
    TEST(MessageBusDebugDomain, HistoryTab_PerTickTotals_DroppedTicksDistinguishable) {
        TestableMessageBusModule module;
        module.Start();
        Bus& bus = module.GetBus();

        // Capacity 2, DropOldest: a tick that posts more than 2 messages
        // drops the excess (see DiaMessageBusCore's
        // Ledger_DroppedCount_ReflectsOverflowThisTick for this exact pattern).
        ASSERT_TRUE((bus.RegisterType<HistMsg, 2>(Dia::Mailbox::OverflowPolicy::DropOldest)));

        bus.Broadcast(HistMsg{});
        module.Update(0.0f); // tick 0: 1 posted, 0 dropped

        for (int i = 0; i < 5; ++i) bus.Broadcast(HistMsg{});
        module.Update(0.0f); // tick 1: 5 posted, capacity 2 -> 3 dropped

        bus.Broadcast(HistMsg{});
        module.Update(0.0f); // tick 2: 1 posted, 0 dropped

        MessageBusDebugDomain domain(module);
        Json::Value state;
        domain.GetJSONState(state);

        const Json::Value& history = state["history"];
        ASSERT_EQ(history.size(), 3u);

        EXPECT_EQ(history[0]["tickIndex"].asUInt64(), 0u);
        EXPECT_EQ(history[0]["dropped"].asUInt(), 0u);

        EXPECT_EQ(history[1]["tickIndex"].asUInt64(), 1u);
        EXPECT_GT(history[1]["dropped"].asUInt(), 0u);
        EXPECT_EQ(history[1]["dropped"].asUInt(), 3u);

        EXPECT_EQ(history[2]["tickIndex"].asUInt64(), 2u);
        EXPECT_EQ(history[2]["dropped"].asUInt(), 0u);

        module.Stop();
    }

    // History window actually truncates the visible tail, not just the
    // reported historyWindowTicks number.
    TEST(MessageBusDebugDomain, SetHistoryWindow_TruncatesToLastNTicks) {
        TestableMessageBusModule module;
        module.Start();
        Bus& bus = module.GetBus();
        ASSERT_TRUE((bus.RegisterType<HistMsg, 8>()));

        for (int tick = 0; tick < 5; ++tick) {
            bus.Broadcast(HistMsg{});
            module.Update(0.0f);
        }
        // 5 ticks pushed (tickIndex 0..4).

        MessageBusDebugDomain domain(module);
        Json::Value windowArgs(Json::objectValue);
        windowArgs["ticks"] = 2;
        domain.OnCommand(Dia::Core::StringCRC("setHistoryWindow"), windowArgs);

        Json::Value state;
        domain.GetJSONState(state);

        EXPECT_EQ(state["historyWindowTicks"].asUInt(), 2u);
        const Json::Value& history = state["history"];
        ASSERT_EQ(history.size(), 2u);
        EXPECT_EQ(history[0]["tickIndex"].asUInt64(), 3u);
        EXPECT_EQ(history[1]["tickIndex"].asUInt64(), 4u);

        module.Stop();
    }

    // =========================================================================
    // AC-5: OnCommand("selectTab", {tab:"history"}) switches activeTab.
    // =========================================================================
    TEST(MessageBusDebugDomain, OnCommand_SelectTab_SwitchesActiveTab) {
        TestableMessageBusModule module;
        module.Start();

        MessageBusDebugDomain domain(module);

        Json::Value state;
        domain.GetJSONState(state);
        EXPECT_EQ(state["activeTab"].asString(), "live"); // default

        Json::Value args(Json::objectValue);
        args["tab"] = "history";
        domain.OnCommand(Dia::Core::StringCRC("selectTab"), args);

        Json::Value stateAfter;
        domain.GetJSONState(stateAfter);
        EXPECT_EQ(stateAfter["activeTab"].asString(), "history");

        args["tab"] = "schema";
        domain.OnCommand(Dia::Core::StringCRC("selectTab"), args);
        Json::Value stateSchema;
        domain.GetJSONState(stateSchema);
        EXPECT_EQ(stateSchema["activeTab"].asString(), "schema");

        module.Stop();
    }

    // =========================================================================
    // AC-6: OnCommand("setHistoryWindow", {ticks:30}) changes
    // historyWindowTicks; reflected in GetJSONState().
    // =========================================================================
    TEST(MessageBusDebugDomain, OnCommand_SetHistoryWindow_UpdatesReportedWindow) {
        TestableMessageBusModule module;
        module.Start();

        MessageBusDebugDomain domain(module);

        Json::Value stateBefore;
        domain.GetJSONState(stateBefore);
        EXPECT_EQ(stateBefore["historyWindowTicks"].asUInt(), 60u); // default

        Json::Value args(Json::objectValue);
        args["ticks"] = 30;
        domain.OnCommand(Dia::Core::StringCRC("setHistoryWindow"), args);

        Json::Value stateAfter;
        domain.GetJSONState(stateAfter);
        EXPECT_EQ(stateAfter["historyWindowTicks"].asUInt(), 30u);

        module.Stop();
    }

    TEST(MessageBusDebugDomain, OnCommand_SetHistoryWindow_OutOfRange_Ignored) {
        TestableMessageBusModule module;
        module.Start();

        MessageBusDebugDomain domain(module);

        Json::Value tooLow(Json::objectValue);
        tooLow["ticks"] = 0;
        domain.OnCommand(Dia::Core::StringCRC("setHistoryWindow"), tooLow);

        Json::Value tooHigh(Json::objectValue);
        tooHigh["ticks"] = 3601;
        domain.OnCommand(Dia::Core::StringCRC("setHistoryWindow"), tooHigh);

        Json::Value state;
        domain.GetJSONState(state);
        EXPECT_EQ(state["historyWindowTicks"].asUInt(), 60u); // unchanged from default

        module.Stop();
    }

    // =========================================================================
    // AC-7: MessageBusDebugDomain.h/.cpp are entirely #ifdef DIA_DEBUG — this
    // whole test file only compiles under DIA_DEBUG (see the outer #ifdef
    // DIA_DEBUG guard at the top of this file), matching
    // TestDiaMessageBusLedgerHistory.cpp's AC-5 approach: verified by code
    // inspection of the header/source guards, not a separate Release-config
    // build.
    // =========================================================================

    // =========================================================================
    // AC-8: identity — GetDomainId()/GetGroup()/HasWorldDrawers().
    // =========================================================================
    TEST(MessageBusDebugDomain, Identity_DomainIdGroupAndNoWorldDrawers) {
        TestableMessageBusModule module;
        module.Start();

        MessageBusDebugDomain domain(module);

        EXPECT_TRUE(domain.GetDomainId() == Dia::Core::StringCRC("MessageBus"));
        EXPECT_TRUE(domain.GetGroup() == Dia::Core::StringCRC("Systems"));
        EXPECT_FALSE(domain.HasWorldDrawers());

        module.Stop();
    }

} // namespace Dia::MessageBus::Testing

#endif // DIA_DEBUG
