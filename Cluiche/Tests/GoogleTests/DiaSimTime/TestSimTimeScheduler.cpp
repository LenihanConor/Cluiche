////////////////////////////////////////////////////////////////////////////////
// Filename: TestSimTimeScheduler.cpp
// GoogleTest suite — DiaSimTime Task 3.1: SimTimeScheduler scheduling engine.
//
// Covers:
//   - insert-then-fire ordering (including across the wheel/heap boundary),
//   - ScheduleAfter / ScheduleRecurring relative to the scheduler's own clock,
//   - Cancel (never fires; slot/handle safely reusable afterwards),
//   - Reschedule (moves fire time, incl. wheel<->heap migration),
//   - handle reuse safety (a stale handle must not resolve to a recycled slot),
//   - GetQueueDepth across both structures.
//
// Task 3.1 is a pure DiaCore/DiaSimTime data structure — no DiaStreams /
// DiaApplicationFlow coupling — so these are plain unit tests, no Application.
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include <DiaSimTime/SimTimeScheduler.h>
#include <DiaSimTime/SimTimeSchedulerFire.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Time/TimeRelative.h>

using namespace Dia::Core;
using namespace Dia::Core::Containers;
using namespace Dia::SimTime;

namespace {

    TimeAbsolute Ms(int ms)  { return TimeAbsolute::CreateFromMilliseconds(ms); }
    TimeRelative RMs(int ms) { return TimeRelative::CreateFromMilliseconds(ms); }

    // Was a given eventType reported as fired this tick?
    template<unsigned int N>
    bool Contains(const DynamicArrayC<SimTimeSchedulerFire, N>& fired, const StringCRC& eventType)
    {
        for (unsigned int i = 0; i < fired.Size(); ++i)
        {
            if (fired[i].eventType == eventType) return true;
        }
        return false;
    }

} // anonymous namespace

// ---------------------------------------------------------------------------
// A freshly-constructed scheduler is empty.
// ---------------------------------------------------------------------------
TEST(SimTimeSchedulerTest, StartsEmpty)
{
    SimTimeScheduler sched;
    EXPECT_EQ(sched.GetQueueDepth(), 0);
}

// ---------------------------------------------------------------------------
// ScheduleAt fires exactly when currentTime >= scheduledTime, not before.
// ---------------------------------------------------------------------------
TEST(SimTimeSchedulerTest, ScheduleAtFiresAtOrAfterTime)
{
    SimTimeScheduler sched;
    sched.ScheduleAt(Ms(100), StringCRC("A"), StringCRC("sys"));
    EXPECT_EQ(sched.GetQueueDepth(), 1);

    DynamicArrayC<SimTimeSchedulerFire, 8> fired;
    sched.Tick(Ms(50), fired);
    EXPECT_EQ(fired.Size(), 0u) << "must not fire before its scheduled time";
    EXPECT_EQ(sched.GetQueueDepth(), 1);

    sched.Tick(Ms(100), fired);
    EXPECT_EQ(fired.Size(), 1u);
    EXPECT_TRUE(Contains(fired, StringCRC("A")));
    EXPECT_EQ(sched.GetQueueDepth(), 0) << "one-shot entry is gone after firing";
}

// ---------------------------------------------------------------------------
// Multiple entries fire in fire-time order within a single Tick.
// ---------------------------------------------------------------------------
TEST(SimTimeSchedulerTest, FiresInFireTimeOrder)
{
    SimTimeScheduler sched;
    // Insert out of order.
    sched.ScheduleAt(Ms(300), StringCRC("C"), StringCRC("sys"));
    sched.ScheduleAt(Ms(100), StringCRC("A"), StringCRC("sys"));
    sched.ScheduleAt(Ms(200), StringCRC("B"), StringCRC("sys"));

    DynamicArrayC<SimTimeSchedulerFire, 8> fired;
    sched.Tick(Ms(1000), fired);

    ASSERT_EQ(fired.Size(), 3u);
    EXPECT_EQ(fired[0].eventType, StringCRC("A"));
    EXPECT_EQ(fired[1].eventType, StringCRC("B"));
    EXPECT_EQ(fired[2].eventType, StringCRC("C"));
}

