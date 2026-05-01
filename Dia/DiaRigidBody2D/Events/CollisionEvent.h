#pragma once

#include "DiaRigidBody2D/Bodies/Body2DBase.h"

namespace Dia::RigidBody2D {

enum class CollisionEventType { kEnter, kStay, kExit };

struct CollisionEvent {
    CollisionEventType  type  = CollisionEventType::kEnter;
    const Body2DBase*   bodyA = nullptr;
    const Body2DBase*   bodyB = nullptr;
};

} // namespace Dia::RigidBody2D
