#include <DiaEntitySpawner/Health/EntitySpawnerHealth.h>
#include <DiaEntitySpawner/EntitySpawnerModule.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::EntitySpawner {

EntitySpawnerHealth::EntitySpawnerHealth(const EntitySpawnerModule& module)
    : mModule(module)
{
}

Dia::Core::StringCRC EntitySpawnerHealth::GetReporterName() const
{
    return Dia::Core::StringCRC("entity-spawner-module");
}

Dia::Observation::Health::Health EntitySpawnerHealth::Report() const
{
    Dia::Observation::Health::Health h;
    h.errors   = 0;
    h.warnings = 0;
    if (!mModule.HasBlueprintLoader())
    {
        h.status   = Dia::Observation::Health::HealthStatus::kDegraded;
        h.reason   = Dia::Core::StringCRC("IBlueprintLoader unavailable");
        h.warnings = 1;
    }
    else
    {
        h.status = Dia::Observation::Health::HealthStatus::kOK;
        h.reason = Dia::Core::StringCRC("");
    }
    return h;
}

} // namespace Dia::EntitySpawner