// ---------------------------------------------------------------------------
// Insert/fire ordering across the wheel/heap boundary. "far" is scheduled well
// beyond the wheel horizon (640ms) so it lands in the overflow heap, "near" in
// the wheel; once the clock catches up they fire together, in fire-time order.
// ---------------------------------------------------------------------------
TEST(SimTimeSchedulerTest, WheelHeapBoundaryOrdering)
{
    SimTimeScheduler sched;
    sched.ScheduleAt(Ms(5000), StringCRC("far"),  StringCRC("sys")); // heap (>640ms)
    sched.ScheduleAt(Ms(100),  StringCRC("near"), StringCRC("sys")); // wheel
    EXPECT_EQ(sched.GetQueueDepth(), 2);

    DynamicArrayC<SimTimeSchedulerFire, 8> fired;

    // Tick partway: only the near (wheel) entry is due; far stays in the heap.
    sched.Tick(Ms(200), fired);
    ASSERT_EQ(fired.Size(), 1u);
    EXPECT_EQ(fired[0].eventType, StringCRC("near"));
    EXPECT_EQ(sched.GetQueueDepth(), 1);

    // Advance past the far entry: it migrates out of the heap and fires.
    DynamicArrayC<SimTimeSchedulerFire, 8> fired2;
    sched.Tick(Ms(5000), fired2);
    ASSERT_EQ(fired2.Size(), 1u);
    EXPECT_EQ(fired2[0].eventType, StringCRC("far"));
    EXPECT_EQ(sched.GetQueueDepth(), 0);
}

// ---------------------------------------------------------------------------
// A far + near pair both becoming due in the SAME tick still order correctly.
// ---------------------------------------------------------------------------
TEST(SimTimeSchedulerTest, WheelHeapBothDueSameTickOrdered)
{
    SimTimeScheduler sched;
    sched.ScheduleAt(Ms(5000), StringCRC("far"),  StringCRC("sys")); // heap
    sched.ScheduleAt(Ms(100),  StringCRC("near"), StringCRC("sys")); // wheel

    DynamicArrayC<SimTimeSchedulerFire, 8> fired;
    sched.Tick(Ms(6000), fired);
    ASSERT_EQ(fired.Size(), 2u);
    EXPECT_EQ(fired[0].eventType, StringCRC("near"));
    EXPECT_EQ(fired[1].eventType, StringCRC("far"));
}

// ---------------------------------------------------------------------------
// ScheduleAfter is relative to the scheduler's current (last-Ticked) time.
// ---------------------------------------------------------------------------
TEST(SimTimeSchedulerTest, ScheduleAfterIsRelativeToCurrentTime)
{
    SimTimeScheduler sched;
    // Advance the clock to 1000ms first.
    DynamicArrayC<SimTimeSchedulerFire, 8> fired;
    sched.Tick(Ms(1000), fired);

    // Fire 500ms after "now" => absolute 1500ms.
    sched.ScheduleAfter(RMs(500), StringCRC("later"), StringCRC("sys"));

    sched.Tick(Ms(1400), fired);
    EXPECT_FALSE(Contains(fired, StringCRC("later"))) << "not yet due at 1400ms";

    sched.Tick(Ms(1500), fired);
    EXPECT_TRUE(Contains(fired, StringCRC("later"))) << "due at 1500ms";
    EXPECT_EQ(sched.GetQueueDepth(), 0);
}

// ---------------------------------------------------------------------------
// ScheduleRecurring re-arms at fixed interval; first fire at now + interval.
// ---------------------------------------------------------------------------
TEST(SimTimeSchedulerTest, RecurringFiresRepeatedlyAtInterval)
{
    SimTimeScheduler sched;
    sched.ScheduleRecurring(RMs(100), StringCRC("tick"), StringCRC("sys"));
    EXPECT_EQ(sched.GetQueueDepth(), 1);

    DynamicArrayC<SimTimeSchedulerFire, 8> fired;

    sched.Tick(Ms(50), fired);
    EXPECT_EQ(fired.Size(), 0u) << "first fire is at now+interval (100ms), not yet";

    sched.Tick(Ms(100), fired);
    EXPECT_EQ(fired.Size(), 1u);
    EXPECT_EQ(sched.GetQueueDepth(), 1) << "recurring entry stays pending after firing";

    DynamicArrayC<SimTimeSchedulerFire, 8> fired2;
    sched.Tick(Ms(200), fired2);
    EXPECT_EQ(fired2.Size(), 1u) << "fires again at the next interval";
    EXPECT_EQ(sched.GetQueueDepth(), 1);
}

