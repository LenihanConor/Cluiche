#pragma once

#include <stdint.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaDeferredStep/IStepStage.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Health/HealthReporterBase.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaSaveGame/SaveContext.h>
#include <Shared/World/WorldSimNames.h>
#include <Shared/WorldStep/HoldGenerationToken.h>
#include <Shared/WorldStep/MutationQueue.h>
#include <Shared/WorldStep/SpinUpSlot.h>
#include <Shared/WorldStep/WorldSimSaveCodec.h>

namespace CoW {
namespace WorldStep {

// The world a stage spins up against. Opaque on purpose: the base is not coupled to WorldGenerationModule.
struct WorldView
{
    uint32_t revision = 0;
    bool present = false;
    uint32_t width = 0;     // cell dimensions, compared with a restored record (step 3)
    uint32_t height = 0;
};

// What Restore did with a record.
enum class RestoreResult : uint8_t
{
    kApplied,     // active sim restored, ready at once
    kNotReady,    // the record says ready:false (or is not an object): nothing restored
    kStashed,     // no world yet: kept for the next WorldGenerationComplete
    kDiscarded,   // mismatch or a failed hook: nothing restored, a normal spin-up runs instead
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
        virtual void RunStep(TSim& working, const TLatch& latch, Dia::ParallelExecutor::IParallelExecutor& executor) = 0;
        virtual void PublishCommit(const TSim& committed, uint32_t stepDay) = 0;
        virtual void OnStartExtra() {}
        virtual void OnStopExtra() {}
        // Restore (step 3): the system's own count check (e.g. basin_count) against the announced world.
        virtual bool ExtraCountMatches(const Json::Value& /*record*/, const WorldView& /*world*/) const { return true; }
        // Restore (step 4): load the committed record into a freshly initialised sim. false -> discard.
        virtual bool RestoreCommitted(TSim& /*sim*/, const Json::Value& /*record*/) { return true; }
        // Pending blob: the system arrays of the in-flight step result (added after the common header).
        virtual void WritePendingFields(Json::Value& /*blob*/, const TSim& /*sim*/) {}
        // Pending blob: apply a validated blob to the restored active sim. false -> the stage is not ready.
        virtual bool RestorePending(TSim& /*sim*/, const Json::Value& /*blob*/, const Json::Value& /*committed*/) { return true; }
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
        mHold.Take();   // generation stays held until the owner reports its subscription (NotifySubscribed) or Stop
        ++mVisitGeneration;
        Dia::Observation::Health::HealthRegistry::Instance().Register(&mHealth);
        RegisterMetrics();
        mHooks.OnStartExtra();
        DIA_LOG_INFO(mLogChannel, "%s: started (visit %u)", mLogChannel, static_cast<unsigned>(mVisitGeneration));
        const bool restoredHere = mRestoredValid && mWorld.revision == mRestoredRevision;
        if (mWorld.present && !restoredHere) { BeginSpinUp(mWorld); }
    }

    // The generation-hold seam. Set before Start; null -> no hold is taken.
    void SetGenerationHold(IGenerationHold* host) { mHold.SetHost(host); }
    // The owner calls this once its WorldGenerationComplete/Failed subscription is in place: releases the hold taken in
    // Start. Idempotent; Stop releases it too if the owner never did.
    void NotifySubscribed() { mHold.Release(); }
    bool GenerationHeld() const { return mHold.IsHeld(); }

    // Polls the spin-up slot. SimPU only.
    void Update()
    {
        TSim* sim = nullptr;
        uint32_t revision = 0;
        if (!mSpinUp.TryTake(sim, revision)) { return; }
        const bool supersededByRestore = mRestoredValid && revision == mRestoredRevision;
        if (supersededByRestore || mResultGeneration.load(std::memory_order_acquire) != mVisitGeneration
            || revision != mTargetRevision)
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
        mRestoredValid = false;
        mHaveStash = false;
        mCause = 0;
        mHealth.SetFailing(Dia::Core::StringCRC("world_generation_failed"));
        DIA_LOG_ERROR(mLogChannel, "%s: world generation failed, dropping the held world", mLogChannel);
        if (mInFlight) { mDropAfterStep = true; }   // the worker may be reading the active sim
        else { DropSims(); }
    }

