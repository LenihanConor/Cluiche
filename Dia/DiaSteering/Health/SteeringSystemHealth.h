#pragma once
#include <DiaObservation/Health/IHealthReporter.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Steering { class SteeringSystem; } }

namespace Dia { namespace Steering {

    class SteeringSystemHealth : public Dia::Observation::Health::IHealthReporter
    {
    public:
        explicit SteeringSystemHealth(const SteeringSystem& system);

        Dia::Core::StringCRC GetReporterName() const override;
        Dia::Observation::Health::Health Report() const override;

    private:
        const SteeringSystem& mSystem;
    };

} }
