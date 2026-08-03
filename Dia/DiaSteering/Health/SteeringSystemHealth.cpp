#include <DiaSteering/Health/SteeringSystemHealth.h>
#include <DiaSteering/SteeringSystem.h>

namespace Dia { namespace Steering {

    SteeringSystemHealth::SteeringSystemHealth(const SteeringSystem& system)
        : mSystem(system)
    {
    }

    Dia::Core::StringCRC SteeringSystemHealth::GetReporterName() const
    {
        return Dia::Core::StringCRC("dia.steering.system");
    }

    Dia::Observation::Health::Health SteeringSystemHealth::Report() const
    {
        Dia::Observation::Health::Health h;
        h.errors   = 0;
        h.warnings = 0;

        if (mSystem.GetAgentCount() == 0)
        {
            h.status  = Dia::Observation::Health::HealthStatus::kDegraded;
            h.reason  = Dia::Core::StringCRC("no agents registered");
            h.warnings = 1;
        }
        else
        {
            h.status = Dia::Observation::Health::HealthStatus::kOK;
            h.reason = Dia::Core::StringCRC("");
        }

        return h;
    }

} }
