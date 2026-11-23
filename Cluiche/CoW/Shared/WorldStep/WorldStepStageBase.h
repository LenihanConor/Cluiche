#pragma once

#include <stdint.h>

#include <atomic>
#include <chrono>
#include <thread>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaDeferredStep/IStepStage.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Health/HealthReporterBase.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <Shared/World/WorldSimNames.h>
#include <Shared/WorldStep/MutationQueue.h>
#include <Shared/WorldStep/SpinUpSlot.h>

namespace CoW {
namespace WorldStep {

// The world a stage spins up against. Opaque on purpose: the base is not coupled to WorldGenerationModule.
struct WorldView
{
    uint32_t revision = 0;
    bool present = false;
};

// Health reporter held by the base. One per stage.
class StageHealthReporter : public Dia::Observation::Health::HealthReporterBase
{
public:
    explicit StageHealthReporter(const Dia::Core::StringCRC& name)
        : mName(name)
    {
        SetOK();
    }
    Dia::Core::StringCRC GetReporterName() const override { return mName; }

private:
    Dia::Core::StringCRC mName;
};

// Behaviour every world-sim stage shares (readiness, promotion, queue, discard rule, spin-up, stop order).
// A system supplies its differences through Hooks. The owner module drives Start/Update/Stop; the base is
// not itself a SimModule.
// Threading: Run touches only the working sim and latch (worker thread). Everything else is SimPU-only,
// except the spin-up job body, which touches only the SpinUpSlot and its own sim.
template <class TSim, class TEntry, class TLatch>
class WorldStepStageBase : public Dia::DeferredStep::IStepStage
{
public:
    typedef uint32_t CauseCode;
    typedef const char* (*CauseTextFn)(CauseCode);

    static const uint32_t kQueueCapacity = 128;

    struct Hooks
    {
        virtual ~Hooks() {}
        // Build the sim for the world. false + cause -> health Failing, not ready. Runs on the spin-up job thread.
        virtual bool InitialiseSim(TSim& sim, const WorldView& world, CauseCode& cause) = 0;
        virtual bool ExtraReady() const { return true; }
        virtual bool LatchInputs(TLatch& latch, Dia::DeferredStep::StepKey key) = 0;   // false -> step invalid
        virtual void ApplyEntry(TSim& working, TLatch& latch, const TEntry& entry) = 0;
        virtual void RunStep(TSim& working, const TLatch& latch, Dia::DeferredStep::IParallelExecutor& executor) = 0;
        virtual void PublishCommit(const TSim& committed, uint32_t stepDay) = 0;
        virtual void OnStartExtra() {}
        virtual void OnStopExtra() {}
    };

    WorldStepStageBase(Hooks& hooks, const char* sysToken, const char* logChannel, CauseTextFn causeText)
        : mHooks(hooks)
        , mSysToken(sysToken)
        , mLogChannel(logChannel)
        , mCauseText(causeText)
        , mHealth(Dia::Core::StringCRC(World::SaveId(sysToken).c_str()))
    {
    }

    ~WorldStepStageBase() override
    {
        Teardown(false);
    }

    // ----- owner-driven lifecycle ------------------------------------------------------------------

    void Start()
    {
        if (mStarted) { return; }
        mStarted = true;
        ++mVisitGeneration;
        Dia::Observation::Health::HealthRegistry::Instance().Register(&mHealth);
        RegisterMetrics();
        mHooks.OnStartExtra();
        DIA_LOG_INFO(mLogChannel, "%s: started (visit %u)", mLogChannel, static_cast<unsigned>(mVisitGeneration));
        if (mWorld.present) { BeginSpinUp(mWorld); }
    }

    // Polls the spin-up slot. SimPU only.
    void Update()
    {
        TSim* sim = nullptr;
        uint32_t revision = 0;
        if (!mSpinUp.TryTake(sim, revision)) { return; }
        if (mResultGeneration.load(std::memory_order_acquire) != mVisitGeneration || revision != mTargetRevision)
        {
            DIA_LOG_INFO(mLogChannel, "%s: stale spin-up result for revision %u dropped", mLogChannel,
                         static_cast<unsigned>(revision));
            delete sim;
            return;
        }
        if (sim == nullptr)
        {
            const CauseCode cause = mJobCause.load(std::memory_order_acquire);
            mCause = cause;
            mHealth.SetFailing(Dia::Core::StringCRC(CauseText(cause)));
            DIA_LOG_ERROR(mLogChannel, "%s: spin-up failed for revision %u: %s", mLogChannel,
                          static_cast<unsigned>(revision), CauseText(cause));
            return;
        }
        delete mPending;
        mPending = sim;
        mPendingRevision = revision;
    }

    // ----- world events ----------------------------------------------------------------------------

