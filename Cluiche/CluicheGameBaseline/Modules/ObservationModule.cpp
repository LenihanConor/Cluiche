#include "Modules/ObservationModule.h"

#include <DiaObservation/Config/ObservationConfigCli.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>

#include <cstdlib>

namespace Cluiche { namespace AppFlow {

ObservationModule::ObservationModule(const Dia::Core::StringCRC& instanceId)
    : Dia::ApplicationFlow::ObservationModule(
        instanceId,
        "CluicheTest",
        "../../../../Assets/CluicheTest/cluichetest.diaobservation")
{
}

void ObservationModule::ApplyConfigOverrides(Dia::Observation::ObservationConfig& config)
{
    Dia::Observation::ObservationConfigCli::ApplyOverrides(__argc, (const char* const*)__argv, config);
}

const Dia::Core::StringCRC ObservationModule::kTypeId("ObservationModule");

} } // namespace Cluiche::AppFlow

namespace { using ObservationModule_ = Cluiche::AppFlow::ObservationModule; }
DIA_MODULE(ObservationModule_);
DIA_DESCRIBE(ObservationModule_::kTypeId, "Initializes the observation system: logging sinks, metrics, traces, and health reporters.");
