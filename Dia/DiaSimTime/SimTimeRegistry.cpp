#include <DiaSimTime/SimTimeRegistry.h>

#include <DiaSimTime/ISimTimeBudgetedSystem.h>
#include <DiaSimTime/SimTimeScheduler.h>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Time/TimeRelative.h>

namespace Dia::SimTime {

    // "$simtime.wake" hashed once at static-init (StringCRC::Calc is a runtime
    // table lookup, not constexpr). The leading '$' keeps it out of any natural
    // gameplay event-type namespace.
    const Core::StringCRC SimTimeRegistry::kWakeSentinelEventType("$simtime.wake");

    SimTimeRegistry::SimTimeRegistry(SimTimeScheduler& scheduler)
        : mScheduler(scheduler)
        // Default Hz-tier intervals (overridable via SetTierHz from config JSON):
        , mHighInterval  (Core::TimeRelative::CreateFromMicroseconds(33333.0f)) // ~30 Hz
        , mMediumInterval(Core::TimeRelative::CreateFromMilliseconds(100))      // 10 Hz
        , mLowInterval   (Core::TimeRelative::CreateFromMilliseconds(500))      // 2 Hz
    {
    }

    // Tier -> minimum interval between runs. kImmediate is always due, kDormant
    // never due (both hardcoded); the three Hz tiers read the configurable
    // members (SetTierHz). Returns "always due" via outAlways and "never due" via
    // outNever; otherwise outInterval holds the tier interval.
    void SimTimeRegistry::TierInterval(SimTimeTier tier,
                                       bool& outAlways, bool& outNever, Core::TimeRelative& outInterval) const
    {
        outAlways = false;
        outNever  = false;
        switch (tier)
        {
        case SimTimeTier::kImmediate:
            outAlways = true;               // every tick
            break;
        case SimTimeTier::kHigh:
            outInterval = mHighInterval;
            break;
        case SimTimeTier::kMedium:
            outInterval = mMediumInterval;
            break;
        case SimTimeTier::kLow:
            outInterval = mLowInterval;
            break;
        case SimTimeTier::kDormant:
            outNever = true;                // never via throttle
            break;
        }
    }

    // --- Lookup --------------------------------------------------------------
    const SimTimeRegistry::Entry* SimTimeRegistry::FindEntry(Core::StringCRC systemId) const
    {
        for (unsigned int i = 0; i < mEntries.Size(); ++i)
        {
            const Entry& e = mEntries.At(i);
            if (e.systemId == systemId)
            {
                return &e;
            }
        }
        return nullptr;
    }

    SimTimeRegistry::Entry* SimTimeRegistry::FindEntry(Core::StringCRC systemId)
    {
        for (unsigned int i = 0; i < mEntries.Size(); ++i)
        {
            Entry& e = mEntries.At(i);
            if (e.systemId == systemId)
            {
                return &e;
            }
        }
        return nullptr;
    }

    // --- Registration --------------------------------------------------------
    void SimTimeRegistry::Register(ISimTimeBudgetedSystem* system, const SimTimePolicy& policy)
    {
        DIA_ASSERT(system != nullptr, "SimTimeRegistry::Register — null system");
        const Core::StringCRC systemId = system->GetSystemId();
        DIA_ASSERT(FindEntry(systemId) == nullptr, "SimTimeRegistry::Register — a system with this id is already registered");
        DIA_ASSERT(!mEntries.IsFull(), "SimTimeRegistry::Register — registry is at capacity (%d systems)", kMaxSystems);

        mEntries.AddDefault();
        Entry& entry     = mEntries.Back();
        entry.systemId   = systemId;
        entry.system     = system;
        entry.policy     = policy;
        entry.state      = SimTimeState::kAwake;
        entry.hasRun     = false;
    }

    void SimTimeRegistry::Unregister(ISimTimeBudgetedSystem* system)
    {
        if (system == nullptr)
        {
            return;
        }
        for (unsigned int i = 0; i < mEntries.Size(); ++i)
        {
            if (mEntries.At(i).system == system)
            {
                mEntries.RemoveAt(i);
                return;
            }
        }
    }

    // --- LOD throttle --------------------------------------------------------
    bool SimTimeRegistry::DueThisTick(Core::StringCRC systemId, Core::TimeAbsolute currentGameTime) const
    {
        const Entry* e = FindEntry(systemId);
        if (e == nullptr)
        {
            return false;   // unknown system: safe no-op query
        }

        // Gate-sequence nuance (documented, not silently resolved): SimTimeState
        // (sleep) and SimTimeTier (LOD) are two SEPARATE axes. The spec's gate
        // sequence checks sleep FIRST, then the tier throttle — so this query
        // deliberately does NOT consult state (that is the caller's step 4a).
        // A kDormant-tier system that is kAwake is still never due here: it is
        // "registered but never ticked" by design, and getting it to actually
        // run requires whoever wired it up to reconsider its tier at the call
        // site (out of scope for this task).
        bool always = false;
        bool never  = false;
        Core::TimeRelative interval = Core::TimeRelative::Zero();
        TierInterval(e->policy.tier, always, never, interval);

        if (never)   { return false; }   // kDormant — never via throttle
        if (always)  { return true;  }   // kImmediate — every tick

        // policy.maxInterval (when non-zero) overrides the tier default.
        if (e->policy.maxInterval != Core::TimeRelative::Zero())
        {
            interval = e->policy.maxInterval;
        }

        // A freshly-registered system that has never run is due immediately.
        if (!e->hasRun)
        {
            return true;
        }

        const Core::TimeRelative elapsed = currentGameTime - e->lastRunGameTime;
        return elapsed >= interval;
    }

