// Test support for the world-step chain. Ships with the library.
// Compiles against IParallelExecutor only.
#pragma once

#include <Shared/WorldStep/IWorldStepStage.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <cstdint>

namespace CoW::WorldStep::Testing
{
    enum class CallPhase : uint8_t { Capture, Run, Commit };

    struct CallRecord
    {
        Dia::Core::StringCRC stageId;
        CallPhase            phase = CallPhase::Capture;
        StepKey              key   = 0;   // Capture only; 0 otherwise
    };

    using CallLog = Dia::Core::Containers::DynamicArrayC<CallRecord, 128>;

    class FakeWorldStepStage;

    // Records another fake's committed value at the owner's own Commit time.
    struct CommitOrderProbe
    {
        const FakeWorldStepStage* other    = nullptr;
        int                       observed = -1;
        bool                      fired    = false;
    };

    class FakeWorldStepStage final : public IWorldStepStage
    {
    public:
        FakeWorldStepStage(Dia::Core::StringCRC id, CallLog& log) : mId(id), mLog(log) {}

        void SetReady(bool ready)                { mReady = ready; }
        void SetParallelForCalls(uint32_t n)     { mParallelForCalls = n; }
        void SetCommitProbe(CommitOrderProbe* p) { mProbe = p; }

        bool IsReady() const override { return mReady; }

        void Capture(StepKey key) override
        {
            Record(CallPhase::Capture, key);
            if (resetOnCapture) { working = 0; result = 0; }
            working = source;
            mCapturedKey = key;
        }

        void Run(Dia::DeferredStep::IParallelExecutor& executor) override
        {
            Record(CallPhase::Run, 0);
            mLastExecutor = &executor;
            for (uint32_t i = 0; i < mParallelForCalls; ++i)
            {
                executor.ParallelFor(1, 1, [](uint32_t, uint32_t) {});
            }
            result = working * 3;
        }

        void Commit() override
        {
            Record(CallPhase::Commit, 0);
            if (mProbe && mProbe->other)
            {
                mProbe->observed = mProbe->other->committed;
                mProbe->fired = true;
            }
            committed = result;
        }

        // Feature 3 extends these with a checksum.
        void SerializePending(Dia::SaveGame::SaveContext&) const override {}
        void DeserializePending(Dia::SaveGame::LoadContext&) override {}
        bool ValidatePending(Dia::SaveGame::LoadContext&) const override { return true; }

        const Dia::DeferredStep::IParallelExecutor* LastExecutor() const { return mLastExecutor; }
        StepKey CapturedKey() const { return mCapturedKey; }

        int  source    = 0;   // tests overwrite this after Capture to poison it
        int  working   = 0;
        int  result    = 0;
        int  committed = 0;
        bool resetOnCapture = false;

    private:
        void Record(CallPhase phase, StepKey key)
        {
            CallRecord r;
            r.stageId = mId;
            r.phase   = phase;
            r.key     = key;
            mLog.Add(r);
        }

        Dia::Core::StringCRC mId;
        CallLog&             mLog;
        CommitOrderProbe*    mProbe = nullptr;
        bool                 mReady = true;
        uint32_t             mParallelForCalls = 0;
        StepKey              mCapturedKey = 0;
        const Dia::DeferredStep::IParallelExecutor* mLastExecutor = nullptr;
    };
}
