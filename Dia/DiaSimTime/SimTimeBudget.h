#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaSimTime/SimTimePriority.h>
#include <DiaCore/SimTime/SimTimeContext.h>

namespace Dia { namespace Observation { namespace Metric { class Counter; class Gauge; } } }

namespace Dia::SimTime {

    class ISimTimeBudgetedSystem;
    class IOneShotWork;

    // SimTimeBudget
    // -------------------------------------------------------------------------
    // The per-tick CPU-time budget allocator for DiaSimTime. Given a fixed
    // per-tick millisecond budget split across the four SimTimePriority tiers
    // (kCritical/kHigh/kNormal/kBackground), it decides which registered systems
    // actually run this tick, tracks systems that get starved, and force-promotes
    // them once they have gone too long without running (ST-008: bounded
    // worst-case latency via per-tier deadlines / carry-forward promotion).
    //
    // -- Design note: why this does NOT literally compose AIBudgetScheduler ----
    // Pattern E says SimTimeBudget "extends" Dia::AIBudget::AIBudgetScheduler.
    // Pattern C's gate loop (Task 4.4) calls mBudget.RunIfCapacity(entry) ONCE
    // PER registered system, interleaved with sleep/LOD checks that live in a
    // *different* class (SimTimeRegistry). AIBudgetScheduler::Update(totalBudgetMs)
    // is a BATCH API: it owns its own registration list and, in one call, iterates
    // every system and distributes budget internally. That batch-all-at-once shape
    // cannot expose a clean per-entry RunIfCapacity(one system) call, so wrapping
    // four AIBudgetScheduler instances (one per tier) does not compose with the
    // per-entry loop this task must serve.
    //
    // Resolution (same spirit as Task 4.1's inheritance-vs-alias deviation):
    // SimTimeBudget is its own class with its own per-tier remaining-ms pools and
    // its own minimal per-system bookkeeping (same fixed-capacity DynamicArrayC
    // entry-table shape as SimTimeRegistry::Entry). It "extends" AIBudgetScheduler
    // only in the sense the plan means — same wall-clock timing technique
    // (std::chrono::steady_clock, measure real elapsed time of UpdateBudgeted),
    // same AB-005 zero-budget-is-a-no-op contract inherited through
    // ISimTimeBudgetedSystem::UpdateBudgeted, and the same fixed registration-cap
    // philosophy (kMaxSystems). Registration-order fairness (AB-003) is replaced by
    // priority tiers; deadline promotion (ST-008) is the new fairness mechanism.
    // It does NOT derive from, instantiate, or depend on AIBudgetScheduler, and it
    // does NOT depend on SimTimeRegistry (a sibling, not a dependency).
    //
    // -- One-shot completion path (ST-012) ------------------------------------
    // A separate, lightweight queue (SubmitOneShot / RunOneShots) exists for
    // transient async work (IOneShotWork). It shares NO state with the four
    // priority-tier pools, is not tier-based, and is sized generously
    // (kMaxOneShots) so it scales with population instead of contending for the
    // kMaxSystems steady-state slots.
    //
    // PD-001: all identity / metric keys are StringCRC. PD-004: public storage is
    // DynamicArrayC; std::chrono is used only inside the .cpp (matching
    // AIBudgetScheduler's own precedent), never in this public header.
    class SimTimeBudget
    {
    public:
        // Steady-state priority-tier registration cap. Same philosophy and value
        // as AIBudgetScheduler::kMaxSystems (16) — long-lived systems only.
        static constexpr int kMaxSystems = 16;

        // One-shot queue capacity. DELIBERATELY independent of (and much larger
        // than) kMaxSystems: ST-012 requires transient work to scale with
        // population without contending for steady-state slots.
        static constexpr int kMaxOneShots = 256;

        // Number of priority tiers (SimTimePriority enumerators).
        static constexpr int kTierCount = 4;

        // Spec example default per-tier per-tick budgets (ms). Task 4.4's
        // OnConfigure overrides these from parsed JSON via SetTierBudgets.
        static constexpr float kDefaultCriticalMs   = 2.0f;
        static constexpr float kDefaultHighMs       = 1.0f;
        static constexpr float kDefaultNormalMs     = 0.5f;
        static constexpr float kDefaultBackgroundMs = 0.25f;

        // Default per-tier promotion deadlines (ms of wall-clock staleness before
        // a starved system is force-run). Chosen proportional to tier importance:
        // kCritical gets the tightest worst-case latency bound, kBackground the
        // loosest. Overridable via SetTierDeadlines (Task 4.4 config hook).
        static constexpr float kDefaultCriticalDeadlineMs   = 4.0f;
        static constexpr float kDefaultHighDeadlineMs       = 16.0f;
        static constexpr float kDefaultNormalDeadlineMs     = 100.0f;
        static constexpr float kDefaultBackgroundDeadlineMs = 1000.0f;

        SimTimeBudget();

        // --- Registration -----------------------------------------------------
        // Register a budgeted system, routed by system->GetPriority(). A freshly
        // registered system is immediately eligible to run when its tier has
        // budget; its deadline-staleness clock starts at registration time.
        // DIA_ASSERT on nullptr, duplicate pointer, or at capacity.
        void Register(ISimTimeBudgetedSystem* system);
        // Remove by pointer identity. No-op if not registered.
        void Unregister(ISimTimeBudgetedSystem* system);