    void OnWorldGenerationComplete(const WorldView& world)
    {
        mWorld = world;
        if (mRestoredValid && world.present && world.revision == mRestoredRevision)
        {
            // Restore step 7: the world we restored against is announced again. Keep the restored sim.
            DIA_LOG_INFO(mLogChannel, "%s: re-announce of restored revision %u ignored", mLogChannel,
                         static_cast<unsigned>(world.revision));
            return;
        }
        mRestoredValid = false;
        mTargetRevision = world.revision;
        mCause = 0;
        if (!mParamsDegraded) { mHealth.SetOK(); }
        mQueueDegraded = false;
        if (mHaveStash && world.present)
        {
            // Event-order fallback: the record arrived before the world did.
            Json::Value record = mStashRecord;
            mHaveStash = false;
            mStashRecord = Json::Value();
            ApplyRecord(record);
            return;
        }
        if (mStarted) { BeginSpinUp(world); }
    }

    // ----- restore ---------------------------------------------------------------------------------

    // The shared 7-step restore of a committed record (already passed Validate). SimPU only.
    RestoreResult Restore(const Json::Value& record)
    {
        // 1. cancel the job, drop pending; a late completion of the cancelled job is rejected by the slot
        mSpinUp.Cancel();
        TSim* result = nullptr;
        uint32_t resultRevision = 0;
        if (mSpinUp.TryTake(result, resultRevision)) { delete result; }
        delete mPending;
        mPending = nullptr;
        mRestoredValid = false;
        mHaveStash = false;
        mStashRecord = Json::Value();
        mHaveCommitted = false;
        mCommittedRecord = Json::Value();

        // 2. ready:false, or no world: stop
        const bool recordReady = record.isObject() && record["ready"].isBool() && record["ready"].asBool();
        if (!recordReady)
        {
            DIA_LOG_INFO(mLogChannel, "%s: saved record is not ready, nothing restored", mLogChannel);
            mHaveStashPending = false;
            if (mStarted && mWorld.present) { BeginSpinUp(mWorld); }
            return RestoreResult::kNotReady;
        }
        if (!mWorld.present)
        {
            DIA_LOG_INFO(mLogChannel, "%s: no world yet, record kept for the next world generation", mLogChannel);
            mStashRecord = record;
            mHaveStash = true;
            return RestoreResult::kStashed;
        }
        return ApplyRecord(record);
    }

    // The owner sets the current ParameterFile::Hash() before a restore and after a reload.
    void SetCurrentParamsHash(uint64_t hash)
    {
        mParamsHash = hash;
        mHaveParamsHash = true;
    }

    // A reload or a pack changed the values: the restore-time params_hash warning no longer applies.
    void NotifyParamsReloaded()
    {
        if (!mParamsDegraded) { return; }
        mParamsDegraded = false;
        if (mHealth.Report().status == Dia::Observation::Health::HealthStatus::kDegraded) { mHealth.SetOK(); }
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
        DIA_PROFILE_SCOPE("WorldStep.Capture", Dia::Observation::Profile::Category::kNone);
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
            if (!mParamsDegraded
                && mHealth.Report().status == Dia::Observation::Health::HealthStatus::kDegraded) { mHealth.SetOK(); }
        }
        if (mStepValid && !mHooks.LatchInputs(mLatch, key)) { mStepValid = false; }
    }

    void Run(Dia::ParallelExecutor::IParallelExecutor& executor) override
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

