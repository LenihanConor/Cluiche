#pragma once
#include <DiaApplicationFlow/Observation/ObservationModule.h>

namespace Cluiche { namespace AppFlow {

class ObservationModule : public Dia::ApplicationFlow::ObservationModule {
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kAny;
    static constexpr const char* kDescription = "Starts DiaObservation session; available to all PUs";

    explicit ObservationModule(const Dia::Core::StringCRC& instanceId);

protected:
    void ApplyConfigOverrides(Dia::Observation::ObservationConfig& config) override;
};

} } // namespace Cluiche::AppFlow
