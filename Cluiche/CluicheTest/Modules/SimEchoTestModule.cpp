#include "Modules/SimEchoTestModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaCore/Json/external/json/json.h>

namespace CluicheTest {

const Dia::Core::StringCRC SimEchoTestModule::kTypeId("SimEchoTestModule");

SimEchoTestModule::SimEchoTestModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{}

Dia::ApplicationFlow::StartResult SimEchoTestModule::DoStart()
{
    Cluiche::AppFlow::SimQueryHostModule* host = mHost.Get();
    if (host == nullptr)
        return Dia::ApplicationFlow::StartResult::kLoading;

    if (!mRegistered)
    {
        mRegistered = host->RegisterQuery("sim.echo", HelpInfo(), [](const Json::Value& args)
        {
            Json::Value result(Json::objectValue);
            result["echo"]   = args;
            result["thread"] = "sim";
            return result;
        });
    }
    return Dia::ApplicationFlow::StartResult::kReady;
}

void SimEchoTestModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
{
}

Dia::ApplicationFlow::StopResult SimEchoTestModule::DoStop()
{
    if (mRegistered)
    {
        if (Cluiche::AppFlow::SimQueryHostModule* host = mHost.Get())
            host->UnregisterQuery("sim.echo");
        mRegistered = false;
    }
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace CluicheTest

namespace { using SimEchoTestModule_ = CluicheTest::SimEchoTestModule; }
DIA_MODULE(SimEchoTestModule_);
DIA_DESCRIBE(SimEchoTestModule_::kTypeId, "Registers the sim.echo sim-hosted debug query used by the Sim-Hosted Queries E2E.");

#endif // DIA_DEBUG
