#include <DiaSimTime/SimTimeScheduler.h>
#include <DiaApplicationFlow/Module.h>  // required for EventStreamWriter<T>::Send()'s
                                        // mOwner->GetInstanceId() to compile (owner is
                                        // always nullptr here, but the template still
                                        // needs Module's complete type).
#include <DiaCore/Core/Assert.h>

namespace Dia::SimTime {

    using Dia::Core::Containers::DynamicArrayC;

    SimTimeScheduler::SimTimeScheduler()
    {
        mCurrentMicros = 0;                 // Zero() before the first Tick()
        mBaseBucket    = BaseBucketOf(mCurrentMicros);
        mPendingCount  = 0;
    }

    // --- Handle packing ------------------------------------------------------
    // Low 32 bits = slot index, high 32 bits = generation. Generation is always
    // >= 1 for a live slot, so a valid handle is never 0 (== kInvalidHandle).
    ScheduleHandle SimTimeScheduler::Pack(uint32_t index, uint32_t generation)
    {
        return (static_cast<uint64_t>(generation) << 32) | static_cast<uint64_t>(index);
    }

    uint32_t SimTimeScheduler::IndexOf(ScheduleHandle handle)
    {
        return static_cast<uint32_t>(handle & 0xFFFFFFFFull);
    }

    uint32_t SimTimeScheduler::GenerationOf(ScheduleHandle handle)
    {
        return static_cast<uint32_t>(handle >> 32);
    }

    SimTimeScheduler::Slot* SimTimeScheduler::ResolveSlot(ScheduleHandle handle)
    {
        if (handle == kInvalidHandle)
        {
            return nullptr;
        }
        const uint32_t index = IndexOf(handle);
        if (index >= mSlots.Size())
        {
            return nullptr;
        }
        Slot& s = mSlots[index];
        if (!s.active || s.generation != GenerationOf(handle))
        {
            return nullptr; // stale handle into a freed / recycled slot
        }
        return &s;
    }

    // --- Slot pool -----------------------------------------------------------
    uint32_t SimTimeScheduler::AllocSlot()
    {
        if (mFreeSlots.Size() > 0)
        {
            const uint32_t index = mFreeSlots.Back();
            mFreeSlots.RemoveAt(mFreeSlots.Size() - 1);
            Slot& s = mSlots[index];
            s.active    = true;
            s.recurring = false;
            s.interval.reset();
            return index;
        }

        DIA_ASSERT(!mSlots.IsFull(),
                   "SimTimeScheduler: entry capacity exhausted (%d entries)", kMaxEntries);
        const uint32_t index = mSlots.Size();
        Slot fresh;
        fresh.active     = true;
        fresh.generation = 1;   // never 0 -> handle never collides with kInvalidHandle
        fresh.epoch      = 0;
        mSlots.Add(fresh);
        return index;
    }

    void SimTimeScheduler::FreeSlot(uint32_t slotIndex)
    {
        Slot& s = mSlots[slotIndex];
        if (!s.active)
        {
            return;
        }
        s.active = false;
        s.interval.reset();
        ++s.generation;                 // any surviving copy of the old handle is now stale
        ++s.epoch;                      // any surviving Ref in wheel/heap is now stale
        mFreeSlots.Add(slotIndex);
        --mPendingCount;
    }

    // --- Placement -----------------------------------------------------------
    void SimTimeScheduler::PushToWheel(const Ref& ref)
    {
        const int64_t absBucket   = BaseBucketOf(ref.scheduledMicros);
        // Clamp past-due entries onto the window front so they can't alias with a
        // future entry a full revolution away (they get fired next Tick anyway).
        const int64_t placeBucket = (absBucket < mBaseBucket) ? mBaseBucket : absBucket;
        const unsigned int slot   = static_cast<unsigned int>(placeBucket % kNumBuckets);
        DIA_ASSERT(!mWheel[slot].IsFull(),
                   "SimTimeScheduler: wheel bucket %u full (>%u entries in one %lldus window)",
                   slot, kBucketCapacity, static_cast<long long>(kBucketWidthMicros));
        mWheel[slot].Add(ref);
    }

    void SimTimeScheduler::PlaceRef(uint32_t slotIndex)
    {
        Slot& s = mSlots[slotIndex];
        ++s.epoch;                      // fresh placement -> older Refs to this slot go stale
        Ref ref;
        ref.slotIndex       = slotIndex;
        ref.epoch           = s.epoch;
        ref.scheduledMicros = s.scheduledMicros;

        const int64_t absBucket = BaseBucketOf(s.scheduledMicros);
        if (absBucket < mBaseBucket + kNumBuckets)
        {
            PushToWheel(ref);
        }
        else
        {
            HeapPush(ref);
        }
    }

