#pragma once
#include "Modules/TestStages/TestStageModuleBase.h"
#include "Modules/TestStages/DeferredStepClockModule.h"
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include <Modules/DeferredStepModule.h>
#include <DiaDeferredStep/IDeferredStep.h>
#include <DiaParallelExecutor/IParallelExecutor.h>
#include <DiaSaveGame/SaveConfig.h>
#include <DiaSaveGame/SaveManager.h>
#include <DiaSaveGame/SaveRegistry.h>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace CluicheTest {

// Synthetic deferred step: spins for spinMs inside Run over a cell array split through the executor, and
// commits a checksum in Apply. The checksum is a commutative sum, so it is identical at any range count.
class SyntheticDeferredStep final : public Dia::DeferredStep::IDeferredStep
{
public:
    // Deterministic cell value for (stepIndex, cell). Used by the consumer to fill the live array and to
    // compute the expected checksum independently of the executor path.
    static uint32_t CellValue(uint32_t stepIndex, uint32_t cell);
    static uint64_t Mix(uint32_t v);
    static uint64_t ExpectedChecksum(uint32_t stepIndex, uint32_t cellCount);

    void Configure(uint32_t cellCount, uint32_t spinMs);
    // SimPU: the live (SimPU-owned) cells that Capture copies. The consumer refills them before each Kick.
    std::vector<uint32_t>& GetLiveCells() { return mLive; }

    void Capture() override;
    void Run(Dia::ParallelExecutor::IParallelExecutor& executor) override;
    void Apply() override;

    uint32_t GetPendingVersion() const override { return 1; }
    // The pending result is the checksum Run computed (mWorkChecksum), stored as a 16-digit hex string so the full
    // 64 bits survive the JSON number type. Apply commits it, so a restored step reproduces the identical checksum.
    bool ValidatePending(Dia::SaveGame::LoadContext& ctx) const override;
    void SerializePending(Dia::SaveGame::SaveContext& ctx) const override;
    void DeserializePending(Dia::SaveGame::LoadContext& ctx) override;

    // Test hook: overwrite the pending result with a sentinel so a load that fails to restore it is visible.
    void PoisonPending() { mWorkChecksum = 0xBADBADBADBADBADBull; }
    uint64_t GetPendingChecksum() const { return mWorkChecksum; }

    uint32_t GetApplyCount() const { return mApplyCount; }
    uint64_t GetCommittedChecksum() const { return mCommitted; }
    uint32_t GetLastRangeCount() const { return mLastRanges; }

private:
    uint32_t mCellCount = 0;
    uint32_t mSpinMs = 0;
    std::vector<uint32_t> mLive;       // SimPU-owned
    std::vector<uint32_t> mInput;      // working copy (Capture)
    std::vector<uint64_t> mOutput;     // working copy (Run)
    uint64_t mWorkChecksum = 0;        // working copy (Run)
    uint32_t mLastRanges = 0;
    uint64_t mCommitted = 0;           // what readers see (Apply)
    uint32_t mApplyCount = 0;
};

// Proves DeferredStepModule in a real app. Two SimPU modules cooperate: DeferredStepClockModule (key source,
// listed BEFORE DeferredStepModule) and this consumer (listed AFTER it); see DeferredStepClockModule.h.
//
// Kick cadence: kick on every key where no step is in flight, so steps pipeline back-to-back. Step n is kicked at
// key K and applies at K+k; the next step is kicked in the same sim step the previous one applied.
// save_at_step (8; 0 disables): right after step N is kicked (in flight, apply key not reached) the stage does
// SaveManager::Save then Load in the SAME sim step (both are synchronous on the calling thread; no gap needed).
// Save drains the step (completes it without applying) and stores its pending checksum; Load discards the drained
// step and restores it as complete with the stored kick/apply keys, so it applies once at kick+k without a stall.
// The clock is never rewound; the stage's own bookkeeping (mInFlight, record apply key) is already consistent.
// Stage config keys: k (7), spin_ms (50), steps (20), cells (4096), worker_cap (report only; the cap itself is
// set by the DeferredStepModule config in deferred_step_test_stage.diaapp).
// Verification is by DEFERREDSTEP_RESULT / DEFERREDSTEP_STEP lines in the session log.
class DeferredStepTestStageModule : public TestStageModuleBase
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription = "Integration stage for DiaDeferredStep: synthetic step on one channel with apply-key, stall, metric and health checks from committed data";
    explicit DeferredStepTestStageModule(const Dia::Core::StringCRC& instanceId);

