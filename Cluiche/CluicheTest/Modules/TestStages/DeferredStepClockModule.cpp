#include "Modules/TestStages/DeferredStepClockModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>

namespace CluicheTest {

const Dia::Core::StringCRC DeferredStepClockModule::kTypeId("DeferredStepClockModule");

DeferredStepClockModule::DeferredStepClockModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{}

Dia::ApplicationFlow::StartResult DeferredStepClockModule::DoStart()
{
    mKey = 0;   // restart per stage entry; the scheduler's key floors are discarded with the scheduler
    return Dia::ApplicationFlow::StartResult::kReady;
}

void DeferredStepClockModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
{
    ++mKey;
}

Dia::ApplicationFlow::StopResult DeferredStepClockModule::DoStop()
{
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace CluicheTest

namespace { using DeferredStepClockModule_ = CluicheTest::DeferredStepClockModule; }
DIA_MODULE(DeferredStepClockModule_);
