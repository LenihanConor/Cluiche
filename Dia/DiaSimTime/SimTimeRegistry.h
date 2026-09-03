#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Time/TimeRelative.h>
#include <DiaSimTime/SimTimePolicy.h>
#include <DiaSimTime/SimTimeState.h>
#include <DiaSimTime/SimTimeSchedulerFire.h>
#include <DiaStreams/EventStreamReader.h>

namespace Dia { namespace ApplicationFlow { class IStreamConnector; } }

namespace Dia::SimTime {

    class ISimTimeBudgetedSystem;
    class SimTimeScheduler;

    // SimTimeRegistryEntryView
    // -------------------------------------------------------------------------
    // Read-only snapshot of one registered entry, handed out by GetEntryAt() so
    // Task 4.4's gate loop can walk the registry (systemId + system pointer +
    // policy) without exposing the private Entry struct or its mutable
    // state/throttle bookkeeping. Thin value view over data that already exists.
    struct SimTimeRegistryEntryView
    {
        Core::StringCRC         systemId;
        ISimTimeBudgetedSystem* system = nullptr;
        SimTimePolicy           policy;
    };

    // SimTimeRegistry
    // -------------------------------------------------------------------------
    // The per-system registry that DiaSimTimeModule (Task 4.4) will own and
    // delegate to. Each gameplay / AI / physics system that wants LOD throttling
    // and dormancy registers itself here with a SimTimePolicy; the registry
    // tracks its awake/asleep state, its LOD "due this tick" schedule, and its
    // wake conditions (wake-on-time via the SimTimeScheduler, wake-on-message via
    // an externally-driven NotifyMessage() hook).
    //
    // This class builds the *mechanism* only. It does NOT run the per-tick gate
    // loop (spec step 4: sleep check -> tier check -> budget check -> run) — that
    // loop is assembled by Task 4.4. The registry exposes the primitives that
    // loop calls into: DueThisTick() (pure query), MarkRan() (post-run update),
    // Sleep()/Wake()/GetState(), and ProcessWakeEvents() (drive once per tick).
    //
    // Dependencies are injected (a SimTimeScheduler& in the ctor, an
    // IStreamConnector& in Connect()), matching how SimTimeDomainRegistry binds
    // to an externally-owned SimTimeDomain& — nothing here is constructed as a
    // singleton or fetched from a service locator.
    //
    // PD-001: all identity is StringCRC. PD-004: public storage is DynamicArrayC;
    // std::optional / other STL never leaks into the public surface here.
    class SimTimeRegistry
    {
    public:
        // Fixed-capacity like the rest of the codebase (cf. AIBudgetScheduler's
        // kMaxSystems=16, SimTimeDomainRegistry's kMaxDomains=32). Register /
        // RegisterWakeOnMessage DIA_ASSERT on overflow.
        static constexpr int kMaxSystems           = 32;
        static constexpr int kMaxWakeSubscriptions = 64;

        // Event type the registry schedules for its own wake-on-time entries.
        // Not literal constexpr (StringCRC::Calc is a runtime table lookup) —
        // declared here, defined in the .cpp, same precedent as
        // SimTimeDomainRegistry::kWorldId.
        static const Core::StringCRC kWakeSentinelEventType;

        // Binds to the externally-owned scheduler (dependency injection). The
        // scheduler is used for wake-on-time (RegisterWakeOnTime -> ScheduleAt).
        explicit SimTimeRegistry(SimTimeScheduler& scheduler);

        // --- Registration -----------------------------------------------------
        // Register a budgeted system with its policy. State defaults to kAwake.
        // DIA_ASSERT on nullptr, on a duplicate systemId, or at capacity.
        void  Register(ISimTimeBudgetedSystem* system, const SimTimePolicy& policy);
        // Remove by pointer identity. No-op if not registered.
        void  Unregister(ISimTimeBudgetedSystem* system);

        // --- LOD throttle (tier) ---------------------------------------------
        // Pure query: has enough game time elapsed since this system's last run
        // for its tier (or policy.maxInterval, if non-zero) to make it due this
        // tick? Never mutates — call MarkRan() after actually running the system.
        // Unknown systemId -> false (safe no-op query). See the .cpp for the
        // tier -> interval mapping and the kDormant gate-sequence nuance.
        bool  DueThisTick(Core::StringCRC systemId, Core::TimeAbsolute currentGameTime) const;
        // Records that the system ran at currentGameTime (updates its throttle
        // baseline). Unknown systemId -> no-op.
        void  MarkRan(Core::StringCRC systemId, Core::TimeAbsolute currentGameTime);

        // --- Sleep / wake state ----------------------------------------------
        // No-op on an unregistered systemId (mirrors SimTimeScheduler::Cancel /
        // Reschedule tolerance — stale/missing references never crash the caller).
        void          Sleep(Core::StringCRC systemId);
        void          Wake(Core::StringCRC systemId);
        // Unknown systemId -> kAwake (the default state).
        SimTimeState  GetState(Core::StringCRC systemId) const;
        // Whether a system with this id is currently registered. Distinct from
        // GetState (which returns kAwake for BOTH a registered-awake system and an
        // unknown one). SimTimeSaveState::Deserialize uses this to tell a saved
        // systemId that is no longer registered (log + skip) from one that is.
        bool          IsRegistered(Core::StringCRC systemId) const;

