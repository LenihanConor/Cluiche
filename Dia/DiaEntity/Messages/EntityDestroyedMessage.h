#pragma once
#include <diaentitytemplate/Entity.h>

namespace Dia::Entity {

    // EntityDestroyedMessage — broadcast to MakeAllAddress() during ApplyDestroyEntity,
    // before the entity slot is freed. Subscribers can inspect the entity one last time
    // via domain before the message drain.
    struct EntityDestroyedMessage {
        Entity destroyed;
    };

} // namespace Dia::Entity
