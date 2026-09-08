// TestDiaMessageBusLedgerHistory.cpp
//
// Covers Dia::MessageBus::LedgerHistory — the fixed-size, allocation-free
// ring buffer retaining the last kLedgerCapacity completed-tick
// LedgerSnapshots, and MessageBusModule's automatic push of the
// completed-tick snapshot into it from DoUpdate().
//
// AC-1: N <= capacity pushes retains all N snapshots.
// AC-2: N > capacity pushes evicts oldest (FIFO), Count() stays at capacity.
// AC-3: ForEachSnapshot visits oldest-first (ascending tickIndex).
// AC-4: ForEachDroppedSnapshot visits only ticks with droppedCount > 0.
// AC-5: LedgerHistory/mLedgerHistory only exist under DIA_DEBUG — verified by
//       code inspection (grep for #ifdef DIA_DEBUG guards), not a separate
//       Release build+link-map inspection. See report for the grep output.
// AC-6: zero heap allocation after construction — verified by code
//       inspection (LedgerHistory's only member data is a fixed C array
//       LedgerSnapshot mSlots[kLedgerCapacity] plus two uint32_t indices; no
//       new/malloc anywhere in LedgerHistory.h/.cpp). This codebase has no
//       existing allocator-tracking test helper (_CrtMemCheckpoint or
//       similar) to grep for, so no runtime allocation-count test is added.
// AC-7: MessageBusModule::DoUpdate() pushes mBus.GetLastTickLedger() into
//       mLedgerHistory right after mBus.Update() returns.

#ifdef DIA_DEBUG

#include <gtest/gtest.h>
#include <vector>

#include <DiaMessageBus/LedgerHistory.h>
#include <DiaMessageBus/MessageBusModule.h>
#include <DiaMessageBus/Bus.h>

namespace Dia::MessageBus::Testing {

    // -------------------------------------------------------------------------
    // Testable module subclass — exposes protected lifecycle hooks, mirroring
    // TestDiaMessageBusCore.cpp's TestableMessageBusModule.
    // -------------------------------------------------------------------------
    class TestableMessageBusModule : public Dia::MessageBus::MessageBusModule {
    public:
        void Start() { DoStart(); }
        void Update(float dt)
        {
            Dia::SimTime::SimTimeContext ctx{
                Dia::Core::TimeAbsolute::Zero(),
                Dia::Core::TimeRelative::CreateFromSeconds(dt),
                0, 1.0f, false
            };
            DoUpdate(ctx);
        }
        void Stop()           { DoStop(); }
    };

    // Test-only message type so a real tick has something to route/drop.
    struct LedgerHistoryTestMsg {
        static inline const Dia::Core::StringCRC kTypeId{ "LedgerHistoryTestMsg" };
        int value = 0;
    };

    namespace {

        // Builds a snapshot with a distinguishing tickIndex and, optionally,
        // a non-zero droppedCount — LedgerSnapshot's fields are plain public
        // data, so tests can construct snapshots directly without going
        // through Bus/Mailbox.
        LedgerSnapshot MakeSnapshot(uint64_t tickIndex, uint32_t droppedCount = 0) {
            LedgerSnapshot snapshot;
            snapshot.tickIndex    = tickIndex;
            snapshot.timestampUs  = tickIndex * 1000ull;
            snapshot.droppedCount = droppedCount;
            return snapshot;
        }

    } // namespace

    // =========================================================================
    // AC-1: N <= kLedgerCapacity pushes retains all N snapshots.
    // =========================================================================
    TEST(DiaMessageBusLedgerHistory, Push_NLessThanCapacity_RetainsAllN) {
        LedgerHistory history;
        EXPECT_EQ(history.Count(), 0u);

        const uint32_t kN = 5;
        for (uint32_t i = 0; i < kN; ++i) {
            history.Push(MakeSnapshot(i));
        }

        EXPECT_EQ(history.Count(), kN);

        std::vector<uint64_t> seenTicks;
        history.ForEachSnapshot([&seenTicks](const LedgerSnapshot& snapshot) {
            seenTicks.push_back(snapshot.tickIndex);
        });

        ASSERT_EQ(seenTicks.size(), kN);
        for (uint32_t i = 0; i < kN; ++i) {
            EXPECT_EQ(seenTicks[i], i);
        }
    }

    // =========================================================================
    // AC-2: N > kLedgerCapacity pushes evicts oldest (FIFO); Count() stays
    // pinned at kLedgerCapacity.
    // =========================================================================
    TEST(DiaMessageBusLedgerHistory, Push_NGreaterThanCapacity_EvictsOldestFIFO) {
        LedgerHistory history;

        const uint32_t kCapacity = LedgerHistory::kLedgerCapacity;
        const uint32_t kExtra    = 7;
        const uint32_t kTotal    = kCapacity + kExtra;

        for (uint32_t i = 0; i < kTotal; ++i) {
            history.Push(MakeSnapshot(i));
        }

        EXPECT_EQ(history.Count(), kCapacity);

        // Oldest surviving tick should be kExtra (ticks 0..kExtra-1 evicted).
        // Newest should be kTotal - 1.
        std::vector<uint64_t> seenTicks;
        seenTicks.reserve(kCapacity);
        history.ForEachSnapshot([&seenTicks](const LedgerSnapshot& snapshot) {
            seenTicks.push_back(snapshot.tickIndex);
        });

        ASSERT_EQ(seenTicks.size(), kCapacity);
        EXPECT_EQ(seenTicks.front(), static_cast<uint64_t>(kExtra));
        EXPECT_EQ(seenTicks.back(), static_cast<uint64_t>(kTotal - 1));
    }

