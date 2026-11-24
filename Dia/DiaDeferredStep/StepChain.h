// Step chain: stage table, declared-order resolution, readiness gate and the
// Capture -> Run -> Commit state machine. Pure logic: no modules, bus, scheduler, cadence or saving.
// Spec: docs/specs/applications/dia/systems/diadeferredstep/step-chain.md
//
// Threading: the chain is NOT thread-safe. RegisterStage / UnregisterStage / Configure and every
// sequencing call are SimPU-only, except Run, which the owner calls from the worker thread while
// SimPU makes no chain call (the owner guarantees this). Run reads the enabled table and nothing else;
// that table is rebuilt only at Idle, so unregistering mid-step goes to a separate pending-removal list.
//
// Capacity is a template parameter (default 8); all storage is inline, so no allocation on the sim thread.
#pragma once

#include <DiaDeferredStep/IStepStage.h>
#include <DiaDeferredStep/StepChainConfig.h>

#include <DiaCore/Core/Assert.h>
#include <DiaCore/CRC/StringCRC.h>

#include <cstdint>

namespace Dia::DeferredStep
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

    template <uint32_t Capacity = kDefaultStageCapacity>
    class StepChain
    {
    public:
        using Config = StepChainConfig<Capacity>;
        using IdList = StageIdList<Capacity>;

        // false if a step is in flight (nothing changes).
        bool Configure(const Config& config);
        // false: duplicate id, null stage, table full (Capacity registrations), removal pending for id, or re-entrant call.
        bool RegisterStage(Dia::Core::StringCRC id, IStepStage& stage);
        // At Idle removes at once; mid-step the removal is queued until the step ends. Unknown id: no-op.
        void UnregisterStage(Dia::Core::StringCRC id);

        // No configured stage missing and every enabled stage ready. False if never configured.
        bool AllReady() const;
        bool MissingStages(IdList& out) const;   // configured but unregistered, declared order; true if any
        bool UnknownStages(IdList& out) const;   // registered but not in order; true if any

        bool Capture(StepKey key);                  // false if not Idle (or not AllReady / re-entrant)
        bool Run(Dia::ParallelExecutor::IParallelExecutor& executor);      // false if not Captured
        bool Commit();                              // false if not Ran
        void Abandon();                             // Idle without Commit; applies pending removals
        bool RestoreRan(StepKey key);               // load only: Idle -> Ran without calling any stage

        ChainPhase Phase() const { return mPhase; }
        StepKey CapturedKey() const { return mCapturedKey; }
        uint32_t EnabledCount() const { return mEnabledCount; }
        IStepStage* StageAt(uint32_t index) const;  // declared order, enabled only; null if out of range
        Dia::Core::StringCRC StageIdAt(uint32_t index) const;

        void SetObserver(IChainObserver* observer) { mObserver = observer; }

        // Diagnostics for tests: the structures Run walks and the removal queue.
        const void* EnabledTableData() const { return mEnabled; }
        static constexpr uint32_t EnabledTableCapacity() { return Capacity; }
        static constexpr uint32_t RegisteredTableCapacity() { return Capacity; }
        uint32_t RegisteredCount() const { return mRegisteredCount; }
        uint32_t PendingRemovalCount() const { return mPendingCount; }

    private:
        struct Entry
        {
            Dia::Core::StringCRC id;
            IStepStage*          stage = nullptr;
        };

        int  FindRegistered(Dia::Core::StringCRC id) const;
        bool IsPendingRemoval(Dia::Core::StringCRC id) const;
        void RebuildEnabled();
        void ApplyPendingRemovals();
        bool BeginCall();   // re-entrancy guard; false (and debug assert) when called from inside a stage method

        template <typename Fn> void VisitStages(StagePhase phase, Fn&& fn);

        Entry                mRegistered[Capacity];
        uint32_t             mRegisteredCount = 0;
        Entry                mEnabled[Capacity];       // declared order; what Capture/Run/Commit walk
        uint32_t             mEnabledCount = 0;
        Dia::Core::StringCRC mPending[Capacity];       // pending removals; Run never reads this
        uint32_t             mPendingCount = 0;

        Config               mConfig;
        bool                 mConfigured = false;
        ChainPhase           mPhase = ChainPhase::Idle;
        StepKey              mCapturedKey = 0;
        IChainObserver*      mObserver = nullptr;
        bool                 mInStage = false;
    };

    template <uint32_t Capacity>
    bool StepChain<Capacity>::BeginCall()
    {
        if (mInStage)
        {
            DIA_ASSERT(false, "StepChain method called from inside a stage method");
            return false;
        }
        return true;
    }

    template <uint32_t Capacity>
    int StepChain<Capacity>::FindRegistered(Dia::Core::StringCRC id) const
    {
        for (uint32_t i = 0; i < mRegisteredCount; ++i)
        {
            if (mRegistered[i].id == id) { return static_cast<int>(i); }
        }
        return -1;
    }

    template <uint32_t Capacity>
    bool StepChain<Capacity>::IsPendingRemoval(Dia::Core::StringCRC id) const
    {
        for (uint32_t i = 0; i < mPendingCount; ++i)
        {
            if (mPending[i] == id) { return true; }
        }
        return false;
    }

    // Rebuilds the enabled table in declared order. Only called at Idle (or at the end of a step).
    template <uint32_t Capacity>
    void StepChain<Capacity>::RebuildEnabled()
    {
        mEnabledCount = 0;
        if (!mConfigured) { return; }
        for (uint32_t i = 0; i < mConfig.order.Size(); ++i)
        {
            const int at = FindRegistered(mConfig.order[i].crc);
            if (at >= 0 && mEnabledCount < Capacity)
            {
                mEnabled[mEnabledCount++] = mRegistered[at];
            }
        }
    }

    template <uint32_t Capacity>
    void StepChain<Capacity>::ApplyPendingRemovals()
    {
        for (uint32_t p = 0; p < mPendingCount; ++p)
        {
            const int at = FindRegistered(mPending[p]);
            if (at < 0) { continue; }
            for (uint32_t i = static_cast<uint32_t>(at); i + 1 < mRegisteredCount; ++i)
            {
                mRegistered[i] = mRegistered[i + 1];
            }
            --mRegisteredCount;
            mRegistered[mRegisteredCount] = Entry();
        }
        mPendingCount = 0;
        // Always rebuild: a RegisterStage made mid-step also waits for the end of the step.
        RebuildEnabled();
    }

    template <uint32_t Capacity>
    bool StepChain<Capacity>::Configure(const Config& config)
    {
        if (!BeginCall()) { return false; }
        if (mPhase != ChainPhase::Idle) { return false; }
        mConfig = config;
        mConfigured = true;
        RebuildEnabled();
        return true;
    }

    template <uint32_t Capacity>
    bool StepChain<Capacity>::RegisterStage(Dia::Core::StringCRC id, IStepStage& stage)
    {
        if (!BeginCall()) { return false; }
        if (FindRegistered(id) >= 0) { return false; }          // duplicate (includes removal pending)
        if (mRegisteredCount >= Capacity) { return false; }     // table full
        mRegistered[mRegisteredCount].id = id;
        mRegistered[mRegisteredCount].stage = &stage;
        ++mRegisteredCount;
        // The table Run walks must not change mid-step; the new stage is picked up when the step ends.
        if (mPhase == ChainPhase::Idle) { RebuildEnabled(); }
        return true;
    }

    template <uint32_t Capacity>
    void StepChain<Capacity>::UnregisterStage(Dia::Core::StringCRC id)
    {
        if (!BeginCall()) { return; }
        const int at = FindRegistered(id);
        if (at < 0) { return; }

        if (mPhase != ChainPhase::Idle)
        {
            if (!IsPendingRemoval(id) && mPendingCount < Capacity)
            {
                mPending[mPendingCount++] = id;
            }
            return;
        }

        for (uint32_t i = static_cast<uint32_t>(at); i + 1 < mRegisteredCount; ++i)
        {
            mRegistered[i] = mRegistered[i + 1];
        }
        --mRegisteredCount;
        mRegistered[mRegisteredCount] = Entry();
        RebuildEnabled();
    }

    template <uint32_t Capacity>
    bool StepChain<Capacity>::MissingStages(IdList& out) const
    {
        out.RemoveAll();
        if (!mConfigured) { return false; }
        for (uint32_t i = 0; i < mConfig.order.Size(); ++i)
        {
            if (FindRegistered(mConfig.order[i].crc) < 0) { out.Add(mConfig.order[i]); }
        }
        return out.Size() > 0;
    }

    template <uint32_t Capacity>
    bool StepChain<Capacity>::UnknownStages(IdList& out) const
    {
        out.RemoveAll();
        for (uint32_t r = 0; r < mRegisteredCount; ++r)
        {
            bool inOrder = false;
            if (mConfigured)
            {
                for (uint32_t i = 0; i < mConfig.order.Size(); ++i)
                {
                    if (mConfig.order[i].crc == mRegistered[r].id) { inOrder = true; break; }
                }
            }
            if (!inOrder)
            {
                StageId entry;
                entry.crc = mRegistered[r].id;
                entry.name = Dia::Core::Containers::String32(static_cast<const char*>(mRegistered[r].id));
                out.Add(entry);
            }
        }
        return out.Size() > 0;
    }

    template <uint32_t Capacity>
    bool StepChain<Capacity>::AllReady() const
    {
        if (!mConfigured) { return false; }
        for (uint32_t i = 0; i < mConfig.order.Size(); ++i)
        {
            if (FindRegistered(mConfig.order[i].crc) < 0) { return false; }   // missing
        }
        for (uint32_t i = 0; i < mEnabledCount; ++i)
        {
            if (!mEnabled[i].stage->IsReady()) { return false; }
        }
        return true;
    }

    template <uint32_t Capacity>
    template <typename Fn>
    void StepChain<Capacity>::VisitStages(StagePhase phase, Fn&& fn)
    {
        for (uint32_t i = 0; i < mEnabledCount; ++i)
        {
            if (mObserver) { mObserver->OnStageBegin(i, phase); }
            mInStage = true;
            fn(*mEnabled[i].stage);
            mInStage = false;
            if (mObserver) { mObserver->OnStageEnd(i, phase); }
        }
    }

    template <uint32_t Capacity>
    bool StepChain<Capacity>::Capture(StepKey key)
    {
        if (!BeginCall()) { return false; }
        if (mPhase != ChainPhase::Idle) { return false; }
        if (!AllReady())
        {
            DIA_ASSERT(false, "StepChain::Capture called while AllReady() is false");
            return false;
        }
        mCapturedKey = key;
        VisitStages(StagePhase::Capture, [key](IStepStage& s) { s.Capture(key); });
        mPhase = ChainPhase::Captured;
        return true;
    }

    template <uint32_t Capacity>
    bool StepChain<Capacity>::Run(Dia::ParallelExecutor::IParallelExecutor& executor)
    {
        if (!BeginCall()) { return false; }
        if (mPhase != ChainPhase::Captured) { return false; }
        VisitStages(StagePhase::Run, [&executor](IStepStage& s) { s.Run(executor); });
        mPhase = ChainPhase::Ran;
        return true;
    }

    template <uint32_t Capacity>
    bool StepChain<Capacity>::Commit()
    {
        if (!BeginCall()) { return false; }
        if (mPhase != ChainPhase::Ran) { return false; }
        VisitStages(StagePhase::Commit, [](IStepStage& s) { s.Commit(); });
        mPhase = ChainPhase::Idle;
        ApplyPendingRemovals();
        return true;
    }

    template <uint32_t Capacity>
    void StepChain<Capacity>::Abandon()
    {
        if (!BeginCall()) { return; }
        mPhase = ChainPhase::Idle;
        mCapturedKey = 0;
        ApplyPendingRemovals();
    }

    template <uint32_t Capacity>
    bool StepChain<Capacity>::RestoreRan(StepKey key)
    {
        if (!BeginCall()) { return false; }
        if (mPhase != ChainPhase::Idle) { return false; }
        if (!AllReady()) { return false; }
        mCapturedKey = key;
        mPhase = ChainPhase::Ran;
        return true;
    }

    template <uint32_t Capacity>
    IStepStage* StepChain<Capacity>::StageAt(uint32_t index) const
    {
        return index < mEnabledCount ? mEnabled[index].stage : nullptr;
    }

    template <uint32_t Capacity>
    Dia::Core::StringCRC StepChain<Capacity>::StageIdAt(uint32_t index) const
    {
        return index < mEnabledCount ? mEnabled[index].id : Dia::Core::StringCRC();
    }
}
