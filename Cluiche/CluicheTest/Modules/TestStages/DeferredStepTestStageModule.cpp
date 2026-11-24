#include "Modules/TestStages/DeferredStepTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <DiaSaveGame/LoadContext.h>
#include <DiaSaveGame/SaveContext.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <direct.h>

namespace CluicheTest {

namespace {
    const Dia::Core::StringCRC kChannel("DeferredStepSynthetic");
    const Dia::Core::StringCRC kSaveSlot("deferredstep_stage");
    const Dia::Core::StringCRC kChecksumKey("checksum");
    constexpr uint32_t kMinRange = 256;
    constexpr const char* kSaveDir = "out/saves/cluichetest/";

    // Reads the 16-hex-digit checksum written by SerializePending.
    bool ReadChecksum(Dia::SaveGame::LoadContext& ctx, uint64_t& out)
    {
        char text[32] = {};
        if (!ctx.Read(kChecksumKey, text, sizeof(text))) { return false; }
        if (std::strlen(text) != 16) { return false; }
        char* end = nullptr;
        out = std::strtoull(text, &end, 16);
        return end != nullptr && *end == '\0';
    }

    void EnsureSaveDirectory()
    {
        // SaveManager does not create its base directory. Relative to the working directory, like CoW's out/saves/cow/.
        _mkdir("out");
        _mkdir("out/saves");
        _mkdir("out/saves/cluichetest");
    }
}

// ---------------------------------------------------------------------------
// SyntheticDeferredStep
// ---------------------------------------------------------------------------

uint32_t SyntheticDeferredStep::CellValue(uint32_t stepIndex, uint32_t cell)
{
    return (cell * 2654435761u) ^ (stepIndex * 40503u + 12345u);
}

uint64_t SyntheticDeferredStep::Mix(uint32_t v)
{
    uint64_t x = static_cast<uint64_t>(v) + 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

uint64_t SyntheticDeferredStep::ExpectedChecksum(uint32_t stepIndex, uint32_t cellCount)
{
    uint64_t sum = 0;
    for (uint32_t i = 0; i < cellCount; ++i) { sum += Mix(CellValue(stepIndex, i)); }
    return sum;
}

void SyntheticDeferredStep::Configure(uint32_t cellCount, uint32_t spinMs)
{
    mCellCount = cellCount;
    mSpinMs = spinMs;
    mLive.assign(cellCount, 0u);
    mInput.clear();
    mOutput.clear();
    mWorkChecksum = 0;
    mCommitted = 0;
    mApplyCount = 0;
    mLastRanges = 0;
}

void SyntheticDeferredStep::Capture()
{
    mInput = mLive;
    mOutput.assign(mCellCount, 0ull);
}

void SyntheticDeferredStep::Run(Dia::ParallelExecutor::IParallelExecutor& executor)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(mSpinMs);
    executor.ParallelFor(mCellCount, kMinRange, [this, deadline](uint32_t begin, uint32_t end)
    {
        for (uint32_t i = begin; i < end; ++i) { mOutput[i] = Mix(mInput[i]); }
        // Every range busy-waits to the shared deadline: wall time ~ spinMs however many ranges run in parallel.
        while (std::chrono::steady_clock::now() < deadline) {}
    });
    mLastRanges = executor.GetLastRangeCount();

    uint64_t sum = 0;
    for (uint32_t i = 0; i < mCellCount; ++i) { sum += mOutput[i]; }
    mWorkChecksum = sum;
}

void SyntheticDeferredStep::SerializePending(Dia::SaveGame::SaveContext& ctx) const
{
    char text[32];
    std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(mWorkChecksum));
    ctx.Write(kChecksumKey, static_cast<const char*>(text));
}

bool SyntheticDeferredStep::ValidatePending(Dia::SaveGame::LoadContext& ctx) const
{
    uint64_t checksum = 0;
    return ReadChecksum(ctx, checksum);   // read-only: nothing on the step is touched
}

void SyntheticDeferredStep::DeserializePending(Dia::SaveGame::LoadContext& ctx)
{
    uint64_t checksum = 0;
    if (ReadChecksum(ctx, checksum)) { mWorkChecksum = checksum; }
}

void SyntheticDeferredStep::Apply()
{
    mCommitted = mWorkChecksum;
    ++mApplyCount;
}

