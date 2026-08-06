#pragma once
#ifndef DIA_ENTITYSPAWNER_ENTITYSPAWNERIMPL_H
#define DIA_ENTITYSPAWNER_ENTITYSPAWNERIMPL_H

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaEntity/Entity.h>

namespace Dia::EntitySpawner {

static constexpr unsigned int kMaxChildrenPerEmitter = 128;

struct EmitterState {
    float tokenAccumulator = 0.0f;
    Dia::Core::Containers::DynamicArrayC<Dia::Entity::Entity, kMaxChildrenPerEmitter> children;
    bool burstFired = false;
};

} // namespace Dia::EntitySpawner

#endif // DIA_ENTITYSPAWNER_ENTITYSPAWNERIMPL_H
