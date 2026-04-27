#pragma once

#include "DiaRigidBody2D/Constraints/IConstraint.h"
#include "DiaRigidBody2D/World/PhysicsWorldCapacities.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia::RigidBody2D {

struct ConstraintSolverConfig {
    int iterations = 10;
};

void SolveConstraints(
    Dia::Core::Containers::DynamicArrayC<IConstraint*, kMaxConstraints>& constraints,
    const ConstraintSolverConfig& config,
    float dt);

} // namespace Dia::RigidBody2D
