#pragma once

#include <DiaObservation/Health/IHealthReporter.h>

namespace Dia { namespace Editor {

class InspectorHealthReporter : public Dia::Observation::Health::IHealthReporter
{
public:
    static constexpr float kStaleThresholdSec = 5.0f;

    InspectorHealthReporter(const bool& isConnected, const float& secondsSinceLastData);

    Dia::Core::StringCRC             GetReporterName() const override;
    Dia::Observation::Health::Health Report()          const override;

private:
    const bool&  mIsConnected;
    const float& mSecondsSinceLastData;
};

}} // namespace Dia::Editor