    void SimTimeRegistry::MarkRan(Core::StringCRC systemId, Core::TimeAbsolute currentGameTime)
    {
        Entry* e = FindEntry(systemId);
        if (e == nullptr)
        {
            return;   // unknown system: no-op
        }
        e->lastRunGameTime = currentGameTime;
        e->hasRun          = true;
    }

    // --- Sleep / wake --------------------------------------------------------
    void SimTimeRegistry::Sleep(Core::StringCRC systemId)
    {
        Entry* e = FindEntry(systemId);
        if (e != nullptr)
        {
            e->state = SimTimeState::kSleeping;
        }
    }

    void SimTimeRegistry::Wake(Core::StringCRC systemId)
    {
        Entry* e = FindEntry(systemId);
        if (e != nullptr)
        {
            e->state = SimTimeState::kAwake;
        }
    }

    SimTimeState SimTimeRegistry::GetState(Core::StringCRC systemId) const
    {
        const Entry* e = FindEntry(systemId);
        return e ? e->state : SimTimeState::kAwake;
    }

    // --- Wake conditions -----------------------------------------------------
    void SimTimeRegistry::RegisterWakeOnTime(Core::StringCRC systemId, Core::TimeAbsolute at)
    {
        mScheduler.ScheduleAt(at, kWakeSentinelEventType, systemId);
    }

    void SimTimeRegistry::RegisterWakeOnMessage(Core::StringCRC systemId, Core::StringCRC eventType)
    {
        DIA_ASSERT(!mWakeSubs.IsFull(), "SimTimeRegistry::RegisterWakeOnMessage — at subscription capacity (%d)", kMaxWakeSubscriptions);
        mWakeSubs.AddDefault();
        WakeSub& sub  = mWakeSubs.Back();
        sub.systemId  = systemId;
        sub.eventType = eventType;
    }

    void SimTimeRegistry::NotifyMessage(Core::StringCRC eventType)
    {
        for (unsigned int i = 0; i < mWakeSubs.Size(); ++i)
        {
            const WakeSub& sub = mWakeSubs.At(i);
            if (sub.eventType == eventType)
            {
                Wake(sub.systemId);
            }
        }
    }

    // --- Wake-on-time stream wiring ------------------------------------------
    void SimTimeRegistry::Connect(Dia::ApplicationFlow::IStreamConnector& connector, unsigned int maxReaders)
    {
        mWakeReader.Connect(connector, maxReaders);
    }

    void SimTimeRegistry::ProcessWakeEvents()
    {
        // Drain in bounded batches so a burst larger than one batch still fully
        // clears across a single call.
        while (mWakeReader.HasPending())
        {
            Dia::Core::Containers::DynamicArrayC<
                Dia::ApplicationFlow::Event<SimTimeSchedulerFire>, kMaxSystems> batch;
            mWakeReader.Consume(batch);
            if (batch.Size() == 0)
            {
                break;   // safety: nothing drained despite HasPending()
            }
            for (unsigned int i = 0; i < batch.Size(); ++i)
            {
                const SimTimeSchedulerFire& fire = batch.At(i).payload;
                // The stream fans out EVERY scheduled event type; only our own
                // wake sentinel should wake a system.
                if (fire.eventType == kWakeSentinelEventType)
                {
                    Wake(fire.targetSystemId);
                }
            }
        }
    }

    int SimTimeRegistry::GetRegisteredCount() const
    {
        return static_cast<int>(mEntries.Size());
    }

    SimTimeRegistryEntryView SimTimeRegistry::GetEntryAt(int index) const
    {
        DIA_ASSERT(index >= 0 && index < static_cast<int>(mEntries.Size()),
                   "SimTimeRegistry::GetEntryAt — index %d out of range (count %u)",
                   index, mEntries.Size());
        const Entry& e = mEntries.At(static_cast<unsigned int>(index));
        SimTimeRegistryEntryView view;
        view.systemId = e.systemId;
        view.system   = e.system;
        view.policy   = e.policy;
        return view;
    }

    int SimTimeRegistry::GetSleepingCount() const
    {
        int count = 0;
        for (unsigned int i = 0; i < mEntries.Size(); ++i)
        {
            if (mEntries.At(i).state == SimTimeState::kSleeping)
            {
                ++count;
            }
        }
        return count;
    }

    void SimTimeRegistry::SetTierHz(float highHz, float mediumHz, float lowHz)
    {
        // A non-positive Hz leaves that tier's interval unchanged (keeps the
        // ctor default / previously-configured value) — never divide by <= 0.
        if (highHz   > 0.0f) { mHighInterval   = Core::TimeRelative::CreateFromMilliseconds(static_cast<int>(1000.0f / highHz));   }
        if (mediumHz > 0.0f) { mMediumInterval = Core::TimeRelative::CreateFromMilliseconds(static_cast<int>(1000.0f / mediumHz)); }
        if (lowHz    > 0.0f) { mLowInterval    = Core::TimeRelative::CreateFromMilliseconds(static_cast<int>(1000.0f / lowHz));    }
    }

} // namespace Dia::SimTime
