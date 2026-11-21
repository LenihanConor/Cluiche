// World Step chain: stage table, declared-order resolution, readiness gate and the
// Capture -> Run -> Commit state machine. Pure logic: no modules, bus, scheduler, cadence or saving.
// Spec: docs/specs/applications/cow/systems/worldstep/step-chain.md
//
// Threading: the chain is NOT thread-safe. RegisterStage / UnregisterStage / Configure and every
// sequencing call are SimPU-only, except Run, which the module calls from the worker thread while
// SimPU makes no chain call (the module guarantees this). Run reads the enabled table and nothing else;
// that table is rebuilt only at Idle, so unregistering mid-step goes to a separate pending-removal list.
#pragma once

#include <Shared/WorldStep/IWorldStepStage.h>
#include <Shared/WorldStep/WorldStepConfig.h>

#include <DiaCore/CRC/StringCRC.h>

#include <cstdint>

namespace CoW::WorldStep
{
    enum class ChainPhase : uint8_t { Idle, Captured, Ran };

    // Which stage method an observer callback brackets.
    enum class StagePhase : uint8_t { Capture, Run, Commit };

    // Optional hook (e.g. per-stage profiling scopes) so the chain needs no DiaObservation dependency.
    // `index` is the stage's position in declared (enabled) order.
    class IChainObserver
    {
    public:
        virtual ~IChainObserver() = default;
        virtual void OnStageBegin(uint32_t index, StagePhase phase) = 0;
        virtual void OnStageEnd(uint32_t index, StagePhase phase) = 0;
    };

    class WorldStepChain
    {
    public:
        // false if a step is in flight (nothing changes).
        bool Configure(const WorldStepConfig& config);
        // false: duplicate id, null stage, table full (kMaxStages registrations), removal pending for id, or re-entrant call.
        bool RegisterStage(Dia::Core::StringCRC id, IWorldStepStage& stage);
        // At Idle removes at once; mid-step the removal is queued until the step ends. Unknown id: no-op.
        void UnregisterStage(Dia::Core::StringCRC id);

        // No configured stage missing and every enabled stage ready. False if never configured.
        bool AllReady() const;
        bool MissingStages(StageIdList& out) const;   // configured but unregistered, declared order; true if any
        bool UnknownStages(StageIdList& out) const;   // registered but not in order; true if any

        bool Capture(StepKey key);                                // false if not Idle (or not AllReady / re-entrant)
        bool Run(Dia::DeferredStep::IParallelExecutor& executor); // false if not Captured
        bool Commit();                                            // false if not Ran
        void Abandon();                                           // Idle without Commit; applies pending removals
        bool RestoreRan(StepKey key);                             // load only: Idle -> Ran without calling any stage

        ChainPhase Phase() const { return mPhase; }
        StepKey CapturedKey() const { return mCapturedKey; }
        uint32_t EnabledCount() const { return mEnabledCount; }
        IWorldStepStage* StageAt(uint32_t index) const;           // declared order, enabled only; null if out of range
        Dia::Core::StringCRC StageIdAt(uint32_t index) const;

        void SetObserver(IChainObserver* observer) { mObserver = observer; }

        // Diagnostics for tests (AC 20, AC 25): the structures Run walks and the removal queue.
        const void* EnabledTableData() const { return mEnabled; }
        static constexpr uint32_t EnabledTableCapacity() { return kMaxStages; }
        static constexpr uint32_t RegisteredTableCapacity() { return kMaxStages; }
        uint32_t RegisteredCount() const { return mRegisteredCount; }
        uint32_t PendingRemovalCount() const { return mPendingCount; }

    private:
        struct Entry
        {
            Dia::Core::StringCRC id;
            IWorldStepStage*     stage = nullptr;
        };

        int  FindRegistered(Dia::Core::StringCRC id) const;
        bool IsPendingRemoval(Dia::Core::StringCRC id) const;
        void RebuildEnabled();
        void ApplyPendingRemovals();
        bool BeginCall();   // re-entrancy guard; false (and debug assert) when called from inside a stage method

        template <typename Fn> void VisitStages(StagePhase phase, Fn&& fn);

        Entry                mRegistered[kMaxStages];
        uint32_t             mRegisteredCount = 0;
        Entry                mEnabled[kMaxStages];       // declared order; what Capture/Run/Commit walk
        uint32_t             mEnabledCount = 0;
        Dia::Core::StringCRC mPending[kMaxStages];       // pending removals; Run never reads this
        uint32_t             mPendingCount = 0;

        WorldStepConfig      mConfig;
        bool                 mConfigured = false;
        ChainPhase           mPhase = ChainPhase::Idle;
        StepKey              mCapturedKey = 0;
        IChainObserver*      mObserver = nullptr;
        bool                 mInStage = false;
    };
}
