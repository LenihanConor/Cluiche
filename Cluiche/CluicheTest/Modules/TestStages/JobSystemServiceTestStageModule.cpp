#include "Modules/TestStages/JobSystemServiceTestStageModule.h"
#include <DiaObservation/Log/DiaLog.h>

namespace Cluiche { namespace Test {

const Dia::Core::StringCRC JobSystemServiceTestStageModule::kTypeId("JobSystemServiceTestStageModule");

JobSystemServiceTestStageModule::JobSystemServiceTestStageModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{
}

void JobSystemServiceTestStageModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mJobSystemReader.Connect(app);
}

Dia::ApplicationFlow::StartResult JobSystemServiceTestStageModule::DoStart()
{
    DIA_LOG_INFO("Test", "JobSystemServiceTestStage: START");
    mParallelJobCounter = 0;
    mSerialFallbackCounter = 0;
    mFrameCount = 0;
    mTestCompleted = false;

    // AC1: ServiceStreamReader connects successfully
    if (!mJobSystemReader.IsConnected())
    {
        DIA_LOG_ERROR("Test", "JobSystemServiceTestStage: FAILED - ServiceStreamReader not connected");
        return Dia::ApplicationFlow::StartResult::kFailed;
    }

    DIA_LOG_INFO("Test", "JobSystemServiceTestStage: ServiceStreamReader connected");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void JobSystemServiceTestStageModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
{
    if (mTestCompleted) return;

    mFrameCount++;

    // AC2: Service is available after MainPU module registers it
    if (!mJobSystemReader.IsAvailable())
    {
        mSerialFallbackCounter++;

        if (mFrameCount > 10)
        {
            DIA_LOG_ERROR("Test", "JobSystemServiceTestStage: FAILED - Service never became available after 10 frames");
            mTestCompleted = true;
        }
        return;
    }

    // AC3: Can call Get() and receive valid JobSystem reference
    Dia::Threading::JobSystem& jobSystem = mJobSystemReader.Get();

    // AC4: TrySubmit returns valid handle and job executes
    Dia::Threading::JobHandle handle = jobSystem.TrySubmit([this]() {
        mParallelJobCounter++;
    });

    if (!handle.IsValid())
    {
        DIA_LOG_ERROR("Test", "JobSystemServiceTestStage: FAILED - TrySubmit returned invalid handle");
        mTestCompleted = true;
        return;
    }

    // Wait for job to complete
    jobSystem.Wait(handle);

    // Run for 5 frames to ensure stability
    if (mFrameCount >= 5)
    {
        if (mParallelJobCounter == 5 && mSerialFallbackCounter == 0)
        {
            DIA_LOG_INFO("Test", "JobSystemServiceTestStage: PASSED");
            DIA_LOG_INFO("Test", "  - ServiceStreamReader connected: YES");
            DIA_LOG_INFO("Test", "  - Service available: YES");
            DIA_LOG_INFO("Test", "  - Parallel jobs executed: %d", mParallelJobCounter.load());
            DIA_LOG_INFO("Test", "  - Serial fallbacks: %d", mSerialFallbackCounter.load());
        }
        else
        {
            DIA_LOG_ERROR("Test", "JobSystemServiceTestStage: FAILED - Unexpected counters");
            DIA_LOG_ERROR("Test", "  - Expected parallel: 5, got: %d", mParallelJobCounter.load());
            DIA_LOG_ERROR("Test", "  - Expected serial: 0, got: %d", mSerialFallbackCounter.load());
        }
        mTestCompleted = true;
    }
}

Dia::ApplicationFlow::StopResult JobSystemServiceTestStageModule::DoStop()
{
    DIA_LOG_INFO("Test", "JobSystemServiceTestStage: STOP");
    return Dia::ApplicationFlow::StopResult::kDone;
}

} } // namespace Cluiche::Test

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
namespace { using JobSystemServiceTestStageModule_ = Cluiche::Test::JobSystemServiceTestStageModule; }
DIA_MODULE(JobSystemServiceTestStageModule_);
DIA_DESCRIBE(JobSystemServiceTestStageModule_::kTypeId, "Integration test for cross-PU JobSystemService access");
