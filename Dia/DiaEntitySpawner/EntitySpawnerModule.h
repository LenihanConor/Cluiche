#pragma once
#ifndef DIA_ENTITYSPAWNER_ENTITYSPAWNERMODULE_H
#define DIA_ENTITYSPAWNER_ENTITYSPAWNERMODULE_H

#include <DiaApplicationFlow/Module.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::Entity { class Domain; }

namespace Dia::EntitySpawner {

class EntitySpawnerModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kInstanceId;
    explicit EntitySpawnerModule(Dia::Entity::Domain& domain);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void                              DoUpdate(float deltaTime) override;
    Dia::ApplicationFlow::StopResult  DoStop() override;

private:
    Dia::Entity::Domain& mDomain;
};

} // namespace Dia::EntitySpawner

#endif // DIA_ENTITYSPAWNER_ENTITYSPAWNERMODULE_H
