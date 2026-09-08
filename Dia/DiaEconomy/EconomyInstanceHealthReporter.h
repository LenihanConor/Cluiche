#pragma once

#include <DiaObservation/Health/HealthReporterBase.h>
#include <DiaObservation/Health/HealthRegistry.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Economy {

class EconomyInstance;

// Monitors an EconomyInstance for pool coherence (min <= value <= max on every
// resource).  Attach to HealthRegistry on module start; call Check() each tick.
class EconomyInstanceHealthReporter : public Dia::Observation::Health::HealthReporterBase
{
public:
    explicit EconomyInstanceHealthReporter(Dia::Core::StringCRC reporterName,
                                           const EconomyInstance& instance);
    ~EconomyInstanceHealthReporter() override = default;

    Dia::Core::StringCRC GetReporterName() const override;

    // Call once per sim tick (or on-demand) to re-evaluate pool invariants.
    void Check();

private:
    Dia::Core::StringCRC   mName;
    const EconomyInstance& mInstance;
};

}} // namespace Dia::Economy
