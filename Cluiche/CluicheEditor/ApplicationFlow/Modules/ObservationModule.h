#pragma once
#include <DiaApplicationFlow/Observation/ObservationModule.h>

namespace Cluiche { namespace Editor {

class ObservationModule : public Dia::ApplicationFlow::ObservationModule {
public:
    static const Dia::Core::StringCRC kTypeId;

    explicit ObservationModule(const Dia::Core::StringCRC& instanceId);
};

} } // namespace Cluiche::Editor
