#include <DiaSimTime/DiaMainTime.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>

namespace Dia::SimTime {

const Dia::Core::StringCRC DiaMainTime::kTypeId("DiaMainTime");

DiaMainTime::DiaMainTime(const Dia::Core::StringCRC& instanceId)
    : Dia::ApplicationFlow::MainModule(instanceId)
{
}

Dia::ApplicationFlow::StartResult DiaMainTime::DoStart()
{
    return Dia::ApplicationFlow::StartResult::kReady;
}

void DiaMainTime::DoUpdate(const Dia::SimTime::MainTimeContext& /*ctx*/)
{
    // Nothing to do — ProcessingUnit::Update() already computes and caches
    // MainTimeContext directly for kMain/kAny affinity. This module exists for
    // manifest completeness and symmetry with DiaRenderTime.
}

Dia::ApplicationFlow::StopResult DiaMainTime::DoStop()
{
    return Dia::ApplicationFlow::StopResult::kDone;
}

} // namespace Dia::SimTime

namespace { using DiaMainTime_ = Dia::SimTime::DiaMainTime; }
DIA_MODULE(DiaMainTime_);
DIA_DESCRIBE(DiaMainTime_::kTypeId, "Trivial MainPU presence module; MainTimeContext is computed directly by ProcessingUnit.");
