#include "ObservationModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>

namespace Cluiche { namespace Editor {

ObservationModule::ObservationModule(const Dia::Core::StringCRC& instanceId)
    : Dia::ApplicationFlow::ObservationModule(
        instanceId,
        "CluicheEditor",
        "../../../../Assets/CluicheEditor/cluicheeditor.diaobservation")
{
}

const Dia::Core::StringCRC ObservationModule::kTypeId("ObservationModule");

} } // namespace Cluiche::Editor

namespace { using ObservationModule_ = Cluiche::Editor::ObservationModule; }
DIA_MODULE(ObservationModule_);
DIA_DESCRIBE(ObservationModule_::kTypeId, "Initializes editor observation sinks: logging, metrics, and health reporters for editor diagnostics.");
