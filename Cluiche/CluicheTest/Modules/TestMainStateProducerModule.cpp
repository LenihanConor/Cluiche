#include "Modules/TestMainStateProducerModule.h"
#include "Modules/TestStages/TestResultsRegistry.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
namespace CluicheTest {

const Dia::Core::StringCRC TestMainStateProducerModule::kTypeId("TestMainStateProducerModule");

TestMainStateProducerModule::TestMainStateProducerModule(const Dia::Core::StringCRC& instanceId)
    : MainStateProducerModule(instanceId)
{}

void TestMainStateProducerModule::DoPopulateFrame(Cluiche::AppFlow::MainToRenderFrame& frame)
{
    if (!TestResultsRegistry::IsCreated())
        return;

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

        frame.stageHUD.budgetFrames    = result->budgetFrames;
        frame.stageHUD.checkpointCount = result->checkpoints.Size();

        const unsigned int count = result->checkpoints.Size() < Cluiche::AppFlow::StageHUDState::kMaxCheckpoints
            ? result->checkpoints.Size()
            : Cluiche::AppFlow::StageHUDState::kMaxCheckpoints;
        for (unsigned int i = 0; i < count; ++i)
            frame.stageHUD.checkpoints[i] = result->checkpoints[i];
    }
}

} // namespace CluicheTest

namespace { using TestMainStateProducerModule_ = CluicheTest::TestMainStateProducerModule; }
DIA_MODULE(TestMainStateProducerModule_);
