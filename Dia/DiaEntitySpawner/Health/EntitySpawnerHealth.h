#pragma once
#ifndef DIA_ENTITYSPAWNER_HEALTH_ENTITYSPAWNERHEALTH_H
#define DIA_ENTITYSPAWNER_HEALTH_ENTITYSPAWNERHEALTH_H

#include <DiaObservation/Health/IHealthReporter.h>

namespace Dia::EntitySpawner {
    class EntitySpawnerModule;
}

namespace Dia::EntitySpawner {

class EntitySpawnerHealth : public Dia::Observation::Health::IHealthReporter
{
public:
    explicit EntitySpawnerHealth(const EntitySpawnerModule& module);
    Dia::Core::StringCRC GetReporterName() const override;
    Dia::Observation::Health::Health Report() const override;
private:
    const EntitySpawnerModule& mModule;
};

} // namespace Dia::EntitySpawner

#endif // DIA_ENTITYSPAWNER_HEALTH_ENTITYSPAWNERHEALTH_H
