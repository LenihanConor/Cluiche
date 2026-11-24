// Test support for the step chain. Ships with the library.
// Compiles against IParallelExecutor only.
#pragma once

#include <DiaDeferredStep/IStepChainListener.h>
#include <DiaDeferredStep/IStepStage.h>

#include <DiaSaveGame/LoadContext.h>
#include <DiaSaveGame/SaveContext.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <cstdint>

namespace Dia::DeferredStep::Testing
{
    enum class CallPhase : uint8_t { Capture, Run, Commit };

    struct CallRecord
    {
        Dia::Core::StringCRC stageId;
        CallPhase            phase = CallPhase::Capture;
        StepKey              key   = 0;   // Capture only; 0 otherwise
    };

    using CallLog = Dia::Core::Containers::DynamicArrayC<CallRecord, 128>;

    class FakeStepStage;

    // Records another fake's committed value at the owner's own Commit time.
    struct CommitOrderProbe
    {
        const FakeStepStage* other    = nullptr;
        int                       observed = -1;
        bool                      fired    = false;
    };

    class FakeStepStage final : public IStepStage
    {
    public:
        FakeStepStage(Dia::Core::StringCRC id, CallLog& log) : mId(id), mLog(log) {}

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

        void Run(Dia::ParallelExecutor::IParallelExecutor& executor) override
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

        // Feature 3: the pending result is the working copy plus what Run produced, guarded by a checksum.
        static int32_t ChecksumOf(int32_t w, int32_t r) { return w * 31 + r * 7 + 11; }

        void SerializePending(Dia::SaveGame::SaveContext& ctx) const override
        {
            ++serializeCalls;
            ctx.Write(Dia::Core::StringCRC("working"), static_cast<int32_t>(working));
            ctx.Write(Dia::Core::StringCRC("result"), static_cast<int32_t>(result));
            ctx.Write(Dia::Core::StringCRC("checksum"), ChecksumOf(working, result));
        }

        void DeserializePending(Dia::SaveGame::LoadContext& ctx) override
        {
            ++deserializeCalls;
            committedAtDeserialize = committed;
            int32_t w = 0, r = 0, c = 0;
            if (failDeserialize || !ctx.Read(Dia::Core::StringCRC("working"), w) || !ctx.Read(Dia::Core::StringCRC("result"), r)
                || !ctx.Read(Dia::Core::StringCRC("checksum"), c) || c != ChecksumOf(w, r))
            {
                mReady = false;
                return;
            }
            working = w;
            result = r;
        }

        // Read-only: never touches working/result/committed.
        bool ValidatePending(Dia::SaveGame::LoadContext& ctx) const override
        {
            ++validateCalls;
            if (failValidate)
                return false;
            int32_t w = 0, r = 0, c = 0;
            return ctx.Read(Dia::Core::StringCRC("working"), w) && ctx.Read(Dia::Core::StringCRC("result"), r)
                && ctx.Read(Dia::Core::StringCRC("checksum"), c) && c == ChecksumOf(w, r);
        }

        const Dia::ParallelExecutor::IParallelExecutor* LastExecutor() const { return mLastExecutor; }
        StepKey CapturedKey() const { return mCapturedKey; }

        int  source    = 0;   // tests overwrite this after Capture to poison it
        int  working   = 0;
        int  result    = 0;
        int  committed = 0;
        bool resetOnCapture = false;

        // Feature 3 instrumentation and fault switches.
        bool failValidate = false;
        bool failDeserialize = false;
        mutable uint32_t serializeCalls = 0;
        mutable uint32_t validateCalls = 0;
        uint32_t deserializeCalls = 0;
        int  committedAtDeserialize = -1;   // `committed` as seen when DeserializePending ran (participant order check)

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
        const Dia::ParallelExecutor::IParallelExecutor* mLastExecutor = nullptr;
    };

    // Records every StepChainApplied; from inside the callback it reads each observed stage's committed value
    // (AC 15) and can run a hook (the re-entrancy tests call back into the module through it).
    class FakeStepChainListener final : public IStepChainListener
    {
    public:
        using Hook = void (*)(void* user, const StepChainApplied& applied);
        static constexpr uint32_t kMaxObserved = 4;

        void OnStepChainApplied(const StepChainApplied& applied) override
        {
            received.Add(applied);
            if (sequence != nullptr)
                sequence->Add(tag);
            for (uint32_t i = 0; i < observedCount; ++i)
                observedCommitted[i] = observed[i]->committed;
            if (hook != nullptr)
                hook(hookUser, applied);
        }

        void Observe(const FakeStepStage* stage)
        {
            if (observedCount < kMaxObserved)
            {
                observed[observedCount] = stage;
                observedCommitted[observedCount] = -1;
                ++observedCount;
            }
        }

        Dia::Core::Containers::DynamicArrayC<StepChainApplied, 64> received;
        Dia::Core::Containers::DynamicArrayC<int, 16>* sequence = nullptr;   // shared order log across listeners
        int tag = 0;
        const FakeStepStage* observed[kMaxObserved] = {};
        int observedCommitted[kMaxObserved] = { -1, -1, -1, -1 };
        uint32_t observedCount = 0;
        Hook hook = nullptr;
        void* hookUser = nullptr;
    };
}