// ---------------------------------------------------------------------------
// A cancelled entry never fires, and depth drops immediately.
// ---------------------------------------------------------------------------
TEST(SimTimeSchedulerTest, CancelPreventsFiring)
{
    SimTimeScheduler sched;
    ScheduleHandle h = sched.ScheduleAt(Ms(100), StringCRC("A"), StringCRC("sys"));
    EXPECT_EQ(sched.GetQueueDepth(), 1);

    sched.Cancel(h);
    EXPECT_EQ(sched.GetQueueDepth(), 0);

    DynamicArrayC<SimTimeSchedulerFire, 8> fired;
    sched.Tick(Ms(1000), fired);
    EXPECT_EQ(fired.Size(), 0u) << "cancelled entry must never fire";
}

// ---------------------------------------------------------------------------
// Cancelling an entry parked in the overflow heap also prevents firing.
// ---------------------------------------------------------------------------
TEST(SimTimeSchedulerTest, CancelHeapEntry)
{
    SimTimeScheduler sched;
    ScheduleHandle h = sched.ScheduleAt(Ms(5000), StringCRC("far"), StringCRC("sys")); // heap
    sched.Cancel(h);
    EXPECT_EQ(sched.GetQueueDepth(), 0);

    DynamicArrayC<SimTimeSchedulerFire, 8> fired;
    sched.Tick(Ms(6000), fired);
    EXPECT_EQ(fired.Size(), 0u);
}

// ---------------------------------------------------------------------------
// Cancel on an already-fired / invalid handle is a safe no-op.
// ---------------------------------------------------------------------------
TEST(SimTimeSchedulerTest, CancelInvalidAndDoubleCancelAreNoOps)
{
    SimTimeScheduler sched;
    sched.Cancel(kInvalidHandle);                 // no crash

    ScheduleHandle h = sched.ScheduleAt(Ms(100), StringCRC("A"), StringCRC("sys"));
    DynamicArrayC<SimTimeSchedulerFire, 8> fired;
    sched.Tick(Ms(100), fired);                   // fires and frees
    EXPECT_EQ(fired.Size(), 1u);

    sched.Cancel(h);                              // stale after fire — no-op
    sched.Cancel(h);                              // double cancel — no-op
    EXPECT_EQ(sched.GetQueueDepth(), 0);
}

// ---------------------------------------------------------------------------
// Reschedule moves the fire time (later); entry fires at the new time.
// ---------------------------------------------------------------------------
TEST(SimTimeSchedulerTest, RescheduleMovesFireTime)
{
    SimTimeScheduler sched;
    ScheduleHandle h = sched.ScheduleAt(Ms(100), StringCRC("A"), StringCRC("sys"));
    sched.Reschedule(h, Ms(500));

    DynamicArrayC<SimTimeSchedulerFire, 8> fired;
    sched.Tick(Ms(100), fired);
    EXPECT_EQ(fired.Size(), 0u) << "no longer due at old time";
    EXPECT_EQ(sched.GetQueueDepth(), 1);

    sched.Tick(Ms(500), fired);
    EXPECT_EQ(fired.Size(), 1u) << "fires at the new time";
    EXPECT_TRUE(Contains(fired, StringCRC("A")));
}

// ---------------------------------------------------------------------------
// Reschedule can migrate an entry from the wheel out to the heap and back.
// ---------------------------------------------------------------------------
TEST(SimTimeSchedulerTest, RescheduleMigratesBetweenWheelAndHeap)
{
    SimTimeScheduler sched;
    // Starts in the wheel (100ms).
    ScheduleHandle h = sched.ScheduleAt(Ms(100), StringCRC("A"), StringCRC("sys"));
    // Push it far out into the heap.
    sched.Reschedule(h, Ms(5000));

    DynamicArrayC<SimTimeSchedulerFire, 8> fired;
    sched.Tick(Ms(1000), fired);
    EXPECT_EQ(fired.Size(), 0u) << "now in heap, not due at 1s";

    // Pull it back into the wheel (near future) again.
    sched.Reschedule(h, Ms(1100));
    sched.Tick(Ms(1100), fired);
    EXPECT_EQ(fired.Size(), 1u);
    EXPECT_TRUE(Contains(fired, StringCRC("A")));
    EXPECT_EQ(sched.GetQueueDepth(), 0);
}

