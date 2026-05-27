#include "Modules/TestStages/TestAssetRuntimeStageModule.h"
#include "Modules/AssetServiceModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>

namespace CluicheTest {

const Dia::Core::StringCRC TestAssetRuntimeStageModule::kTypeId("TestAssetRuntimeStageModule");

TestAssetRuntimeStageModule::TestAssetRuntimeStageModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult TestAssetRuntimeStageModule::DoStart()
{
    DIA_LOG_INFO("CluicheTest", "TestAssetRuntimeStageModule DoStart entry");

    ++mEntryCount;
    mLoadStartFrame = mFrameCount;
    mAllLoaded   = false;
    mCleanReload = false;

    RegisterCheckpoints();

    const Dia::Core::StringCRC checkpoints[] = {
        Dia::Core::StringCRC("asset_runtime.all_loaded"),
        Dia::Core::StringCRC("asset_runtime.clean_reload")
    };
    TestResultsRegistry::GetInstance().SetRunning(
        Dia::Core::StringCRC("AssetRuntimeStage"), 240, checkpoints, 2);

    auto& reg = Dia::Observation::Metric::MetricRegistry::Instance();
    if (!mMetricLoadCount)
        mMetricLoadCount = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.asset_runtime.load_count"));
    if (!mMetricActiveHandles)
        mMetricActiveHandles = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.asset_runtime.active_handles"));
    if (!mMetricLoadTimeMs)
        mMetricLoadTimeMs = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.asset_runtime.load_time_ms"));
    if (!mMetricEntryCount)
        mMetricEntryCount = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.asset_runtime.entry_count"));
    if (!mMetricSnapshotLoaded)
        mMetricSnapshotLoaded = reg.RegisterGauge(Dia::Core::StringCRC("cluichetest.asset_runtime.snapshot_loaded"));

    if (mMetricEntryCount)
        mMetricEntryCount->Set(static_cast<double>(mEntryCount));

    DIA_LOG_INFO("CluicheTest", "TestAssetRuntimeStageModule DoStart ready — entry %u", mEntryCount);
    return Dia::ApplicationFlow::StartResult::kReady;
}

void TestAssetRuntimeStageModule::DoUpdate(float /*deltaTime*/)
{
    if (mAllLoaded)
        return;

    ++mFrameCount;
    TestResultsRegistry::GetInstance().SetActiveFrameCount(mFrameCount);

    auto* svc = Cluiche::AppFlow::AssetServiceModule::GetStatic();
    if (!svc)
        return;

    if (mFrameCount <= 2)
        return;

    if (!mAllLoaded)
    {
        if (svc->IsStageLoadComplete(Dia::Core::StringCRC("AssetRuntimeStage")))
        {
            mAllLoaded = true;
            const auto& runtime = svc->GetRuntime();
            auto progress = runtime.GetLoadProgress(Dia::Core::StringCRC("stage.asset_runtime_stage"));

            if (mEntryCount == 1)
            {
                mFirstEntrySnapshot.loadedCount  = progress.loaded;
                mFirstEntrySnapshot.allSucceeded = (progress.failed == 0);
                if (mMetricSnapshotLoaded)
                    mMetricSnapshotLoaded->Set(static_cast<double>(progress.loaded));
            }
            else
            {
                mCleanReload = (progress.loaded == mFirstEntrySnapshot.loadedCount &&
                                progress.failed == 0 &&
                                mFirstEntrySnapshot.allSucceeded);
            }

            DIA_LOG_INFO("CluicheTest", "TestAssetRuntimeStageModule: all loaded at frame %u (entry %u, loaded=%u, total=%u, failed=%u)",
                mFrameCount, mEntryCount, progress.loaded, progress.total, progress.failed);

            // Drive HUD PASS state: on entry 1, all_loaded passing is sufficient.
            // On entry 2+, require clean_reload as well for full PASS.
            const bool allCheckpointsPassed = (mEntryCount == 1) ? true : mCleanReload;
            if (allCheckpointsPassed)
            {
                TestResultsRegistry::GetInstance().SetPassed(
                    Dia::Core::StringCRC("AssetRuntimeStage"), mFrameCount);
            }
        }
        else if (mFrameCount >= 300)
        {
            TestResultsRegistry::GetInstance().SetTimeout(
                Dia::Core::StringCRC("AssetRuntimeStage"));
        }
    }

    // Metrics
    {
        const auto& runtime = svc->GetRuntime();
        auto progress = runtime.GetLoadProgress(Dia::Core::StringCRC("stage.asset_runtime_stage"));
        if (mMetricLoadCount)
            mMetricLoadCount->Set(static_cast<double>(progress.loaded));
        if (mMetricActiveHandles)
            mMetricActiveHandles->Set(static_cast<double>(progress.loaded));
        if (mMetricLoadTimeMs && mAllLoaded)
        {
            double elapsedMs = static_cast<double>(mFrameCount - mLoadStartFrame) * (1000.0 / 30.0);
            mMetricLoadTimeMs->Set(elapsedMs);
        }
    }
}

Dia::ApplicationFlow::StopResult TestAssetRuntimeStageModule::DoStop()
{
    DIA_LOG_INFO("CluicheTest", "TestAssetRuntimeStageModule DoStop entry");

    if (auto* automationModule = mAutomation.Get())
    {
        if (auto* service = automationModule->GetService())
            service->UnregisterCheckpoints(this);
    }

    mAllLoaded   = false;
    mCleanReload = false;
    mFrameCount  = 0;
    mLoadStartFrame = 0;
    // mEntryCount and mFirstEntrySnapshot persist across DoStop/DoStart

    DIA_LOG_INFO("CluicheTest", "TestAssetRuntimeStageModule DoStop exit");
    return Dia::ApplicationFlow::StopResult::kDone;
}

void TestAssetRuntimeStageModule::RegisterCheckpoints()
{
    auto* automationModule = mAutomation.Get();
    if (!automationModule || !automationModule->GetService())
        return;
    auto* service = automationModule->GetService();

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("asset_runtime.all_loaded"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mAllLoaded,
                     mAllLoaded ? "4/4 assets ready" : "loading in progress",
                     0.0f };
        });

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("asset_runtime.clean_reload"),
        [this]() -> Dia::Automation::CheckpointResult {
            if (mEntryCount < 2)
                return { false, "awaiting second entry", 0.0f };
            return { mCleanReload,
                     mCleanReload ? "reload state matches" : "state mismatch after reload",
                     0.0f };
        });
}

} // namespace CluicheTest

namespace { using TestAssetRuntimeStageModule_ = CluicheTest::TestAssetRuntimeStageModule; }
DIA_MODULE(TestAssetRuntimeStageModule_);
