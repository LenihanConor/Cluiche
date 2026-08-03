#pragma once

#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
    namespace Steering
    {
        // Minimal agent state consumed by all steering behaviour free functions.
        // Behaviours read position and velocity to compute desired velocity outputs.
        struct SteeringAgent
        {
            Dia::Maths::Vector2D position;
            Dia::Maths::Vector2D velocity;
            float maxSpeed = 1.0f;
            float maxForce = 1.0f;
        };
    }
}