    // =========================================================================
    // AC-3: ForEachSnapshot visits oldest-first — each visited snapshot's
    // tickIndex is >= the previous one's, including across a wrap.
    // =========================================================================
    TEST(DiaMessageBusLedgerHistory, ForEachSnapshot_VisitsOldestFirst_AcrossWrap) {
        LedgerHistory history;

        const uint32_t kTotal = LedgerHistory::kLedgerCapacity + 42;
        for (uint32_t i = 0; i < kTotal; ++i) {
            history.Push(MakeSnapshot(i));
        }

        uint64_t previousTick = 0;
        bool     first        = true;
        uint32_t visitCount   = 0;
        history.ForEachSnapshot([&](const LedgerSnapshot& snapshot) {
            if (!first) {
                EXPECT_GE(snapshot.tickIndex, previousTick);
            }
            previousTick = snapshot.tickIndex;
            first        = false;
            ++visitCount;
        });

        EXPECT_EQ(visitCount, LedgerHistory::kLedgerCapacity);
    }

    // =========================================================================
    // AC-4: ForEachDroppedSnapshot visits only snapshots with
    // droppedCount > 0, still oldest-first, and is a strict subset of
    // ForEachSnapshot's visited set.
    // =========================================================================
    TEST(DiaMessageBusLedgerHistory, ForEachDroppedSnapshot_OnlyVisitsTicksWithDrops) {
        LedgerHistory history;

        // Ticks 0..9: even ticks are clean, odd ticks dropped some messages.
        for (uint64_t i = 0; i < 10; ++i) {
            const uint32_t dropped = (i % 2 == 1) ? static_cast<uint32_t>(i) : 0u;
            history.Push(MakeSnapshot(i, dropped));
        }

        std::vector<uint64_t> droppedTicks;
        history.ForEachDroppedSnapshot([&droppedTicks](const LedgerSnapshot& snapshot) {
            EXPECT_GT(snapshot.droppedCount, 0u);
            droppedTicks.push_back(snapshot.tickIndex);
        });

        const std::vector<uint64_t> expected = { 1, 3, 5, 7, 9 };
        EXPECT_EQ(droppedTicks, expected);
    }

    TEST(DiaMessageBusLedgerHistory, ForEachDroppedSnapshot_NoDrops_VisitsNone) {
        LedgerHistory history;
        for (uint64_t i = 0; i < 10; ++i) {
            history.Push(MakeSnapshot(i, 0));
        }

        uint32_t visitCount = 0;
        history.ForEachDroppedSnapshot([&visitCount](const LedgerSnapshot&) {
            ++visitCount;
        });

        EXPECT_EQ(visitCount, 0u);
    }

    // =========================================================================
    // AC-7: MessageBusModule::DoUpdate() automatically pushes the
    // completed-tick snapshot into mLedgerHistory immediately after
    // mBus.Update() returns.
    // =========================================================================
    TEST(DiaMessageBusLedgerHistory, ModuleDoUpdate_PushesCompletedTickIntoHistory) {
        TestableMessageBusModule module;
        module.Start();

        EXPECT_EQ(module.GetLedgerHistory().Count(), 0u);

        module.Update(0.0f);
        EXPECT_EQ(module.GetLedgerHistory().Count(), 1u);

        module.Update(0.0f);
        module.Update(0.0f);
        EXPECT_EQ(module.GetLedgerHistory().Count(), 3u);

        // The most recently pushed snapshot should match Bus's own
        // last-completed-tick view at the time of the push.
        const LedgerSnapshot& busLedger = module.GetBus().GetLastTickLedger();
        bool matchedLast = false;
        module.GetLedgerHistory().ForEachSnapshot([&](const LedgerSnapshot& snapshot) {
            // Iterate to the last one visited (oldest-first, so the final
            // visit is the newest).
            matchedLast = (snapshot.tickIndex == busLedger.tickIndex);
        });
        EXPECT_TRUE(matchedLast);

        module.Stop();
    }

    TEST(DiaMessageBusLedgerHistory, ModuleDoUpdate_RegisteredTypeAndBroadcast_RecordedInHistory) {
        TestableMessageBusModule module;
        module.Start();

        ASSERT_TRUE((module.GetBus().RegisterType<LedgerHistoryTestMsg, 8>()));

        module.Update(0.0f); // tick 0: registration only, no messages yet

        ASSERT_TRUE(module.GetBus().Broadcast(LedgerHistoryTestMsg{ 42 }));
        module.Update(0.0f); // tick 1: the broadcast above is drained/counted here

        EXPECT_EQ(module.GetLedgerHistory().Count(), 2u);

        uint32_t totalEntriesSeen = 0;
        module.GetLedgerHistory().ForEachSnapshot([&totalEntriesSeen](const LedgerSnapshot& snapshot) {
            totalEntriesSeen += snapshot.entries.Size();
        });
        EXPECT_GT(totalEntriesSeen, 0u);

        module.Stop();
    }

} // namespace Dia::MessageBus::Testing

#endif // DIA_DEBUG
