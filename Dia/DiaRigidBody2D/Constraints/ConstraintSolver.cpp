#include "DiaRigidBody2D/Constraints/ConstraintSolver.h"

namespace Dia::RigidBody2D {

void SolveConstraints(
    Dia::Core::Containers::DynamicArrayC<IConstraint*, kMaxConstraints>& constraints,
    const ConstraintSolverConfig& config,
    float dt)
{
    const unsigned int count = constraints.Size();
    if (count == 0) return;

    for (unsigned int i = 0; i < count; ++i)
        if (constraints[i]) constraints[i]->PreStep(dt);

    for (int iter = 0; iter < config.iterations; ++iter)
        for (unsigned int i = 0; i < count; ++i)
            if (constraints[i]) constraints[i]->ApplyImpulse();
}

} // namespace Dia::RigidBody2D
