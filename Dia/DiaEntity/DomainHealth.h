#pragma once
#include <DiaObservation/Health/HealthReporterBase.h>

namespace Dia::Entity {

    class DomainHealth : public Dia::Observation::Health::HealthReporterBase {
    public:
        DomainHealth() = default;
        Dia::Core::StringCRC GetReporterName() const override {
            return Dia::Core::StringCRC("dia.entity.domain");
        }
    };

} // namespace Dia::Entity
