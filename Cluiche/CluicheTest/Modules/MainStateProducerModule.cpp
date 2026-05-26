#include "Modules/MainStateProducerModule.h"
#include "Modules/TestStages/TestResultsRegistry.h"
#include "Modules/AutomationModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CluicheTest {

const Dia::Core::StringCRC MainStateProducerModule::kTypeId("MainStateProducerModule");

MainStateProducerModule::MainStateProducerModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult MainStateProducerModule::DoStart()
{
    DIA_LOG_INFO("Application", "MainStateProducerModule started");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void MainStateProducerModule::DoUpdate(float /*dt*/)
{
    Cluiche::AppFlow::MainToRenderFrame frame;

    // Populate AutomationStatus from AutomationModule (same PU)
    if (auto* am = Cluiche::AppFlow::AutomationModule::GetStatic())
    {
        if (auto* svc = am->GetService())
            frame.automationStatus.heartbeatEnabled = svc->IsHeartbeatEnabled();
    }

    // Populate StageHUDState from TestResultsRegistry (same process, MainPU writer)
    if (TestResultsRegistry::IsCreated())
    {
        const TestResultsRegistry& reg = TestResultsRegistry::GetInstance();
        const Dia::Core::StringCRC activeStageName = reg.GetActiveStage();
        const CluicheTest::StageResult* result =
            activeStageName.Value() != 0 ? reg.GetResult(activeStageName) : nullptr;

        frame.stageHUD.activeStageName = activeStageName;
        frame.stageHUD.frameCount      = reg.GetActiveFrameCount();

        if (result)
        {
            using HUDState = Cluiche::AppFlow::StageHUDState::StageState;
            switch (result->state)
            {
                case StageResult::State::kNotRun:  frame.stageHUD.stageState = HUDState::kNotRun;  break;
                case StageResult::State::kRunning: frame.stageHUD.stageState = HUDState::kRunning; break;
                case StageResult::State::kPassed:  frame.stageHUD.stageState = HUDState::kPassed;  break;
                case StageResult::State::kFailed:  frame.stageHUD.stageState = HUDState::kFailed;  break;
                case StageResult::State::kTimeout: frame.stageHUD.stageState = HUDState::kTimeout; break;
            }

            frame.stageHUD.budgetFrames     = result->budgetFrames;
            frame.stageHUD.checkpointCount  = result->checkpoints.Size();

            const unsigned int count = result->checkpoints.Size() < Cluiche::AppFlow::StageHUDState::kMaxCheckpoints
                ? result->checkpoints.Size()
                : Cluiche::AppFlow::StageHUDState::kMaxCheckpoints;
            for (unsigned int i = 0; i < count; ++i)
                frame.stageHUD.checkpoints[i] = result->checkpoints[i];
        }
    }

    mLastFrame = frame;
    mFrameOutput.Write(mLastFrame, Dia::Core::TimeAbsolute::Zero());
}

Dia::ApplicationFlow::StopResult MainStateProducerModule::DoStop()
{
    // Flush a zeroed frame so RenderPU doesn't show stale HUD after stop.
    mLastFrame = Cluiche::AppFlow::MainToRenderFrame{};
    mFrameOutput.Write(mLastFrame, Dia::Core::TimeAbsolute::Zero());
    DIA_LOG_INFO("Application", "MainStateProducerModule stopped");
    return Dia::ApplicationFlow::StopResult::kDone;
}

void MainStateProducerModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mFrameOutput.Connect(app);
}

} // namespace CluicheTest

namespace { using MainStateProducerModule_ = CluicheTest::MainStateProducerModule; }
DIA_MODULE(MainStateProducerModule_);
