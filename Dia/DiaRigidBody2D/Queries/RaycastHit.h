#pragma once

#include "DiaRigidBody2D/Bodies/Body2DBase.h"
#include "DiaMaths/Vector/Vector2D.h"

namespace Dia::RigidBody2D {

struct RaycastHit {
    const Body2DBase*    body     = nullptr;
    Dia::Maths::Vector2D point;
    Dia::Maths::Vector2D normal;
    float                distance = 0.0f;
};

} // namespace Dia::RigidBody2D
