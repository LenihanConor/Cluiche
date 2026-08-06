#pragma once
#ifndef DIA_ENTITYSPAWNER_SPAWNERTYPES_H
#define DIA_ENTITYSPAWNER_SPAWNERTYPES_H

#include <DiaCore/CRC/StringCRC.h>
#include <DiaEntity/Entity.h>
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia::Entity {

enum class SpawnError {
    None,
    BlueprintNotFound,
    DomainFull
};

enum class DespawnReason {
    Lifetime,
    Radius,
    Cap,
    Explicit
};

struct SpawnRequest {
    Dia::Core::StringCRC    blueprintId;
    Dia::Maths::Vector2D    position;
    Entity                  parent;
    Dia::Core::StringCRC    tag;
};

struct SpawnResult {
    Entity      entity;
    SpawnError  error = SpawnError::None;
};

class IEntitySpawner {
public:
    virtual ~IEntitySpawner() = default;

    virtual SpawnResult Spawn(const SpawnRequest& request) = 0;
    virtual void        Despawn(Entity entity) = 0;
};

struct EntitySpawnedEvent {
    Entity              entity;
    Dia::Core::StringCRC blueprintId;
    Dia::Core::StringCRC tag;
};

struct EntityDespawnedEvent {
    Entity          entity;
    DespawnReason   reason;
};

} // namespace Dia::Entity

#endif // DIA_ENTITYSPAWNER_SPAWNERTYPES_H