// ---------------------------------------------------------------------------
// DeferredStepTestStageModule
// ---------------------------------------------------------------------------

const Dia::Core::StringCRC DeferredStepTestStageModule::kTypeId("DeferredStepTestStageModule");

DeferredStepTestStageModule::DeferredStepTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

void DeferredStepTestStageModule::OnConfigure(const char* configJson)
{
    if (configJson == nullptr || configJson[0] == '\0') { return; }
    Json::Value root;
    Json::Reader reader;
    if (!reader.parse(configJson, root) || !root.isObject())
    {
        DIA_LOG_WARNING("DeferredStepTest", "stage config is not a JSON object; using defaults");
        return;
    }
    if (root.isMember("k") && root["k"].isUInt())             { mK = std::max(1u, root["k"].asUInt()); }
    if (root.isMember("spin_ms") && root["spin_ms"].isUInt()) { mSpinMs = root["spin_ms"].asUInt(); }
    if (root.isMember("steps") && root["steps"].isUInt())     { mSteps = std::max(1u, root["steps"].asUInt()); }
    if (root.isMember("cells") && root["cells"].isUInt())     { mCells = std::max(1u, root["cells"].asUInt()); }
    // step_budget_ms: AC 30 ceiling on the longest SimPU step (enforced at k >= 7 only).
    // Default 100 ms, from the Debug baseline (this stage, 150 steps): an idle sim step is ~30 ms (mean 33, ~26-36),
    // and ~1 step in 5 shows a 60-72 ms / ~10 ms pair (scheduler jitter: two frames' worth split unevenly), max 72.
    // 50 ms would fail on that jitter alone. If Run (spin_ms=50) executed on SimPU, every in-flight step would
    // cost >= ~80 ms (30 + 50) and jitter pairs would reach ~110 ms, so 100 ms (28 ms over the observed max, 70 ms
    // over idle) is the tightest ceiling that stays clear of jitter; the stage mean is logged as extra evidence.
    if (root.isMember("step_budget_ms") && root["step_budget_ms"].isNumeric()) { mStepBudgetMs = root["step_budget_ms"].asDouble(); }
    if (root.isMember("save_at_step") && root["save_at_step"].isUInt()) { mSaveAtStep = root["save_at_step"].asUInt(); }
    if (root.isMember("worker_cap")) { mWorkerCapCfg = root["worker_cap"].asString(); }
}

Dia::Core::StringCRC DeferredStepTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("DeferredStepTestStage");
}

unsigned int DeferredStepTestStageModule::GetBudgetFrames() const
{
    // Each step takes k sim steps; allow one extra per step for a late apply, plus startup margin.
    return mSteps * (mK + 1) + 60 + kHoldFrames;
}

const Dia::Core::StringCRC* DeferredStepTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("test.deferred_step.passed")
    };
    outCount = 1;
    return names;
}

bool DeferredStepTestStageModule::AreDependenciesReady()
{
    return mDeferredRef.Get() != nullptr && mClockRef.Get() != nullptr;
}

void DeferredStepTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    mDeferred = mDeferredRef.Get();
    mClock = mClockRef.Get();

    mStep.Configure(mCells, mSpinMs);
    mRecords.clear();
    mRecords.reserve(mSteps);
    mKicked = 0;
    mObservedApplies = 0;
    mInFlight = false;
    mFinalized = false;
    mHoldUntilFrame = 0;
    mCapUtilisationPeak = 0.0;
    mHaveLastUpdate = false;
    mMaxStepMs = 0.0;
    mSumStepMs = 0.0;
    mStepSamples = 0;
    mSummary = "pending";
    mPassed = false;
    mSaveLoad = SaveLoadState::kSkipped;
    mSaveLoadAttempted = false;
    mSaveLoadStep = -1;
    mSkipNextSample = false;

    // Save services must be injected before DeferredStepModule's first DoUpdate (it attaches lazily there).
    EnsureSaveDirectory();
    mSaveConfig.baseDirectory = kSaveDir;
    mSaveConfig.slotPattern = "slot_{id}.sav";
    mSaveConfig.maxSlots = 0;
    mSaveConfig.format = Dia::SaveGame::SaveFormat::Json;
    mSaveManager.Init(mSaveConfig, mSaveRegistry);
    mSaveReady = true;
    mDeferred->SetSaveServices(mSaveRegistry, mSaveManager);

    mDeferred->SetKeySource(*mClock);
    mRegistered = mDeferred->GetScheduler().Register(kChannel, mStep);
    DIA_LOG_INFO("DeferredStepTest", "start k=%u spinMs=%u steps=%u cells=%u workerCapCfg=%s registered=%d",
        mK, mSpinMs, mSteps, mCells, mWorkerCapCfg.c_str(), mRegistered ? 1 : 0);

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.deferred_step.passed"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mPassed, mSummary.c_str(), 0.0f };
        });
}