    // Pending blob: {pending_version:1, valid, step_day, cell_count, ...system fields}, or {pending_version:1, valid:false}
    // when no step result is in flight (none captured, or the step was or will be discarded).
    void SerializePending(Dia::SaveGame::SaveContext& ctx) const override
    {
        Json::Value blob(Json::objectValue);
        const bool inFlight = mInFlight && mStepValid && !mDropAfterStep && mActive != nullptr
                           && mCaptureRevision == mTargetRevision;
        if (inFlight)
        {
            WritePendingHeader(blob, mCapturedDay, mWorld.width * mWorld.height);
            mHooks.WritePendingFields(blob, *mActive);
        }
        else
        {
            WritePendingInvalid(blob);
        }
        Json::Value& root = ctx.Root();
        const Json::Value::Members names = blob.getMemberNames();
        for (const std::string& name : names) { root[name] = blob[name]; }
    }

    // Applies a validated blob to the restored sim. A blob that cannot be restored leaves the stage not ready.
    void DeserializePending(Dia::SaveGame::LoadContext& ctx) override
    {
        const Json::Value& blob = ctx.Root();
        mHaveStashPending = false;
        mStashPending = Json::Value();
        if (!blob.isObject() || !blob["valid"].isBool() || !blob["valid"].asBool()) { return; }   // nothing in flight
        if (mActive != nullptr)
        {
            ApplyPending(blob);
        }
        else if (mHaveStash)
        {
            mStashPending = blob;      // applied after the stashed record restores
            mHaveStashPending = true;
        }
        else
        {
            DIA_LOG_INFO(mLogChannel, "%s: pending blob has no restored sim to apply to, dropped", mLogChannel);
        }
    }
    bool ValidatePending(Dia::SaveGame::LoadContext& ctx) const override { return ValidatePendingBlob(ctx); }

    // ----- inspection ------------------------------------------------------------------------------

    // The sim a reader may look at on SimPU: the active sim of the target revision, else the pending one (spun up, not
    // yet promoted). Null before any sim exists for the target revision. Do not hold the pointer across a Capture.
    const TSim* ReadableSim() const
    {
        if (mActive != nullptr && mActiveRevision == mTargetRevision) { return mActive; }
        if (mPending != nullptr && mPendingRevision == mTargetRevision) { return mPending; }
        return nullptr;
    }
    bool IsReadyForRevision(uint32_t revision) const { return IsReady() && revision == mTargetRevision; }

    bool HasActive() const { return mActive != nullptr; }
    bool HasPending() const { return mPending != nullptr; }
    bool SpinUpRunning() const { return mSpinUp.State() == SpinUpSlot<TSim>::kRunning; }
    bool HasStashedRecord() const { return mHaveStash; }

    // The fields and caps ValidatePending checks a pending blob against. Not owned; null -> blobs are not checked.
    void SetRecordSpec(const RecordSpec* spec) { mSpec = spec; }
    // The committed record the pending blob decodes against. A system calls this from its own Validate (which runs
    // before any Deserialize), so ValidatePending can run before Restore.
    void NoteCommittedRecord(const Json::Value& record)
    {
        mCommittedRecord = record;
        mHaveCommitted = true;
    }
    uint32_t RestoredRevision() const { return mRestoredRevision; }
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
    // Codec seam: read-only pending-blob check through the Draoi codec. Without a RecordSpec it accepts.
    virtual bool ValidatePendingBlob(Dia::SaveGame::LoadContext& ctx) const
    {
        if (mSpec == nullptr) { return true; }
        const Json::Value* committed = mHaveCommitted ? &mCommittedRecord : (mHaveStash ? &mStashRecord : nullptr);
        Draoi::HexWorld::SaveError err;
        if (WorldStep::ValidatePendingBlob(ctx.Root(), committed, *mSpec, err)) { return true; }
        DIA_LOG_WARNING(mLogChannel, "%s: pending blob rejected at field %s: %s", mLogChannel, err.field.AsCStr(),
                        err.reason.AsCStr());
        return false;
    }

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

