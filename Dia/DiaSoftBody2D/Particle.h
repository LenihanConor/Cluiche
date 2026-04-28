#pragma once

#include "DiaMaths/Vector/Vector2D.h"

namespace Dia::SoftBody2D {

struct Particle {
    Dia::Maths::Vector2D position;
    Dia::Maths::Vector2D prevPosition;
    float                invMass;
    float                radius;
};

inline Dia::Maths::Vector2D DeriveVelocity(const Particle& p, float dt)
{
    return (p.position - p.prevPosition) * (1.0f / dt);
}

} // namespace Dia::SoftBody2D