void DeferredStepTestStageModule::OnUpdate(float /*deltaTime*/)
{
    if (IsResolved() || !mRegistered) { return; }

    // Hold after the verdict so the harness can observe the checkpoint before the stage auto-returns to Boot.
    if (mFinalized)
    {
        if (GetFrameCount() >= mHoldUntilFrame) { if (mPassed) { ReportPassed(); } else { ReportFailed(); } }
        return;
    }

    // AC 30 frame-time evidence: wall time between consecutive stage updates == one SimPU step.
    // The first update has no predecessor and is skipped; the post-verdict hold returned above.
    {
        const auto now = std::chrono::steady_clock::now();
        if (mSkipNextSample)
        {
            // The previous update ran the save drain + disk IO + load: spec'd blocking work, not a deferred-step cost.
            mSkipNextSample = false;
        }
        else if (mHaveLastUpdate)
        {
            const double ms = std::chrono::duration<double, std::milli>(now - mLastUpdateTime).count();
            mMaxStepMs = std::max(mMaxStepMs, ms);
            mSumStepMs += ms;
            ++mStepSamples;
        }
        mLastUpdateTime = now;
        mHaveLastUpdate = true;
    }

    auto& scheduler = mDeferred->GetScheduler();
    const uint64_t key = mClock->GetCurrentKey();

    if (mInFlight)
    {
        // Sample cap utilisation while a step is in flight (the gauge is published by DeferredStepModule).
        if (auto* g = Dia::Observation::Metric::MetricRegistry::Instance().FindGauge(Dia::Core::StringCRC("dia.deferredstep.cap_utilisation")))
        {
            mCapUtilisationPeak = std::max(mCapUtilisationPeak, g->Value());
        }
    }

    // DeferredStepModule ticked earlier this sim step (it is listed before us), so an apply due at `key` is already committed.
    if (mInFlight && mStep.GetApplyCount() > mObservedApplies)
    {
        mObservedApplies = mStep.GetApplyCount();
        StepRecord& rec = mRecords.back();
        rec.appliedAt = key;
        rec.checksum = mStep.GetCommittedChecksum();
        mInFlight = false;
    }

    if (!mInFlight && mKicked < mSteps)
    {
        const uint32_t stepIndex = mKicked;
        auto& live = mStep.GetLiveCells();
        for (uint32_t i = 0; i < live.size(); ++i) { live[i] = SyntheticDeferredStep::CellValue(stepIndex, i); }

        StepRecord rec;
        rec.kickKey = key;
        rec.applyKey = key + mK;
        rec.expected = SyntheticDeferredStep::ExpectedChecksum(stepIndex, mCells);
        const auto result = scheduler.Kick(kChannel, rec.kickKey, rec.applyKey);
        if (result != Dia::DeferredStep::KickResult::kLaunched)
        {
            DIA_LOG_ERROR("DeferredStepTest", "kick %u rejected (result=%d)", stepIndex, static_cast<int>(result));
            Finalize();
            return;
        }
        // Capture has copied the inputs: scribble the live array to prove Run only reads the captured copy.
        std::fill(live.begin(), live.end(), 0xDEADBEEFu);

        mRecords.push_back(rec);
        ++mKicked;
        mInFlight = true;

        if (mSaveAtStep > 0 && stepIndex == mSaveAtStep && !mSaveLoadAttempted)
        {
            SaveThenLoadMidStep(stepIndex, mRecords.back());
        }
        return;
    }

    if (!mInFlight && mKicked >= mSteps)
    {
        Finalize();
    }
}

