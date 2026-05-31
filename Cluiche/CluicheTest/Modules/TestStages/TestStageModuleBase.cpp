#include "Modules/TestStages/TestStageModuleBase.h"
#include "Modules/TestStages/TestResultsRegistry.h"

#include <DiaAutomation/AutomationService.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/Streams/ServiceStreamReader.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Capture/DiaCapture.h>

// Full instantiation lives here — header only forward-declares.
template class Dia::ApplicationFlow::ServiceStreamReader<Dia::Automation::AutomationService>;

namespace CluicheTest {

TestStageModuleBase::TestStageModuleBase(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
    , mAutomationServiceStream(new Dia::ApplicationFlow::ServiceStreamReader<Dia::Automation::AutomationService>(this, Dia::Core::StringCRC("AutomationService")))
{}

TestStageModuleBase::~TestStageModuleBase()
{
    delete mAutomationServiceStream;
}

Dia::ApplicationFlow::StartResult TestStageModuleBase::DoStart()
{
    if (!mAutomationServiceStream->IsAvailable())
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

    OnStart(&mAutomationServiceStream->Get());

    return Dia::ApplicationFlow::StartResult::kReady;
}

void TestStageModuleBase::DoUpdate(float deltaTime)
{
    if (!mResolved)
    {
        ++mFrameCount;
        TestResultsRegistry::GetInstance().SetActiveFrameCount(mFrameCount);
    }

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

    if (mAutomationServiceStream->IsAvailable())
        mAutomationServiceStream->Get().UnregisterCheckpoints(this);

    mFrameCount = 0;
    mResolved = false;

    return Dia::ApplicationFlow::StopResult::kDone;
}

void TestStageModuleBase::ReportPassed()
{
    if (mResolved) return;
    mResolved = true;
    TestResultsRegistry::GetInstance().SetPassed(GetStageName(), mFrameCount);
    DIA_CAPTURE(GetStageName(), "passed");
}

void TestStageModuleBase::ReportFailed()
{
    if (mResolved) return;
    mResolved = true;
    TestResultsRegistry::GetInstance().SetFailed(GetStageName(), mFrameCount);
    DIA_CAPTURE(GetStageName(), "failed");
}

Dia::Automation::AutomationService* TestStageModuleBase::GetAutomationService()
{
    return mAutomationServiceStream->IsAvailable() ? &mAutomationServiceStream->Get() : nullptr;
}

void TestStageModuleBase::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mAutomationServiceStream->Connect(app);
}

} // namespace CluicheTest
