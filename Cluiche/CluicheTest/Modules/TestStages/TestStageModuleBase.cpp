#include "Modules/TestStages/TestStageModuleBase.h"
#include "Modules/TestStages/TestResultsRegistry.h"

#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CluicheTest {

TestStageModuleBase::TestStageModuleBase(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

TestStageModuleBase::~TestStageModuleBase() = default;

Dia::ApplicationFlow::StartResult TestStageModuleBase::DoStart()
{
    auto* automationModule = mAutomation.Get();
    if (!automationModule || !automationModule->GetService())
        return Dia::ApplicationFlow::StartResult::kLoading;

    if (!AreDependenciesReady())
        return Dia::ApplicationFlow::StartResult::kLoading;

    ++mEntryCount;
    mFrameCount = 0;
    mResolved = false;

    unsigned int checkpointCount = 0;
    const auto* checkpointNames = GetCheckpointNames(checkpointCount);
    TestResultsRegistry::GetInstance().SetRunning(
        GetStageName(), GetBudgetFrames(), checkpointNames, checkpointCount);

    OnStart(automationModule->GetService());

    return Dia::ApplicationFlow::StartResult::kReady;
}

void TestStageModuleBase::DoUpdate(float deltaTime)
{
    ++mFrameCount;
    TestResultsRegistry::GetInstance().SetActiveFrameCount(mFrameCount);

    OnUpdate(deltaTime);

    if (!mResolved && mFrameCount >= GetBudgetFrames())
    {
        mResolved = true;
        OnTimeout();
        TestResultsRegistry::GetInstance().SetTimeout(GetStageName());
    }
}

Dia::ApplicationFlow::StopResult TestStageModuleBase::DoStop()
{
    OnStop();

    if (auto* automationModule = mAutomation.Get())
    {
        if (auto* service = automationModule->GetService())
            service->UnregisterCheckpoints(this);
    }

    mFrameCount = 0;
    mResolved = false;

    return Dia::ApplicationFlow::StopResult::kDone;
}

void TestStageModuleBase::ReportPassed()
{
    if (mResolved) return;
    mResolved = true;
    TestResultsRegistry::GetInstance().SetPassed(GetStageName(), mFrameCount);
}

void TestStageModuleBase::ReportFailed()
{
    if (mResolved) return;
    mResolved = true;
    TestResultsRegistry::GetInstance().SetFailed(GetStageName(), mFrameCount);
}

Dia::Automation::AutomationService* TestStageModuleBase::GetAutomationService()
{
    auto* mod = mAutomation.Get();
    return mod ? mod->GetService() : nullptr;
}

} // namespace CluicheTest
