#pragma once

#include <DiaObservation/Health/IHealthReporter.h>

namespace Dia { namespace Editor {

class InspectorHealthReporter : public Dia::Observation::Health::IHealthReporter
{
public:
    InspectorHealthReporter(const bool& isConnected, const bool& isActive);

    Dia::Core::StringCRC             GetReporterName() const override;
    Dia::Observation::Health::Health Report()          const override;

private:
    const bool& mIsConnected;
    const bool& mIsActive;
};

}} // namespace Dia::Editor
