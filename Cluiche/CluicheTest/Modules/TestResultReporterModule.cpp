#include "Modules/TestResultReporterModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>

#include <chrono>

namespace CluicheTest {

const Dia::Core::StringCRC TestResultReporterModule::kTypeId("TestResultReporterModule");

TestResultReporterModule::TestResultReporterModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{}

Dia::ApplicationFlow::StartResult TestResultReporterModule::DoStart()
{
    return Dia::ApplicationFlow::StartResult::kReady;
}

void TestResultReporterModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mResultOut.Connect(app);
}

void TestResultReporterModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
{
    if (!TestResultsRegistry::IsCreated())
        return;

    const uint64_t utcMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    Cluiche::AppFlow::StageResultEvent events[8];
    const unsigned int count = mTracker.Poll(TestResultsRegistry::GetInstance(), utcMs, events, 8);
    for (unsigned int i = 0; i < count; ++i)
    {
        if (mResultOut.Send(events[i]) == Dia::ApplicationFlow::SendResult::kFailLoudRejected)
            DIA_LOG_WARNING("STAGES", "StageResult send rejected: stage=%s", events[i].stage.AsChar());
    }
}

Dia::ApplicationFlow::StopResult TestResultReporterModule::DoStop()
{
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace CluicheTest

namespace { using TestResultReporterModule_ = CluicheTest::TestResultReporterModule; }
DIA_MODULE(TestResultReporterModule_);
DIA_DESCRIBE(TestResultReporterModule_::kTypeId, "Reports TestResultsRegistry state changes to the StageCatalogue via the StageResult stream.");
