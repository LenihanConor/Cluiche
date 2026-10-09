#include "Modules/StageStatusModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC StageStatusModule::kTypeId("StageStatusModule");

StageStatusModule::StageStatusModule(const Dia::Core::StringCRC& instanceId)
    : RenderModule(instanceId)
{}

// RED-phase stubs: real behaviour arrives in the GREEN task.
bool StageStatusModule::Set(const Dia::Core::StringCRC&, const StageStatus&) { return false; }
const StageStatusModule::StageStatus* StageStatusModule::Get(const Dia::Core::StringCRC&) const { return nullptr; }

Dia::ApplicationFlow::StartResult StageStatusModule::DoStart() { return Dia::ApplicationFlow::StartResult::kReady; }
void StageStatusModule::DoUpdate(const Dia::SimTime::RenderTimeContext&) {}
Dia::ApplicationFlow::StopResult StageStatusModule::DoStop() { return Dia::ApplicationFlow::StopResult::kDone; }

} } // namespace Cluiche::AppFlow

namespace { using StageStatusModule_ = Cluiche::AppFlow::StageStatusModule; }
DIA_MODULE(StageStatusModule_);
DIA_DESCRIBE(StageStatusModule_::kTypeId, "Holds per-stage status (label, severity, visited) keyed by stage CRC.");
