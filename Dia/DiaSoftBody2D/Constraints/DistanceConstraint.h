#pragma once

namespace Dia::SoftBody2D {

enum class ConstraintType { kRope, kStructural, kShear, kBend };

struct DistanceConstraint {
    int            indexA;
    int            indexB;
    float          restLength;
    float          stiffness;
    bool           active;
    ConstraintType type;
};

} // namespace Dia::SoftBody2D