    // The world failed: not ready, own job cancelled, the last world is not kept.
    void OnWorldGenerationFailed()
    {
        mSpinUp.Cancel();
        TSim* result = nullptr;
        uint32_t resultRevision = 0;
        if (mSpinUp.TryTake(result, resultRevision)) { delete result; }
        mWorld.present = false;
        mCause = 0;
        mHealth.SetFailing(Dia::Core::StringCRC("world_generation_failed"));
        DIA_LOG_ERROR(mLogChannel, "%s: world generation failed, dropping the held world", mLogChannel);
        if (mInFlight) { mDropAfterStep = true; }   // the worker may be reading the active sim
        else { DropSims(); }
    }

    void OnWorldGenerationComplete(const WorldView& world)
    {
        mWorld = world;
        mTargetRevision = world.revision;
        mCause = 0;
        mHealth.SetOK();
        mQueueDegraded = false;
        if (mStarted) { BeginSpinUp(world); }
    }

    // ----- mutation queue --------------------------------------------------------------------------

    bool Enqueue(const TEntry& entry)
    {
        if (mQueue.Enqueue(entry)) { return true; }
        DIA_LOG_WARNING(mLogChannel, "%s: mutation queue full (%u), entry refused", mLogChannel,
                        static_cast<unsigned>(kQueueCapacity));
        mQueueDegraded = true;
        if (mHealth.Report().status == Dia::Observation::Health::HealthStatus::kOK)
        {
            mHealth.SetDegraded(Dia::Core::StringCRC("queue_full"));
        }
        return false;
    }

    // ----- IStepStage ------------------------------------------------------------------------------

    bool IsReady() const override
    {
        const bool haveSim = (mPending != nullptr && mPendingRevision == mTargetRevision)
                          || (mActive != nullptr && mActiveRevision == mTargetRevision);
        return haveSim && mHealth.Report().status != Dia::Observation::Health::HealthStatus::kFailing
            && mHooks.ExtraReady();
    }

    void Capture(Dia::DeferredStep::StepKey key) override
    {
        // Promotion happens here and nowhere else.
        if (mPending != nullptr)
        {
            if (mPendingRevision == mTargetRevision)
            {
                delete mActive;
                mActive = mPending;
                mActiveRevision = mPendingRevision;
            }
            else
            {
                delete mPending;
            }
            mPending = nullptr;
        }
        mCaptureRevision = mTargetRevision;
        mStepValid = (mActive != nullptr && mActiveRevision == mTargetRevision);
        mInFlight = true;
        mDropAfterStep = false;
        mCapturedDay = static_cast<uint32_t>(key);

        TEntry entry;
        while (mQueue.Dequeue(entry))
        {
            if (mStepValid) { mHooks.ApplyEntry(*mActive, mLatch, entry); }
        }
        if (mQueueDegraded)
        {
            mQueueDegraded = false;
            if (mHealth.Report().status == Dia::Observation::Health::HealthStatus::kDegraded) { mHealth.SetOK(); }
        }
        if (mStepValid && !mHooks.LatchInputs(mLatch, key)) { mStepValid = false; }
    }

    void Run(Dia::DeferredStep::IParallelExecutor& executor) override
    {
        if (!mStepValid) { return; }
        const std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
        mHooks.RunStep(*mActive, mLatch, executor);
        mLastStepMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
    }

    void Commit() override
    {
        mInFlight = false;
        const bool discard = mCaptureRevision != mTargetRevision || !mStepValid || mDropAfterStep;
        if (discard)
        {
            DIA_LOG_INFO(mLogChannel, "%s: step discarded (%s)", mLogChannel,
                         mCaptureRevision != mTargetRevision ? "world revision changed"
                         : (mDropAfterStep ? "world dropped" : "step invalid"));
            if (mDiscardedCounter != nullptr) { mDiscardedCounter->Inc(); }
        }
        else
        {
            if (mStepMsGauge != nullptr) { mStepMsGauge->Set(mLastStepMs); }
            mHooks.PublishCommit(*mActive, mCapturedDay);
        }
        if (mDropAfterStep)
        {
            DropSims();
            mDropAfterStep = false;
        }
    }

    void SerializePending(Dia::SaveGame::SaveContext&) const override {}
    void DeserializePending(Dia::SaveGame::LoadContext&) override {}
    bool ValidatePending(Dia::SaveGame::LoadContext& ctx) const override { return ValidatePendingBlob(ctx); }

    // ----- inspection ------------------------------------------------------------------------------

    bool HasActive() const { return mActive != nullptr; }
    bool HasPending() const { return mPending != nullptr; }
    uint32_t QueueCount() const { return mQueue.Count(); }
    uint32_t VisitGeneration() const { return mVisitGeneration; }
    uint32_t TargetRevision() const { return mTargetRevision; }
    CauseCode GetCauseCode() const { return mCause; }
    Dia::Observation::Health::Health GetHealth() const { return mHealth.Report(); }
    const char* SystemToken() const { return mSysToken; }
    World::Name GetStageId() const { return World::StageId(mSysToken); }