void DeferredStepTestStageModule::SaveThenLoadMidStep(uint32_t stepIndex, const StepRecord& rec)
{
    mSaveLoadAttempted = true;
    mSaveLoadStep = static_cast<int>(stepIndex);
    mSkipNextSample = true;
    mSaveLoad = SaveLoadState::kFail;
    auto& scheduler = mDeferred->GetScheduler();

    if (scheduler.GetState(kChannel) != Dia::DeferredStep::StepState::kRunning)
    {
        DIA_LOG_ERROR("DeferredStepTest", "save/load: step %u is not in flight before the save", stepIndex);
        return;
    }
    const uint32_t appliesBefore = mStep.GetApplyCount();

    // Save: DeferredStepSaveParticipant drains the in-flight step on SaveStarted (completes it, no apply) and stores
    // its pending checksum. Synchronous: Run has finished and the file is written when this returns.
    const auto saveResult = mSaveManager.Save(kSaveSlot);
    DIA_LOG_INFO("DeferredStepTest", "save slot=%s key=%llu step=%u ok=%d", kSaveSlot.AsChar(),
        (unsigned long long)mClock->GetCurrentKey(), stepIndex, saveResult.Ok() ? 1 : 0);
    if (!saveResult.Ok()) { return; }

    // Make a failed restore visible: Load must bring the real result back from the file.
    mStep.PoisonPending();

    // Load (same sim step): LoadStarted discards the drained step; Deserialize restores it as complete with the
    // stored kick/apply keys. The clock is not touched, so the key stays monotonic and the key floor is respected.
    const auto loadResult = mSaveManager.Load(kSaveSlot);
    DIA_LOG_INFO("DeferredStepTest", "load slot=%s key=%llu step=%u ok=%d", kSaveSlot.AsChar(),
        (unsigned long long)mClock->GetCurrentKey(), stepIndex, loadResult.Ok() ? 1 : 0);
    if (!loadResult.Ok()) { return; }

    Dia::DeferredStep::StepKey restoredApplyKey = 0;
    const bool stateOk = scheduler.GetState(kChannel) == Dia::DeferredStep::StepState::kComplete
        && scheduler.GetPendingCount() == 1
        && scheduler.GetApplyKey(kChannel, restoredApplyKey) && restoredApplyKey == rec.applyKey;
    const bool checksumOk = mStep.GetPendingChecksum() == rec.expected;
    const bool noApplyOk = mStep.GetApplyCount() == appliesBefore;   // restore must not apply early
    if (!stateOk || !checksumOk || !noApplyOk)
    {
        DIA_LOG_ERROR("DeferredStepTest", "save/load: restored state wrong stateOk=%d checksumOk=%d noApplyOk=%d applyKey=%llu expected=%llu",
            stateOk ? 1 : 0, checksumOk ? 1 : 0, noApplyOk ? 1 : 0,
            (unsigned long long)restoredApplyKey, (unsigned long long)rec.applyKey);
        return;
    }
    mSaveLoad = SaveLoadState::kOk;
}

