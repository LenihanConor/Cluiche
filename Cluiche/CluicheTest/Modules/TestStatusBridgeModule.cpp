#include "Modules/TestStatusBridgeModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>

namespace CluicheTest {

const Dia::Core::StringCRC TestStatusBridgeModule::kTypeId("TestStatusBridgeModule");

TestStatusBridgeModule::TestStatusBridgeModule(const Dia::Core::StringCRC& instanceId)
    : RenderModule(instanceId)
{}

Cluiche::AppFlow::StageStatusModule::StageStatus TestStatusBridgeModule::MapResultToStatus(const StageResult* result)
{
    using Severity = Cluiche::AppFlow::StageStatusModule::Severity;
    Cluiche::AppFlow::StageStatusModule::StageStatus status;
    status.label = "NOT RUN";
    status.severity = Severity::kNone;
    status.visited = false;
    if (result == nullptr)
        return status;

    switch (result->state)
    {
    case StageResult::State::kPassed:
        status.label = "PASSED";  status.severity = Severity::kOk;   status.visited = true; break;
    case StageResult::State::kFailed:
        status.label = "FAILED";  status.severity = Severity::kFail; status.visited = true; break;
    case StageResult::State::kTimeout:
        status.label = "TIMEOUT"; status.severity = Severity::kFail; status.visited = true; break;
    case StageResult::State::kRunning:
        status.label = "RUNNING"; status.severity = Severity::kNone; status.visited = true; break;
    default:
        break;
    }
    return status;
}

Dia::ApplicationFlow::StartResult TestStatusBridgeModule::DoStart()
{
    if (mStageStatus.Get() == nullptr)
        return Dia::ApplicationFlow::StartResult::kLoading;

    mStages.RemoveAll();
    GetApplication()->GetStageTransitions(Dia::Core::StringCRC("Boot"), mStages);
    Publish();
    return Dia::ApplicationFlow::StartResult::kReady;
}

void TestStatusBridgeModule::DoUpdate(const Dia::SimTime::RenderTimeContext& /*ctx*/)
{
    Publish();
}

Dia::ApplicationFlow::StopResult TestStatusBridgeModule::DoStop()
{
    return Dia::ApplicationFlow::StopResult::kDone;
}

void TestStatusBridgeModule::Publish()
{
    Cluiche::AppFlow::StageStatusModule* table = mStageStatus.Get();
    if (table == nullptr)
        return;

    const bool haveRegistry = TestResultsRegistry::IsCreated();
    for (unsigned int i = 0; i < mStages.Size(); ++i)
    {
        const StageResult* result = haveRegistry ? TestResultsRegistry::GetInstance().GetResult(mStages[i]) : nullptr;
        table->Set(mStages[i], MapResultToStatus(result));
    }
}

} // namespace CluicheTest

namespace { using TestStatusBridgeModule_ = CluicheTest::TestStatusBridgeModule; }
DIA_MODULE(TestStatusBridgeModule_);
DIA_DESCRIBE(TestStatusBridgeModule_::kTypeId, "Publishes TestResultsRegistry results to StageStatusModule for the boot menu.");
