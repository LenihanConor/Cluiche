#include "Modules/JobSystemPlaceholderConsumerModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CoW {

const Dia::Core::StringCRC JobSystemPlaceholderConsumerModule::kTypeId("JobSystemPlaceholderConsumerModule");

JobSystemPlaceholderConsumerModule::JobSystemPlaceholderConsumerModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{
}

void JobSystemPlaceholderConsumerModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mJobSystemReader.Connect(app);
}

Dia::ApplicationFlow::StartResult JobSystemPlaceholderConsumerModule::DoStart()
{
    DIA_LOG_INFO("CoW", "JobSystemPlaceholderConsumerModule: started (placeholder, replace with A4's consumer)");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void JobSystemPlaceholderConsumerModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
{
}

Dia::ApplicationFlow::StopResult JobSystemPlaceholderConsumerModule::DoStop()
{
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace CoW

namespace { using JobSystemPlaceholderConsumerModule_ = CoW::JobSystemPlaceholderConsumerModule; }
DIA_MODULE(JobSystemPlaceholderConsumerModule_);
DIA_DESCRIBE(JobSystemPlaceholderConsumerModule_::kTypeId, "Placeholder consumer so JobSystemService is not an orphan provider until A4 lands.");
