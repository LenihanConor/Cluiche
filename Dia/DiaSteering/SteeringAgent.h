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

            // Debug-hint properties: zero means "not configured / skip drawing".
            float separationRadius   = 0.0f;  // used by SeparationRadius visual debugger drawer
            float detectionBoxLength = 0.0f;  // used by DetectionBox visual debugger drawer
        };
    }
}