// ---------------------------------------------------------------------------
// Reschedule on a stale handle is a no-op (does not resurrect / corrupt).
// ---------------------------------------------------------------------------
TEST(SimTimeSchedulerTest, RescheduleStaleHandleIsNoOp)
{
    SimTimeScheduler sched;
    ScheduleHandle h = sched.ScheduleAt(Ms(100), StringCRC("A"), StringCRC("sys"));
    sched.Cancel(h);

    sched.Reschedule(h, Ms(500));     // stale — must not create a pending entry
    EXPECT_EQ(sched.GetQueueDepth(), 0);

    DynamicArrayC<SimTimeSchedulerFire, 8> fired;
    sched.Tick(Ms(1000), fired);
    EXPECT_EQ(fired.Size(), 0u);
}

// ---------------------------------------------------------------------------
// Handle reuse safety: after a slot is freed (via Cancel), a new entry may
// recycle that slot. A stale copy of the OLD handle must not resolve to the
// new entry — Cancel/Reschedule on it must be no-ops, and the new entry must
// fire normally.
// ---------------------------------------------------------------------------
TEST(SimTimeSchedulerTest, StaleHandleDoesNotAffectRecycledSlot)
{
    SimTimeScheduler sched;
    ScheduleHandle hOld = sched.ScheduleAt(Ms(100), StringCRC("OLD"), StringCRC("sys"));
    sched.Cancel(hOld);                       // frees the slot (generation bumped)

    // New entry very likely recycles the same slot index.
    ScheduleHandle hNew = sched.ScheduleAt(Ms(100), StringCRC("NEW"), StringCRC("sys"));
    EXPECT_NE(hOld, hNew) << "recycled slot must yield a distinct handle (generation bump)";
    EXPECT_EQ(sched.GetQueueDepth(), 1);

    // Operating on the stale handle must not touch the live entry.
    sched.Cancel(hOld);
    sched.Reschedule(hOld, Ms(9999));
    EXPECT_EQ(sched.GetQueueDepth(), 1) << "stale handle ops must be no-ops";

    DynamicArrayC<SimTimeSchedulerFire, 8> fired;
    sched.Tick(Ms(100), fired);
    ASSERT_EQ(fired.Size(), 1u);
    EXPECT_EQ(fired[0].eventType, StringCRC("NEW")) << "the live entry fires unaffected";
    EXPECT_FALSE(Contains(fired, StringCRC("OLD")));
}

// ---------------------------------------------------------------------------
// The out-param overload is bounded by N; entries beyond N still fire/free
// (depth reflects it) — they're just not reported.
// ---------------------------------------------------------------------------
TEST(SimTimeSchedulerTest, TickOutParamBoundedByCapacity)
{
    SimTimeScheduler sched;
    for (int i = 0; i < 5; ++i)
    {
        sched.ScheduleAt(Ms(100 + i), StringCRC("E"), StringCRC("sys"));
    }
    EXPECT_EQ(sched.GetQueueDepth(), 5);

    DynamicArrayC<SimTimeSchedulerFire, 2> fired; // capacity 2 < 5 due
    sched.Tick(Ms(1000), fired);
    EXPECT_EQ(fired.Size(), 2u) << "reporting is capped at N";
    EXPECT_EQ(sched.GetQueueDepth(), 0) << "but all 5 actually fired and were freed";
}

// ---------------------------------------------------------------------------
// The no-out-param Tick still fires everything (depth proves it).
// ---------------------------------------------------------------------------
TEST(SimTimeSchedulerTest, PlainTickFiresWithoutReporting)
{
    SimTimeScheduler sched;
    sched.ScheduleAt(Ms(100), StringCRC("A"), StringCRC("sys"));
    sched.ScheduleAt(Ms(5000), StringCRC("far"), StringCRC("sys"));
    EXPECT_EQ(sched.GetQueueDepth(), 2);

    sched.Tick(Ms(6000));
    EXPECT_EQ(sched.GetQueueDepth(), 0);
}