        // --- Configuration ----------------------------------------------------
        // Set per-tier per-tick budgets (ms). Task 4.4's OnConfigure calls this
        // from parsed JSON. Negative values are clamped to 0 (a zero-budget tier
        // is valid — its systems only ever run via deadline promotion).
        void SetTierBudgets(float criticalMs, float highMs, float normalMs, float backgroundMs);
        // Set per-tier promotion deadlines (ms of wall-clock staleness).
        void SetTierDeadlines(float criticalMs, float highMs, float normalMs, float backgroundMs);

        // --- Per-tick allocation ----------------------------------------------
        // Resets every tier's remaining-ms pool to its configured budget and the
        // per-tick telemetry (used_ms, this-tick deferred/promoted counts,
        // stale_ms_max). Call once at the top of each tick, before the per-entry
        // RunIfCapacity loop. The SimTimeContext is accepted to match Pattern C's
        // signature; budget bookkeeping is wall-clock based (ST-010), so no field
        // of ctx is actually consumed here.
        void AllocateTick(const SimTimeContext& ctx);

        // --- Per-entry budget gate --------------------------------------------
        // The core per-system decision (Task 4.4's gate loop calls this once per
        // registered, awake, due system):
        //   * If the system's tier still has remaining budget this tick: run it
        //     (UpdateBudgeted with the tier's remaining ms), measure real elapsed
        //     wall-clock time, deduct it from the tier pool (floored at 0),
        //     refresh the system's staleness clock, add to used_ms, return true.
        //   * Else if the system's wall-clock staleness exceeds its tier deadline:
        //     PROMOTE it — run it anyway with a forced allocation (its tier's
        //     configured per-tick ms), refresh staleness, return true. A promoted
        //     system did run (just late), so it does NOT count as deferred.
        //   * Else: do not run — increment deferred_count, update stale_ms_max if
        //     this system's staleness is the largest deferred this tick, return
        //     false.
        // Unknown / unregistered pointer: safe no-op, returns false.
        bool RunIfCapacity(ISimTimeBudgetedSystem* system);

        // --- One-shot completion path (ST-012) --------------------------------
        // Submit a transient one-shot work item. Fixed-capacity queue independent
        // of kMaxSystems. DIA_ASSERT on nullptr or overflow.
        void SubmitOneShot(IOneShotWork* work);
        // Drain the one-shot queue in submission order: call Step(remaining) on
        // each pending item, subtracting real elapsed wall-clock time from the
        // slice, until the queue is empty or budgetMs is exhausted. Items whose
        // Step() returned true are removed; the rest stay queued for a later call.
        // Does NOT touch the four priority-tier pools. Takes its own explicit slice.
        void RunOneShots(float budgetMs);

        // --- Introspection / test visibility ----------------------------------
        int   GetRegisteredCount() const;
        int   GetPendingOneShotCount() const;

        // Total wall-clock ms consumed by registered systems in the last tick
        // (mirrors the simtime.budget.used_ms gauge).
        float GetLastUsedMs() const;
        // Systems deferred (not run, not promoted) in the last tick.
        int   GetLastDeferredCount() const;
        // Systems promoted (force-run at deadline) in the last tick (diagnostic).
        int   GetLastPromotedCount() const;
        // Oldest deferred work this tick, ms since last update (mirrors the
        // simtime.budget.stale_ms_max gauge). 0 if nothing was deferred.
        float GetLastStaleMsMax() const;
        // Lifetime total of deferred events (mirrors the monotonic
        // simtime.budget.deferred_count counter).
        unsigned long long GetDeferredCountTotal() const;

        // Remaining budget in a tier's pool this tick (test visibility).
        float GetTierRemainingMs(SimTimePriority tier) const;

    private:
        struct Entry
        {
            Core::StringCRC         systemId;
            ISimTimeBudgetedSystem* system            = nullptr;
            SimTimePriority         priority          = SimTimePriority::kNormal;
            // Wall-clock steady-clock ms at which this system last ran (or was
            // registered, if it has never run). Staleness = NowMs() - lastRanMs.
            double                  lastRanSteadyMs   = 0.0;
        };

        static int    TierIndex(SimTimePriority tier);
        static double NowMs();

        const Entry* FindEntry(const ISimTimeBudgetedSystem* system) const;
        Entry*       FindEntry(const ISimTimeBudgetedSystem* system);

        // Run a system now, measure elapsed, refresh its staleness clock, and add
        // to the running used_ms total. Returns elapsed ms.
        float RunSystem(Entry& entry, float allowanceMs);

        Dia::Core::Containers::DynamicArrayC<Entry, kMaxSystems>          mEntries;
        Dia::Core::Containers::DynamicArrayC<IOneShotWork*, kMaxOneShots> mOneShots;

        float mTierBudgetMs  [kTierCount];   // configured per-tick budget per tier
        float mTierDeadlineMs[kTierCount];   // promotion deadline (staleness) per tier
        float mTierRemainingMs[kTierCount];  // remaining pool this tick

        // Per-tick telemetry (reset in AllocateTick).
        float mLastUsedMs        = 0.0f;
        int   mLastDeferredCount = 0;
        int   mLastPromotedCount = 0;
        float mLastStaleMsMax    = 0.0f;

        // Metrics (registered once in the ctor; shared across instances via the
        // global MetricRegistry, so pointers are stable and may be null-guarded).
        Dia::Observation::Metric::Gauge*   mMetricUsedMs        = nullptr;
        Dia::Observation::Metric::Counter* mMetricDeferredCount = nullptr;
        Dia::Observation::Metric::Gauge*   mMetricStaleMsMax    = nullptr;
    };

} // namespace Dia::SimTime
