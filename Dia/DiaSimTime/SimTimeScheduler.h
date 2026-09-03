#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Time/TimeRelative.h>
#include <DiaSimTime/SimTimeSchedulerFire.h>
#include <DiaStreams/EventStreamWriter.h>
#include <DiaStreams/IStreamConnector.h>
#include <cstdint>
#include <optional>

namespace Dia::SimTime {

    using ScheduleHandle = uint64_t;
    static constexpr ScheduleHandle kInvalidHandle = 0;

    // SimTimeScheduler
    // -------------------------------------------------------------------------
    // Fires events at specific *game* times (SimTime, not wall-clock). Cost is
    // proportional to events actually coming due, not to the number of pending
    // entries — near-term entries live in a bucketed timer wheel (cheap sweep),
    // far-future entries overflow into a min-heap and migrate into the wheel as
    // the clock catches up.
    //
    // Task 3.1 built the scheduling *engine*: Tick() pops all due entries and
    // re-arms recurring ones, exposing what fired through an out-parameter
    // overload (a test-visibility hook). Task 3.2 adds real fan-out delivery:
    // Connect() wires an EventStreamWriter<SimTimeSchedulerFire>, and every
    // fired entry is also Send() through it (Q2 — fan-out via EventStreamStore,
    // no callback path; consumers filter by targetSystemId). Pause/scale
    // correctness is inherent: Tick(currentTime) never reads any wall-clock
    // source itself, so whatever time domain the caller passes in (paused,
    // scaled, fast-forwarded) is exactly what governs firing.
    //
    // Handles (ScheduleHandle) pack a 32-bit slot index (low dword) and a 32-bit
    // generation (high dword). Generation starts at 1 and is bumped every time a
    // slot is freed, so a stale copy of an old handle never resolves to whatever
    // new entry later reuses that slot (kInvalidHandle == 0 is never a valid
    // packed value because a live generation is always >= 1).
    //
    // PD-004: public storage is DynamicArrayC only; std::optional appears solely
    // as an internal implementation detail (TimeRelative is not default-
    // constructible), never in the public signatures.
    class SimTimeScheduler
    {
    public:
        // --- Tuning (documented, fixed-capacity like the rest of the codebase) ---
        // Total scheduled-entry capacity across wheel + heap combined.
        static constexpr int          kMaxEntries      = 256;
        // Timer-wheel geometry. Bucket width 10ms x 64 buckets => 640ms horizon.
        // Entries due within the horizon of the scheduler's current time live in
        // the wheel; anything further out overflows into the heap.
        static constexpr int          kNumBuckets      = 64;
        static constexpr int64_t      kBucketWidthMicros = 10000; // 10 ms
        // Max entries that may share a single 10ms wheel bucket, and max entries
        // parked in the overflow heap at once. Both DIA_ASSERT on overflow.
        static constexpr unsigned int kBucketCapacity  = 64;
        static constexpr unsigned int kHeapCapacity    = 256;

        // Read-only snapshot of one currently-pending entry, handed out by
        // GetPendingEntries() (Task 4.7) so SimTimeSaveState::Serialize can walk
        // every live scheduled entry across BOTH the wheel and the overflow heap
        // without firing / removing / re-arming anything. recurringInterval is
        // only meaningful when recurring == true (Zero() otherwise).
        struct PendingEntryView
        {
            Core::TimeAbsolute  fireTime          = Core::TimeAbsolute::Zero();
            Core::StringCRC     eventType;
            Core::StringCRC     targetSystemId;
            bool                recurring         = false;
            Core::TimeRelative  recurringInterval = Core::TimeRelative::Zero(); // valid iff recurring
        };

        SimTimeScheduler();

        // Wire this scheduler's fire events into a real EventStreamStore.
        // Stream ID: StringCRC("SimTimeSchedulerFire"). Not called from a
        // Module::OnConnectStreams() yet (DiaSimTimeModule doesn't exist until
        // Task 4.4) — call this directly wherever the scheduler is owned until
        // then. Safe to never call: Send() below no-ops when disconnected.
        // maxReaders defaults higher than EventStreamStore's own default (8)
        // since scheduler fire events are a fan-out point many systems may
        // realistically subscribe to.
        void  Connect(Dia::ApplicationFlow::IStreamConnector& connector,
                      unsigned int maxReaders = 16);

