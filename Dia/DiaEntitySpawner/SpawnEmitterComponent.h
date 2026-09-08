#pragma once
#ifndef DIA_ENTITYSPAWNER_SPAWNEMITTERCOMPONENT_H
#define DIA_ENTITYSPAWNER_SPAWNEMITTERCOMPONENT_H

#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::EntitySpawner {

class SpawnEmitterComponent : public Dia::Entity::IComponent {
    DIA_COMPONENT(SpawnEmitterComponent, "spawn-emitter-component", 1)
    DIA_READONLY

    FIELD(Dia::Core::StringCRC, blueprintId,    Dia::Core::StringCRC())
    FIELD(float,                rate,           0.0f)
    FIELD(int,                  burstCount,     0)
    FIELD(int,                  cap,            0)
    FIELD(float,                lifetime,       0.0f)
    FIELD(float,                despawnRadius,  0.0f)
    FIELD(bool,                 active,         false)

public:
};

} // namespace Dia::EntitySpawner

#endif // DIA_ENTITYSPAWNER_SPAWNEMITTERCOMPONENT_H
