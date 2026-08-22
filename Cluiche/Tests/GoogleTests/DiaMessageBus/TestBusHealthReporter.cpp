// TestBusHealthReporter.cpp
//
// Covers BusHealthReporter's two practically-exercisable paths: OK on a
// healthy bus, and Degraded on a tick with message drops, resetting back to
// OK on a subsequent clean tick. The capacity-headroom paths (type registry
// / handler pool >= 90% of kMaxTypes=32 / kMaxHandlers=256) are simple
// arithmetic over Bus::GetRegisteredTypeCount()/GetTypeCapacity() and
// GetHandlerCount()/GetHandlerCapacity() — verified by code review rather
// than a dedicated test, since exercising them for real would require
// registering ~29 distinct compile-time message types.

#include <gtest/gtest.h>

#include <DiaMessageBus/Bus.h>
#include <DiaMessageBus/BusHealthReporter.h>
#include <DiaMailbox/MailboxTypes.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::MessageBus;
using Dia::Core::StringCRC;
using Dia::Observation::Health::HealthStatus;

namespace {
    struct HealthTestMsg {
        static inline const StringCRC kTypeId{ "HealthTestMsg" };
        uint32_t value = 0;
    };
}

TEST(BusHealthReporter, FreshBus_ReportsOK) {
    Bus bus;
    bus.Initialize();
    BusHealthReporter reporter(StringCRC("test"), bus);

    reporter.Check();

    EXPECT_EQ(reporter.Report().status, HealthStatus::kOK);
}

TEST(BusHealthReporter, HealthyTickAfterRegisterAndDeliver_StaysOK) {
    Bus bus;
    bus.Initialize();
    BusHealthReporter reporter(StringCRC("test"), bus);

    bus.RegisterType<HealthTestMsg, 8>(Dia::Mailbox::OverflowPolicy::DropOldest);
    auto handle = bus.Subscribe<HealthTestMsg>(StringCRC("sub"), [](const HealthTestMsg&) {});
    ASSERT_TRUE(handle.IsValid());

    bus.Broadcast(HealthTestMsg{ 1 });
    bus.Update();
    reporter.Check();

    EXPECT_EQ(reporter.Report().status, HealthStatus::kOK);
}

TEST(BusHealthReporter, TickWithDrops_ReportsDegraded_ThenClearsOnCleanTick) {
    Bus bus;
    bus.Initialize();
    BusHealthReporter reporter(StringCRC("test"), bus);

    // Capacity 1 + DropOldest: the second Broadcast before any Update()
    // forces the first message out, incrementing Mailbox's totalDropped —
    // Bus::DrainTrampoline recovers that delta into the ledger's
    // droppedCount on the next Update().
    bus.RegisterType<HealthTestMsg, 1>(Dia::Mailbox::OverflowPolicy::DropOldest);
    bus.Broadcast(HealthTestMsg{ 1 });
    bus.Broadcast(HealthTestMsg{ 2 }); // drops the first

    bus.Update();
    reporter.Check();

    ASSERT_GT(bus.GetLastTickLedger().droppedCount, 0u);
    EXPECT_EQ(reporter.Report().status, HealthStatus::kDegraded);

    // A subsequent tick with no drops must clear back to OK — Degraded
    // reflects only the most recently Check()-ed tick, matching
    // GetLastTickLedger()'s own last-tick-only semantics.
    bus.Update();
    reporter.Check();

    EXPECT_EQ(bus.GetLastTickLedger().droppedCount, 0u);
    EXPECT_EQ(reporter.Report().status, HealthStatus::kOK);
}