    // --- Min-heap (keyed by Ref::scheduledMicros) ----------------------------
    void SimTimeScheduler::HeapPush(const Ref& ref)
    {
        DIA_ASSERT(!mHeap.IsFull(),
                   "SimTimeScheduler: overflow heap full (>%u far-future entries)", kHeapCapacity);
        mHeap.Add(ref);
        HeapSiftUp(mHeap.Size() - 1);
    }

    void SimTimeScheduler::HeapPopRoot()
    {
        const unsigned int last = mHeap.Size() - 1;
        mHeap[0] = mHeap[last];
        mHeap.RemoveAt(last);
        if (mHeap.Size() > 0)
        {
            HeapSiftDown(0);
        }
    }

    void SimTimeScheduler::HeapSiftUp(unsigned int index)
    {
        while (index > 0)
        {
            const unsigned int parent = (index - 1) / 2;
            if (mHeap[parent].scheduledMicros <= mHeap[index].scheduledMicros)
            {
                break;
            }
            const Ref tmp   = mHeap[parent];
            mHeap[parent]   = mHeap[index];
            mHeap[index]    = tmp;
            index = parent;
        }
    }

    void SimTimeScheduler::HeapSiftDown(unsigned int index)
    {
        const unsigned int size = mHeap.Size();
        for (;;)
        {
            const unsigned int left  = 2 * index + 1;
            const unsigned int right = 2 * index + 2;
            unsigned int smallest = index;
            if (left  < size && mHeap[left].scheduledMicros  < mHeap[smallest].scheduledMicros)  smallest = left;
            if (right < size && mHeap[right].scheduledMicros < mHeap[smallest].scheduledMicros) smallest = right;
            if (smallest == index)
            {
                break;
            }
            const Ref tmp     = mHeap[smallest];
            mHeap[smallest]   = mHeap[index];
            mHeap[index]      = tmp;
            index = smallest;
        }
    }

    void SimTimeScheduler::MigrateHeapToWheel()
    {
        while (mHeap.Size() > 0)
        {
            const Ref top = mHeap[0];
            const Slot& s = mSlots[top.slotIndex];
            const bool stale = !s.active || top.epoch != s.epoch;
            if (stale)
            {
                HeapPopRoot();
                continue;
            }
            if (BaseBucketOf(top.scheduledMicros) < mBaseBucket + kNumBuckets)
            {
                HeapPopRoot();
                PushToWheel(top);   // keep the same epoch -> ref stays valid
                continue;
            }
            break;  // heap is ordered; nothing else is within the horizon yet
        }
    }

    // --- Scheduling API ------------------------------------------------------
    ScheduleHandle SimTimeScheduler::CommonSchedule(int64_t fireMicros, bool recurring,
                                                    std::optional<Core::TimeRelative> interval,
                                                    Core::StringCRC eventType,
                                                    Core::StringCRC targetSystemId)
    {
        const uint32_t index = AllocSlot();
        Slot& s = mSlots[index];
        s.scheduledMicros        = fireMicros;
        s.recurring              = recurring;
        s.interval               = interval;
        s.payload.eventType      = eventType;
        s.payload.targetSystemId = targetSystemId;
        ++mPendingCount;

        PlaceRef(index);
        return Pack(index, s.generation);
    }

    ScheduleHandle SimTimeScheduler::ScheduleAt(Core::TimeAbsolute time,
                                                Core::StringCRC eventType,
                                                Core::StringCRC targetSystemId)
    {
        return CommonSchedule(time.AsLongLongInMicroseconds(), false, std::nullopt,
                              eventType, targetSystemId);
    }

    ScheduleHandle SimTimeScheduler::ScheduleAfter(Core::TimeRelative delay,
                                                   Core::StringCRC eventType,
                                                   Core::StringCRC targetSystemId)
    {
        const int64_t fireMicros = mCurrentMicros + delay.AsLongLongInMicroseconds();
        return CommonSchedule(fireMicros, false, std::nullopt, eventType, targetSystemId);
    }

    ScheduleHandle SimTimeScheduler::ScheduleRecurring(Core::TimeRelative interval,
                                                       Core::StringCRC eventType,
                                                       Core::StringCRC targetSystemId)
    {
        const int64_t fireMicros = mCurrentMicros + interval.AsLongLongInMicroseconds();
        return CommonSchedule(fireMicros, true, interval, eventType, targetSystemId);
    }