        // Fire once at absolute game time.
        ScheduleHandle  ScheduleAt(Core::TimeAbsolute time,
                                   Core::StringCRC eventType,
                                   Core::StringCRC targetSystemId);

        // Fire once after delay from the scheduler's current game time
        // (last time seen by Tick(), or Zero() before the first Tick()).
        ScheduleHandle  ScheduleAfter(Core::TimeRelative delay,
                                      Core::StringCRC eventType,
                                      Core::StringCRC targetSystemId);

        // Fire repeatedly at interval. First fire is at now + interval; each
        // re-arm advances by exactly one interval from the *previous scheduled*
        // time (not from currentTime) to avoid drift. A recurring entry fires at
        // most once per Tick() call — if a single Tick advances far enough to
        // cover several intervals, it catches up one interval per subsequent Tick.
        ScheduleHandle  ScheduleRecurring(Core::TimeRelative interval,
                                          Core::StringCRC eventType,
                                          Core::StringCRC targetSystemId);

        // Cancel a pending entry. No-op on kInvalidHandle or a stale handle.
        void  Cancel(ScheduleHandle handle);

        // Move a pending entry's fire time. No-op on kInvalidHandle or a stale
        // handle. The handle stays valid; the entry migrates between wheel and
        // heap as required by its new time.
        void  Reschedule(ScheduleHandle handle, Core::TimeAbsolute newTime);

        // Advance the scheduler to currentTime, firing every entry whose
        // scheduledTime <= currentTime (in fire-time order) and re-arming
        // recurring entries. This overload does not report what fired.
        void  Tick(Core::TimeAbsolute currentTime);

        // Same as Tick(currentTime) but appends the payload of every entry that
        // fired this call into outFired, bounded by N. Entries beyond N still
        // fire / re-arm correctly — they just aren't reported (test-visibility
        // hook, not a production capacity limit).
        template<unsigned int N>
        void  Tick(Core::TimeAbsolute currentTime,
                   Dia::Core::Containers::DynamicArrayC<SimTimeSchedulerFire, N>& outFired)
        {
            Dia::Core::Containers::DynamicArrayC<SimTimeSchedulerFire, kMaxEntries> all;
            TickInternal(currentTime, all);
            for (unsigned int i = 0; i < all.Size() && !outFired.IsFull(); ++i)
            {
                outFired.Add(all[i]);
            }
        }

        // Number of entries currently pending across wheel + heap combined.
        int   GetQueueDepth() const;

        // Read-only peek at every currently-pending entry, wheel + heap combined,
        // in no particular order (Serialize doesn't need fire-time ordering).
        // Bounded by N — entries beyond N are silently not included (kMaxEntries
        // == 256 total cap already bounds realistic sizes; size N to that cap to
        // guarantee completeness). This walks the same storage TickInternal does
        // and applies the identical stale-ref rule (!s.active || ref.epoch !=
        // s.epoch => skip), but is strictly READ-ONLY: it never frees a slot,
        // removes a Ref, or re-arms anything. Only one live Ref per slot exists
        // (every placement bumps the slot epoch), so no entry is reported twice.
        // Defined inline (template): the whole walk lives in the member body so
        // the private Ref / Slot nested types are in complete-class scope. A
        // local lambda applies the same live-check + view-fill to both wheel and
        // heap refs, keeping it read-only (no FreeSlot / RemoveAt / re-arm).
        template<unsigned int N>
        void  GetPendingEntries(Dia::Core::Containers::DynamicArrayC<PendingEntryView, N>& outEntries) const
        {
            auto appendIfLive = [&](const Ref& ref)
            {
                if (outEntries.IsFull())
                {
                    return;
                }
                const Slot& s = mSlots[ref.slotIndex];
                if (!s.active || ref.epoch != s.epoch)
                {
                    return; // stale ref — same rule TickInternal uses
                }
                PendingEntryView view;
                view.fireTime       = Core::TimeAbsolute::CreateFromMicroseconds(
                                          static_cast<long long>(s.scheduledMicros));
                view.eventType      = s.payload.eventType;
                view.targetSystemId = s.payload.targetSystemId;
                view.recurring      = s.recurring;
                if (s.recurring && s.interval.has_value())
                {
                    view.recurringInterval = s.interval.value();
                }
                outEntries.Add(view);
            };

            for (int b = 0; b < kNumBuckets; ++b)
            {
                const Dia::Core::Containers::DynamicArrayC<Ref, kBucketCapacity>& bucket = mWheel[b];
                for (unsigned int i = 0; i < bucket.Size(); ++i)
                {
                    appendIfLive(bucket[i]);
                }
            }
            for (unsigned int i = 0; i < mHeap.Size(); ++i)
            {
                appendIfLive(mHeap[i]);
            }
        }