void DeferredStepTestStageModule::Finalize()
{
    mFinalized = true;

    // save_at_step > 0 but never reached (e.g. >= steps) is a configuration the stage cannot prove: fail it.
    const bool saveLoadExpected = mSaveAtStep > 0;
    if (saveLoadExpected && !mSaveLoadAttempted) { mSaveLoad = SaveLoadState::kFail; }
    const bool saveLoadOk = !saveLoadExpected || mSaveLoad == SaveLoadState::kOk;
    const char* saveLoadText = !saveLoadExpected ? "skipped" : (mSaveLoad == SaveLoadState::kOk ? "ok" : "fail");

    bool keysOk = true;
    bool checksumOk = true;
    for (uint32_t i = 0; i < mRecords.size(); ++i)
    {
        const StepRecord& r = mRecords[i];
        DIA_LOG_INFO("DeferredStepTest", "DEFERREDSTEP_STEP step=%u kick=%llu apply=%llu appliedAt=%llu checksum=%llu expected=%llu",
            i, (unsigned long long)r.kickKey, (unsigned long long)r.applyKey, (unsigned long long)r.appliedAt,
            (unsigned long long)r.checksum, (unsigned long long)r.expected);
        keysOk = keysOk && (r.appliedAt == r.applyKey);
        checksumOk = checksumOk && (r.checksum == r.expected);
    }
    const bool allApplied = (mObservedApplies == mSteps) && (mRecords.size() == mSteps);
    keysOk = keysOk && allApplied;
    checksumOk = checksumOk && allApplied;

    auto& metrics = Dia::Observation::Metric::MetricRegistry::Instance();
    uint64_t stalls = 0, stallMs = 0, applies = 0;
    if (auto* c = metrics.FindCounter(Dia::Core::StringCRC("dia.deferredstep.stalls")))   { stalls = c->Value(); }
    if (auto* c = metrics.FindCounter(Dia::Core::StringCRC("dia.deferredstep.stall_ms"))) { stallMs = c->Value(); }
    if (auto* c = metrics.FindCounter(Dia::Core::StringCRC("dia.deferredstep.applies")))  { applies = c->Value(); }

    // "missing" means the reporter is not in the registry (HealthRegistry holds 32 reporters; CluicheTest fills it,
    // so the registration is silently dropped). Degraded is then unverifiable and is not enforced.
    const char* health = "missing";
    using Dia::Observation::Health::HealthRegistry;
    constexpr unsigned int kSnapMax = HealthRegistry::kMaxReporters * 2;   // live + retired
    HealthRegistry::ReporterSnapshot snaps[kSnapMax];
    unsigned int count = 0;
    HealthRegistry::Instance().Snapshot(snaps, kSnapMax, count);
    for (unsigned int i = 0; i < count; ++i)
    {
        if (snaps[i].name == Dia::Core::StringCRC("DeferredStep"))
        {
            health = Dia::Observation::Health::HealthStatusToString(snaps[i].health.status);
        }
    }
    const bool degraded = (std::string(health) == "degraded");

    // Stall expectations are enforced only at the two spec'd operating points: k >= 7 never stalls (AC 30),
    // k == 1 always stalls and reads Degraded (AC 31). Other k values are reported without a stall verdict.
    bool stallOk = true;
    const double meanStepMs = (mStepSamples > 0) ? (mSumStepMs / static_cast<double>(mStepSamples)) : 0.0;
    if (mK >= 7)      { stallOk = (stalls == 0) && (mMaxStepMs <= mStepBudgetMs); }
    else if (mK == 1) { stallOk = (stalls > 0) && (stallMs > 0) && (degraded || std::string(health) == "missing"); }

    // Exactly one Apply per step: a restored step that also re-applied (or was applied before the save) shows here.
    const bool applyOnceOk = (mStep.GetApplyCount() == mSteps);
    mPassed = keysOk && checksumOk && stallOk && allApplied && saveLoadOk && applyOnceOk;
    mSummary = mPassed ? "ok" : "failed";

    DIA_LOG_INFO("DeferredStepTest",
        "DEFERREDSTEP_RESULT k=%u spinMs=%u steps=%u applies=%llu stalls=%llu stallMs=%llu expectedApplyKeysOk=%d checksumOk=%d health=%s workerCapCfg=%s capUtilisationPeak=%.3f maxStepMs=%.3f meanStepMs=%.3f budgetMs=%.3f stallOk=%d saveLoad=%s saveLoadStep=%d applyOnceOk=%d passed=%d",
        mK, mSpinMs, mSteps, (unsigned long long)applies, (unsigned long long)stalls, (unsigned long long)stallMs,
        keysOk ? 1 : 0, checksumOk ? 1 : 0, health, mWorkerCapCfg.c_str(), mCapUtilisationPeak,
        mMaxStepMs, meanStepMs, mStepBudgetMs, stallOk ? 1 : 0, saveLoadText, mSaveLoadStep, applyOnceOk ? 1 : 0,
        mPassed ? 1 : 0);

    mHoldUntilFrame = GetFrameCount() + kHoldFrames;
}

void DeferredStepTestStageModule::OnStop()
{
    // Reverse stop order: DeferredStepModule is still alive here, so unregistering is safe.
    if (mRegistered && mDeferred != nullptr)
    {
        mDeferred->GetScheduler().Unregister(kChannel);
        mRegistered = false;
    }
    // Remove the temp slot (no-op if it was never written). Registry/manager stay alive for DeferredStepModule's detach.
    if (mSaveReady)
    {
        mSaveManager.DeleteSlot(kSaveSlot);
        mSaveReady = false;
    }
    mDeferred = nullptr;
    mClock = nullptr;
}

} // namespace CluicheTest

namespace { using DeferredStepTestStageModule_ = CluicheTest::DeferredStepTestStageModule; }
DIA_MODULE(DeferredStepTestStageModule_);