    void SimTimeScheduler::Cancel(ScheduleHandle handle)
    {
        Slot* s = ResolveSlot(handle);
        if (s == nullptr)
        {
            return;     // no-op on invalid / stale handle
        }
        FreeSlot(IndexOf(handle));  // marks stale; wheel/heap Refs self-heal on next visit
    }

    void SimTimeScheduler::Reschedule(ScheduleHandle handle, Core::TimeAbsolute newTime)
    {
        Slot* s = ResolveSlot(handle);
        if (s == nullptr)
        {
            return;     // no-op on invalid / stale handle
        }
        s->scheduledMicros = newTime.AsLongLongInMicroseconds();
        PlaceRef(IndexOf(handle));  // bumps epoch (old Ref goes stale), re-parks in wheel/heap
    }

    // --- Stream connect --------------------------------------------------------
    void SimTimeScheduler::Connect(Dia::ApplicationFlow::IStreamConnector& connector,
                                   unsigned int maxReaders)
    {
        mFireWriter.Connect(connector, maxReaders);
    }

    // --- Tick ----------------------------------------------------------------
    void SimTimeScheduler::TickInternal(Core::TimeAbsolute currentTime,
                                        DynamicArrayC<SimTimeSchedulerFire, kMaxEntries>& firedOut)
    {
        mCurrentMicros = currentTime.AsLongLongInMicroseconds();
        mBaseBucket    = BaseBucketOf(mCurrentMicros);

        // Slide the window forward: pull any now-in-horizon entries out of the heap.
        MigrateHeapToWheel();

        // Collect every due Ref out of the wheel, self-healing stale Refs as we go.
        DynamicArrayC<Ref, kMaxEntries> due;
        for (int b = 0; b < kNumBuckets; ++b)
        {
            DynamicArrayC<Ref, kBucketCapacity>& bucket = mWheel[b];
            unsigned int i = 0;
            while (i < bucket.Size())
            {
                const Ref r = bucket[i];
                const Slot& s = mSlots[r.slotIndex];
                const bool stale = !s.active || r.epoch != s.epoch;
                if (stale)
                {
                    bucket.RemoveAt(i);         // drop stale ref, do not advance
                    continue;
                }
                if (s.scheduledMicros <= mCurrentMicros)
                {
                    due.Add(r);
                    bucket.RemoveAt(i);         // due entry leaves the wheel
                    continue;
                }
                ++i;
            }
        }

        // Fire in fire-time order (tie-break by slotIndex for determinism).
        // Hand-rolled insertion sort over [0, Size()) — DynamicArrayC::Sort()
        // sorts the whole capacity including default-constructed tail, so it
        // cannot be used on a partially-filled scratch array.
        for (unsigned int i = 1; i < due.Size(); ++i)
        {
            const Ref key = due[i];
            int j = static_cast<int>(i) - 1;
            while (j >= 0 &&
                   (due[static_cast<unsigned int>(j)].scheduledMicros > key.scheduledMicros ||
                    (due[static_cast<unsigned int>(j)].scheduledMicros == key.scheduledMicros &&
                     due[static_cast<unsigned int>(j)].slotIndex > key.slotIndex)))
            {
                due[static_cast<unsigned int>(j + 1)] = due[static_cast<unsigned int>(j)];
                --j;
            }
            due[static_cast<unsigned int>(j + 1)] = key;
        }

        for (unsigned int k = 0; k < due.Size(); ++k)
        {
            const uint32_t slotIndex = due[k].slotIndex;
            Slot& s = mSlots[slotIndex];

            if (!firedOut.IsFull())
            {
                firedOut.Add(s.payload);
            }
            // Real fan-out delivery (Q2). Safe unconditionally: Send() no-ops
            // (kFailLoudRejected) when Connect() was never called.
            mFireWriter.Send(s.payload);

            if (s.recurring)
            {
                // Advance by exactly one interval from the previous scheduled
                // time (not from currentTime) to avoid drift. Fires at most once
                // per Tick; catches up one interval per subsequent Tick.
                s.scheduledMicros += s.interval->AsLongLongInMicroseconds();
                PlaceRef(slotIndex);
            }
            else
            {
                FreeSlot(slotIndex);
            }
        }
    }

    void SimTimeScheduler::Tick(Core::TimeAbsolute currentTime)
    {
        DynamicArrayC<SimTimeSchedulerFire, kMaxEntries> scratch;
        TickInternal(currentTime, scratch);
    }

    int SimTimeScheduler::GetQueueDepth() const
    {
        return mPendingCount;
    }

} // namespace Dia::SimTime