    void Stop()
    {
        if (!mStarted) { return; }
        mStarted = false;
        Teardown(true);
    }

protected:
    // Codec seam: the real pending-blob check arrives with the save codec.
    virtual bool ValidatePendingBlob(Dia::SaveGame::LoadContext&) const { return true; }

    // Called first in Stop; the owner unregisters the stage from its chain here.
    virtual void UnregisterStage() {}

    // Starts the spin-up job for (revision, generation). Default: one worker thread, joined in Stop.
    virtual void LaunchJob(uint32_t revision, uint32_t generation, const WorldView& world)
    {
        if (mJob.joinable()) { mJob.join(); }
        mJob = std::thread([this, revision, generation, world]() { RunJob(revision, generation, world); });
    }

    // Job body. Touches only the slot, its own sim and the result atomics.
    void RunJob(uint32_t revision, uint32_t generation, const WorldView& world)
    {
        if (mSpinUp.IsCancelled(revision)) { return; }
        TSim* sim = new TSim();
        CauseCode cause = 0;
        if (!mHooks.InitialiseSim(*sim, world, cause))
        {
            delete sim;
            sim = nullptr;
            mJobCause.store(cause, std::memory_order_release);
        }
        mResultGeneration.store(generation, std::memory_order_release);
        if (!mSpinUp.Complete(sim, revision)) { delete sim; }
    }

private:
    const char* CauseText(CauseCode cause) const { return mCauseText != nullptr ? mCauseText(cause) : "unknown"; }

    void DropSims()
    {
        delete mActive;  mActive = nullptr;
        delete mPending; mPending = nullptr;
    }

    void RegisterMetrics()
    {
        using Dia::Observation::Metric::MetricInfo;
        static const MetricInfo kStepMsInfo{ "World sim step CPU", "CPU time of the last committed step", "ms" };
        static const MetricInfo kDiscardedInfo{ "World sim discarded steps",
                                                "Steps dropped because the world changed or the step was invalid", "count" };
        auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
        mStepMsGauge = metrics.RegisterGauge(Dia::Core::StringCRC(World::MetricName(mSysToken, "step_ms").c_str()), kStepMsInfo);
        mDiscardedCounter = metrics.RegisterCounter(
            Dia::Core::StringCRC(World::MetricName(mSysToken, "discarded_steps").c_str()), kDiscardedInfo);
    }

    void BeginSpinUp(const WorldView& world)
    {
        TSim* stale = nullptr;
        uint32_t staleRevision = 0;
        while (!mSpinUp.TryBegin(world.revision))
        {
            if (mSpinUp.TryTake(stale, staleRevision)) { delete stale; }   // unconsumed result of an older revision
            else { std::this_thread::yield(); }                             // worker mid-publish
        }
        LaunchJob(world.revision, mVisitGeneration, world);
    }

    // Common shutdown. callHooks=false from the destructor (the hooks may already be gone).
    void Teardown(bool callHooks)
    {
        if (callHooks) { UnregisterStage(); }
        mSpinUp.Cancel();
        if (mJob.joinable()) { mJob.join(); }
        TSim* leftover = nullptr;
        uint32_t leftoverRevision = 0;
        if (mSpinUp.TryTake(leftover, leftoverRevision)) { delete leftover; }
        mSpinUp.Reset();
        DropSims();
        mQueue.Clear();
        // TODO(kit task 10): release the world-generation hold here once HoldGenerationToken exists
        // (spec order: ... destroy sims, clear queue, release hold, OnStopExtra).
        Dia::Observation::Health::HealthRegistry::Instance().Unregister(&mHealth);
        if (callHooks) { mHooks.OnStopExtra(); }
    }

    Hooks& mHooks;
    const char* mSysToken;
    const char* mLogChannel;
    CauseTextFn mCauseText;
    StageHealthReporter mHealth;

    TSim* mActive = nullptr;
    TSim* mPending = nullptr;
    uint32_t mTargetRevision = 0;
    uint32_t mActiveRevision = 0;
    uint32_t mPendingRevision = 0;
    uint32_t mCaptureRevision = 0;
    uint32_t mCapturedDay = 0;
    bool mStepValid = false;
    bool mInFlight = false;        // between Capture and Commit
    bool mDropAfterStep = false;   // the world failed mid-step; drop the sims at Commit
    double mLastStepMs = 0.0;      // written by Run (worker), read by Commit (SimPU) after Run returns
    Dia::Observation::Metric::Gauge* mStepMsGauge = nullptr;
    Dia::Observation::Metric::Counter* mDiscardedCounter = nullptr;
    uint32_t mVisitGeneration = 0;
    bool mStarted = false;
    bool mQueueDegraded = false;
    CauseCode mCause = 0;
    WorldView mWorld;
    TLatch mLatch;
    MutationQueue<TEntry, kQueueCapacity> mQueue;
    SpinUpSlot<TSim> mSpinUp;
    std::thread mJob;
    std::atomic<uint32_t> mResultGeneration{0};
    std::atomic<uint32_t> mJobCause{0};
};

}  // namespace WorldStep
}  // namespace CoW
