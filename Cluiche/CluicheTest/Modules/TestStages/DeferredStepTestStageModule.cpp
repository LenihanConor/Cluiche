#include "Modules/TestStages/DeferredStepTestStageModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>
#include <algorithm>
#include <chrono>

namespace CluicheTest {

namespace {
    const Dia::Core::StringCRC kChannel("DeferredStepSynthetic");
    constexpr uint32_t kMinRange = 256;
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

void SyntheticDeferredStep::Run(Dia::DeferredStep::IParallelExecutor& executor)
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
    if (root.isMember("worker_cap")) { mWorkerCapCfg = root["worker_cap"].asString(); }
}

Dia::Core::StringCRC DeferredStepTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("DeferredStepTestStage");
}

unsigned int DeferredStepTestStageModule::GetBudgetFrames() const
{
    // Each step takes k sim steps; allow one extra per step for a late apply, plus startup margin.
    return mSteps * (mK + 1) + 60;
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
    mCapUtilisationPeak = 0.0;
    mSummary = "pending";
    mPassed = false;

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
    if (IsResolved() || mFinalized || !mRegistered) { return; }

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
        return;
    }

    if (!mInFlight && mKicked >= mSteps)
    {
        Finalize();
    }
}

void DeferredStepTestStageModule::Finalize()
{
    mFinalized = true;

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
    constexpr unsigned int kSnapMax = 32;   // HealthRegistry::kMaxReporters is private
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
    if (mK >= 7)      { stallOk = (stalls == 0); }
    else if (mK == 1) { stallOk = (stalls > 0) && (stallMs > 0) && (degraded || std::string(health) == "missing"); }

    mPassed = keysOk && checksumOk && stallOk && allApplied;
    mSummary = mPassed ? "ok" : "failed";

    DIA_LOG_INFO("DeferredStepTest",
        "DEFERREDSTEP_RESULT k=%u spinMs=%u steps=%u applies=%llu stalls=%llu stallMs=%llu expectedApplyKeysOk=%d checksumOk=%d health=%s workerCapCfg=%s capUtilisationPeak=%.3f stallOk=%d passed=%d",
        mK, mSpinMs, mSteps, (unsigned long long)applies, (unsigned long long)stalls, (unsigned long long)stallMs,
        keysOk ? 1 : 0, checksumOk ? 1 : 0, health, mWorkerCapCfg.c_str(), mCapUtilisationPeak, stallOk ? 1 : 0, mPassed ? 1 : 0);

    if (mPassed) { ReportPassed(); } else { ReportFailed(); }
}

void DeferredStepTestStageModule::OnStop()
{
    // Reverse stop order: DeferredStepModule is still alive here, so unregistering is safe.
    if (mRegistered && mDeferred != nullptr)
    {
        mDeferred->GetScheduler().Unregister(kChannel);
        mRegistered = false;
    }
    mDeferred = nullptr;
    mClock = nullptr;
}

} // namespace CluicheTest

namespace { using DeferredStepTestStageModule_ = CluicheTest::DeferredStepTestStageModule; }
DIA_MODULE(DeferredStepTestStageModule_);