    // Restore steps 3-6 against the announced world.
    RestoreResult ApplyRecord(const Json::Value& record)
    {
        // 3. dimension or extra-count mismatch
        const bool dimsMatch = record["width"].isUInt() && record["height"].isUInt()
            && record["width"].asUInt() == mWorld.width && record["height"].asUInt() == mWorld.height;
        if (!dimsMatch || !mHooks.ExtraCountMatches(record, mWorld))
        {
            return DiscardRestore("saved dimensions or counts do not match the world");
        }
        // 4. Init, then the committed record
        TSim* sim = new TSim();
        CauseCode cause = 0;
        if (!mHooks.InitialiseSim(*sim, mWorld, cause))
        {
            delete sim;
            return DiscardRestore("sim initialisation failed");
        }
        if (!mHooks.RestoreCommitted(*sim, record))
        {
            delete sim;
            return DiscardRestore("committed record could not be restored");
        }
        // 5. active = target = current revision, ready at once
        delete mActive;
        mActive = sim;
        mActiveRevision = mTargetRevision = mRestoredRevision = mWorld.revision;
        mRestoredValid = true;
        mCommittedRecord = record;
        mHaveCommitted = true;
        mCause = 0;
        if (!mParamsDegraded) { mHealth.SetOK(); }
        DIA_LOG_INFO(mLogChannel, "%s: restored revision %u from the saved record", mLogChannel,
                     static_cast<unsigned>(mWorld.revision));
        // 6. params_hash
        if (mHaveParamsHash && !ParamsHashMatches(record, mParamsHash))
        {
            DIA_LOG_WARNING(mLogChannel, "%s: saved params_hash differs from the current parameters", mLogChannel);
            mParamsDegraded = true;
            mHealth.SetDegraded(Dia::Core::StringCRC("params_changed"));
        }
        if (mHaveStashPending)
        {
            mHaveStashPending = false;
            Json::Value blob = mStashPending;
            mStashPending = Json::Value();
            ApplyPending(blob);
        }
        return RestoreResult::kApplied;
    }

    void ApplyPending(const Json::Value& blob)
    {
        if (mHooks.RestorePending(*mActive, blob, mCommittedRecord)) { return; }
        DIA_LOG_WARNING(mLogChannel, "%s: pending blob could not be restored, stage not ready", mLogChannel);
        DropSims();
        mRestoredValid = false;
    }

    RestoreResult DiscardRestore(const char* why)
    {
        DIA_LOG_WARNING(mLogChannel, "%s: restore discarded: %s", mLogChannel, why);
        mHaveStashPending = false;
        mStashPending = Json::Value();
        DropSims();
        mRestoredValid = false;
        mTargetRevision = mWorld.revision;
        if (mStarted && mWorld.present) { BeginSpinUp(mWorld); }
        return RestoreResult::kDiscarded;
    }

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
        mRestoredValid = false;
        mQueue.Clear();
        mHold.Release();   // spec order: ... destroy sims, clear queue, release hold, OnStopExtra
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
    bool mParamsDegraded = false;       // restore found a different params_hash; cleared by NotifyParamsReloaded
    uint64_t mParamsHash = 0;
    bool mHaveParamsHash = false;
    uint32_t mRestoredRevision = 0;
    bool mRestoredValid = false;        // the active sim came from a restore at mRestoredRevision
    Json::Value mStashRecord;           // event-order fallback: record waiting for its world
    bool mHaveStash = false;
    Json::Value mStashPending;          // pending blob that arrived before the sim it applies to
    bool mHaveStashPending = false;
    Json::Value mCommittedRecord;       // last restored (or noted) committed record, for pending-blob decoding
    bool mHaveCommitted = false;
    const RecordSpec* mSpec = nullptr;
    CauseCode mCause = 0;
    WorldView mWorld;
    TLatch mLatch;
    MutationQueue<TEntry, kQueueCapacity> mQueue;
    SpinUpSlot<TSim> mSpinUp;
    HoldGenerationToken mHold;
    std::thread mJob;
    std::atomic<uint32_t> mResultGeneration{0};
    std::atomic<uint32_t> mJobCause{0};
};

}  // namespace WorldStep
}  // namespace CoW