        // --- Wake conditions --------------------------------------------------
        // Wake-on-time: schedule a wake for `at` on the injected scheduler, tagged
        // with kWakeSentinelEventType. ProcessWakeEvents() (below) notices the
        // fire and calls Wake(systemId).
        void  RegisterWakeOnTime(Core::StringCRC systemId, Core::TimeAbsolute at);
        // Wake-on-message: record that `systemId` wakes when NotifyMessage(eventType)
        // is called. DELIBERATELY no wiring from any concrete EventStreamStore /
        // message-bus to NotifyMessage() — per the spec's Non-Responsibilities the
        // calling side is external (e.g. a spatial system fires an event; this
        // registry only reacts). DIA_ASSERT at subscription capacity.
        void  RegisterWakeOnMessage(Core::StringCRC systemId, Core::StringCRC eventType);
        // Wake every system currently subscribed to `eventType`. Called by
        // whatever external system decides the message fired (out of scope here).
        void  NotifyMessage(Core::StringCRC eventType);

        // --- Wake-on-time stream wiring --------------------------------------
        // Wire this registry's reader to the SAME stream the SimTimeScheduler
        // publishes fire events to (StringCRC("SimTimeSchedulerFire")). Safe to
        // never call: ProcessWakeEvents() no-ops while disconnected.
        void  Connect(Dia::ApplicationFlow::IStreamConnector& connector,
                      unsigned int maxReaders = 16);
        // Drain the fire stream; for every event whose eventType matches
        // kWakeSentinelEventType, Wake(targetSystemId). Events with any other
        // eventType are ignored (the stream fans out EVERY scheduled event type,
        // per the scheduler's Q2 design, so filtering by our sentinel is required).
        // Meant to be called once per tick by whoever drives the registry (4.4).
        void  ProcessWakeEvents();

        // Number of currently registered systems.
        int   GetRegisteredCount() const;

        // --- Enumeration (Task 4.4 gate loop) --------------------------------
        // Read-only snapshot of the registered entry at `index` (0-based, in
        // registration order). DIA_ASSERT on out-of-range. Pairs with
        // GetRegisteredCount() to walk every registered system.
        SimTimeRegistryEntryView GetEntryAt(int index) const;

        // Number of registered systems currently in the kSleeping state
        // (feeds the simtime.registry.sleeping_count metric).
        int   GetSleepingCount() const;

        // --- Tier -> interval configuration (Task 4.4 OnConfigure) -----------
        // Override the throttle interval for the three Hz-based tiers from the
        // config JSON's tier_hz block. Each hz is converted to an interval of
        // 1000/hz ms. A non-positive hz leaves that tier's interval unchanged
        // (config-preserving default). kImmediate / kDormant are NOT Hz-based
        // and are unaffected.
        void  SetTierHz(float highHz, float mediumHz, float lowHz);

    private:
        struct Entry
        {
            Core::StringCRC         systemId;
            ISimTimeBudgetedSystem* system          = nullptr;
            SimTimePolicy           policy;
            SimTimeState            state           = SimTimeState::kAwake;
            // Throttle baseline. hasRun distinguishes "never ticked yet" (a fresh
            // system is due immediately) from "ran at Zero()".
            Core::TimeAbsolute      lastRunGameTime = Core::TimeAbsolute::Zero();
            bool                    hasRun          = false;
        };

        struct WakeSub
        {
            Core::StringCRC systemId;
            Core::StringCRC eventType;
        };

        const Entry* FindEntry(Core::StringCRC systemId) const;
        Entry*       FindEntry(Core::StringCRC systemId);

        // Tier -> throttle interval. kImmediate is always due (outAlways) and
        // kDormant is never due (outNever) — both hardcoded, not Hz-based. The
        // three Hz tiers (kHigh/kMedium/kLow) read their interval from the
        // configurable members below (defaults set in the ctor; overridable via
        // SetTierHz).
        void TierInterval(SimTimeTier tier,
                          bool& outAlways, bool& outNever, Core::TimeRelative& outInterval) const;

        SimTimeScheduler& mScheduler;   // externally owned (dependency injection)

        // Configurable Hz-tier intervals (defaults in the ctor: kHigh ~30Hz,
        // kMedium 10Hz, kLow 2Hz). SetTierHz overwrites these from config.
        Core::TimeRelative mHighInterval;
        Core::TimeRelative mMediumInterval;
        Core::TimeRelative mLowInterval;

        Dia::Core::Containers::DynamicArrayC<Entry, kMaxSystems>             mEntries;
        Dia::Core::Containers::DynamicArrayC<WakeSub, kMaxWakeSubscriptions> mWakeSubs;

        // Reader on the scheduler's fire stream. owner=nullptr: the reader only
        // uses owner to stamp a sender CRC on outgoing events (it sends none), so
        // a registry that isn't itself a Module is fine here. Same pattern as
        // SimTimeScheduler's own mFireWriter.
        Dia::ApplicationFlow::EventStreamReader<SimTimeSchedulerFire> mWakeReader{
            nullptr, Core::StringCRC("SimTimeSchedulerFire") };
    };

} // namespace Dia::SimTime