        // Wheel/heap store lightweight references back into the slot pool. The
        // epoch makes lazy deletion safe: Cancel/Reschedule/re-arm just bump the
        // owning slot's epoch, so any stale Ref still physically parked in a
        // bucket or the heap is detected (ref.epoch != slot.epoch) and dropped
        // when next visited — no eager removal from the containers is required.
        // scheduledMicros is snapshotted here (not read live off the slot) so the
        // heap ordering is stable even if the slot is later Rescheduled.
        struct Ref
        {
            uint32_t slotIndex     = 0;
            uint32_t epoch         = 0;
            int64_t  scheduledMicros = 0;
        };

        struct Slot
        {
            bool                          active = false;
            uint32_t                      generation = 1;   // handle validity
            uint32_t                      epoch = 0;         // placement freshness
            int64_t                       scheduledMicros = 0;
            bool                          recurring = false;
            std::optional<Core::TimeRelative> interval;      // set iff recurring
            SimTimeSchedulerFire          payload;
        };

        // Internal firing path used by both Tick() overloads. Fills firedOut with
        // every payload that fired this call (bounded by kMaxEntries == total
        // capacity, so it can never overflow).
        void TickInternal(Core::TimeAbsolute currentTime,
                          Dia::Core::Containers::DynamicArrayC<SimTimeSchedulerFire, kMaxEntries>& firedOut);

        // Allocate/free slots in the pool (generation-safe object pool).
        uint32_t       AllocSlot();
        void           FreeSlot(uint32_t slotIndex);
        ScheduleHandle CommonSchedule(int64_t fireMicros, bool recurring,
                                      std::optional<Core::TimeRelative> interval,
                                      Core::StringCRC eventType, Core::StringCRC targetSystemId);

        // Insert a fresh Ref for slotIndex into the wheel or heap depending on
        // where its (snapshotted) time falls relative to the current horizon.
        void  PlaceRef(uint32_t slotIndex);

        // Move due heap entries into the wheel now that the horizon has advanced.
        void  MigrateHeapToWheel();

        // Add an already-valid Ref straight into its wheel bucket (no epoch bump);
        // used by PlaceRef and by heap->wheel migration.
        void  PushToWheel(const Ref& ref);

        // Min-heap primitives keyed by Ref::scheduledMicros.
        void  HeapPush(const Ref& ref);
        void  HeapPopRoot();
        void  HeapSiftUp(unsigned int index);
        void  HeapSiftDown(unsigned int index);

        // Handle packing helpers.
        static ScheduleHandle Pack(uint32_t index, uint32_t generation);
        static uint32_t       IndexOf(ScheduleHandle handle);
        static uint32_t       GenerationOf(ScheduleHandle handle);
        Slot*                 ResolveSlot(ScheduleHandle handle); // nullptr if stale

        int64_t BaseBucketOf(int64_t micros) const { return micros / kBucketWidthMicros; }

        Dia::Core::Containers::DynamicArrayC<Slot, kMaxEntries>       mSlots;
        Dia::Core::Containers::DynamicArrayC<uint32_t, kMaxEntries>   mFreeSlots;
        Dia::Core::Containers::DynamicArrayC<Ref, kBucketCapacity>    mWheel[kNumBuckets];
        Dia::Core::Containers::DynamicArrayC<Ref, kHeapCapacity>      mHeap;

        int64_t mCurrentMicros = 0;   // last time seen by Tick(); Zero() before first Tick
        int64_t mBaseBucket    = 0;   // absolute bucket index at the wheel window front
        int     mPendingCount  = 0;

        // Real fan-out destination for fired entries (Task 3.2 / Q2). Owned as
        // a member, constructed with owner=nullptr: EventStreamWriter's owner
        // param is only used to stamp Event::senderCrc, and falls back to a
        // "$framework" sentinel when null — fine for a scheduler that isn't
        // itself a Module. Send() safely no-ops until Connect() is called.
        Dia::ApplicationFlow::EventStreamWriter<SimTimeSchedulerFire> mFireWriter{
            nullptr, Core::StringCRC("SimTimeSchedulerFire") };
    };

} // namespace Dia::SimTime