protected:
    void OnConfigure(const char* configJson) override;
    Dia::Core::StringCRC GetStageName() const override;
    unsigned int GetBudgetFrames() const override;
    const Dia::Core::StringCRC* GetCheckpointNames(unsigned int& outCount) const override;
    bool AreDependenciesReady() override;
    void OnStart(Dia::Automation::AutomationService* service) override;
    void OnUpdate(float deltaTime) override;
    void OnStop() override;

private:
    struct StepRecord
    {
        uint64_t kickKey = 0;
        uint64_t applyKey = 0;
        uint64_t appliedAt = 0;
        uint64_t checksum = 0;
        uint64_t expected = 0;
    };

    enum class SaveLoadState { kSkipped, kOk, kFail };

    void Finalize();
    // SimPU, right after Kick of step `stepIndex` (the step is in flight, apply key not reached): Save then Load.
    void SaveThenLoadMidStep(uint32_t stepIndex, const StepRecord& rec);

    Dia::ApplicationFlow::ModuleRef<Dia::DeferredStep::DeferredStepModule> mDeferredRef{this};
    Dia::ApplicationFlow::ModuleRef<DeferredStepClockModule>               mClockRef{this};

    // Cached in OnStart: ModuleRef::Get() returns null once the target leaves kActive (e.g. in OnStop).
    Dia::DeferredStep::DeferredStepModule* mDeferred = nullptr;
    DeferredStepClockModule*               mClock = nullptr;

    SyntheticDeferredStep mStep;
    bool mRegistered = false;

    // Config
    uint32_t mK = 7;
    uint32_t mSpinMs = 50;
    uint32_t mSteps = 20;
    uint32_t mCells = 4096;
    std::string mWorkerCapCfg = "auto";
    double mStepBudgetMs = 100.0;   // see OnConfigure for the reasoning behind the default
    uint32_t mSaveAtStep = 8;       // step index whose in-flight window gets a Save+Load; 0 disables

    // Save services, owned by this SimPU module and handed to DeferredStepModule::SetSaveServices in OnStart.
    // They stay alive until the module is destroyed: DeferredStepModule detaches its participant in its own DoStop,
    // which runs after this module's OnStop.
    Dia::SaveGame::SaveConfig   mSaveConfig{};
    Dia::SaveGame::SaveRegistry mSaveRegistry;
    Dia::SaveGame::SaveManager  mSaveManager;
    bool mSaveReady = false;
    SaveLoadState mSaveLoad = SaveLoadState::kSkipped;
    bool mSaveLoadAttempted = false;
    int  mSaveLoadStep = -1;
    bool mSkipNextSample = false;   // the blocking save drain is spec'd behaviour; keep it out of the step-time budget

    // SimPU step wall time: steady_clock delta between consecutive OnUpdate calls (pre-verdict only)
    bool mHaveLastUpdate = false;
    std::chrono::steady_clock::time_point mLastUpdateTime;
    double mMaxStepMs = 0.0;
    double mSumStepMs = 0.0;
    uint64_t mStepSamples = 0;

    // Run state
    std::vector<StepRecord> mRecords;
    uint32_t mKicked = 0;
    uint32_t mObservedApplies = 0;
    bool mInFlight = false;
    bool mFinalized = false;
    unsigned int mHoldUntilFrame = 0;
    static constexpr unsigned int kHoldFrames = 120; // keep the stage (and checkpoint) observable before auto-return to Boot
    double mCapUtilisationPeak = 0.0;
    std::string mSummary = "pending";
    bool mPassed = false;
};

} // namespace CluicheTest
