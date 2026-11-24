#pragma once
#include <DiaApplicationFlow/Observation/ObservationModuleBase.h>

namespace Cluiche { namespace Editor {

class ObservationModule : public Dia::ApplicationFlow::ObservationModuleBase {
public:
    static const Dia::Core::StringCRC kTypeId;

    explicit ObservationModule(const Dia::Core::StringCRC& instanceId);
};

} } // namespace Cluiche::Editor
