#pragma once

#include "DiaRigidBody2D/Bodies/Body2DBase.h"
#include "DiaMaths/Vector/Vector2D.h"

namespace Dia::RigidBody2D {

struct Contact {
    Body2DBase* bodyA = nullptr;
    Body2DBase* bodyB = nullptr;

    Dia::Maths::Vector2D normal;     // From A toward B
    float                depth  = 0.0f;
    Dia::Maths::Vector2D point;      // World-space contact point
};

} // namespace Dia::RigidBody2D
