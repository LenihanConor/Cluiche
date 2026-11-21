#include <Shared/WorldStep/WorldStepChain.h>

#include <DiaCore/Core/Assert.h>

namespace CoW::WorldStep
{
    using Dia::Core::StringCRC;

    bool WorldStepChain::BeginCall()
    {
        if (mInStage)
        {
            DIA_ASSERT(false, "WorldStepChain method called from inside a stage method");
            return false;
        }
        return true;
    }

    int WorldStepChain::FindRegistered(StringCRC id) const
    {
        for (uint32_t i = 0; i < mRegisteredCount; ++i)
        {
            if (mRegistered[i].id == id) { return static_cast<int>(i); }
        }
        return -1;
    }

    bool WorldStepChain::IsPendingRemoval(StringCRC id) const
    {
        for (uint32_t i = 0; i < mPendingCount; ++i)
        {
            if (mPending[i] == id) { return true; }
        }
        return false;
    }

    // Rebuilds the enabled table in declared order. Only called at Idle (or at the end of a step).
    void WorldStepChain::RebuildEnabled()
    {
        mEnabledCount = 0;
        if (!mConfigured) { return; }
        for (uint32_t i = 0; i < mConfig.order.Size(); ++i)
        {
            const int at = FindRegistered(mConfig.order[i].crc);
            if (at >= 0 && mEnabledCount < kMaxStages)
            {
                mEnabled[mEnabledCount++] = mRegistered[at];
            }
        }
    }

    void WorldStepChain::ApplyPendingRemovals()
    {
        const bool any = mPendingCount > 0;
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
        (void)any;
        // Always rebuild: a RegisterStage made mid-step also waits for the end of the step.
        RebuildEnabled();
    }

    bool WorldStepChain::Configure(const WorldStepConfig& config)
    {
        if (!BeginCall()) { return false; }
        if (mPhase != ChainPhase::Idle) { return false; }
        mConfig = config;
        mConfigured = true;
        RebuildEnabled();
        return true;
    }

    bool WorldStepChain::RegisterStage(StringCRC id, IWorldStepStage& stage)
    {
        if (!BeginCall()) { return false; }
        if (FindRegistered(id) >= 0) { return false; }          // duplicate (includes removal pending)
        if (mRegisteredCount >= kMaxStages) { return false; }   // table full
        mRegistered[mRegisteredCount].id = id;
        mRegistered[mRegisteredCount].stage = &stage;
        ++mRegisteredCount;
        // The table Run walks must not change mid-step; the new stage is picked up when the step ends.
        if (mPhase == ChainPhase::Idle) { RebuildEnabled(); }
        return true;
    }

    void WorldStepChain::UnregisterStage(StringCRC id)
    {
        if (!BeginCall()) { return; }
        const int at = FindRegistered(id);
        if (at < 0) { return; }

        if (mPhase != ChainPhase::Idle)
        {
            if (!IsPendingRemoval(id) && mPendingCount < kMaxStages)
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

    bool WorldStepChain::MissingStages(StageIdList& out) const
    {
        out.RemoveAll();
        if (!mConfigured) { return false; }
        for (uint32_t i = 0; i < mConfig.order.Size(); ++i)
        {
            if (FindRegistered(mConfig.order[i].crc) < 0) { out.Add(mConfig.order[i]); }
        }
        return out.Size() > 0;
    }

    bool WorldStepChain::UnknownStages(StageIdList& out) const
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

    bool WorldStepChain::AllReady() const
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

    template <typename Fn>
    void WorldStepChain::VisitStages(StagePhase phase, Fn&& fn)
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

    bool WorldStepChain::Capture(StepKey key)
    {
        if (!BeginCall()) { return false; }
        if (mPhase != ChainPhase::Idle) { return false; }
        if (!AllReady())
        {
            DIA_ASSERT(false, "WorldStepChain::Capture called while AllReady() is false");
            return false;
        }
        mCapturedKey = key;
        VisitStages(StagePhase::Capture, [key](IWorldStepStage& s) { s.Capture(key); });
        mPhase = ChainPhase::Captured;
        return true;
    }

    bool WorldStepChain::Run(Dia::DeferredStep::IParallelExecutor& executor)
    {
        if (!BeginCall()) { return false; }
        if (mPhase != ChainPhase::Captured) { return false; }
        VisitStages(StagePhase::Run, [&executor](IWorldStepStage& s) { s.Run(executor); });
        mPhase = ChainPhase::Ran;
        return true;
    }

    bool WorldStepChain::Commit()
    {
        if (!BeginCall()) { return false; }
        if (mPhase != ChainPhase::Ran) { return false; }
        VisitStages(StagePhase::Commit, [](IWorldStepStage& s) { s.Commit(); });
        mPhase = ChainPhase::Idle;
        ApplyPendingRemovals();
        return true;
    }

    void WorldStepChain::Abandon()
    {
        if (!BeginCall()) { return; }
        mPhase = ChainPhase::Idle;
        mCapturedKey = 0;
        ApplyPendingRemovals();
    }

    bool WorldStepChain::RestoreRan(StepKey key)
    {
        if (!BeginCall()) { return false; }
        if (mPhase != ChainPhase::Idle) { return false; }
        if (!AllReady()) { return false; }
        mCapturedKey = key;
        mPhase = ChainPhase::Ran;
        return true;
    }

    IWorldStepStage* WorldStepChain::StageAt(uint32_t index) const
    {
        return index < mEnabledCount ? mEnabled[index].stage : nullptr;
    }

    StringCRC WorldStepChain::StageIdAt(uint32_t index) const
    {
        return index < mEnabledCount ? mEnabled[index].id : StringCRC();
    }
}
