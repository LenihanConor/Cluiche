#pragma once

#include "DiaMaths/Vector/Vector2D.h"
#include "DiaGeometry2D/Spatial/ISpatialStructure.h"
#include "DiaRigidBody2D/Response/ResolveCollisions.h"
#include "DiaRigidBody2D/Constraints/ConstraintSolver.h"

namespace Dia::RigidBody2D {

class Body2DBase;

struct WorldDef {
    Dia::Maths::Vector2D gravity        = { 0.0f, -9.81f };
    float                fixedTimestep  = 1.0f / 60.0f;
    int                  maxSubSteps    = 8;
    ResponseConfig       responseConfig;
    ConstraintSolverConfig constraintConfig;
    float                sleepLinearThreshold  = 0.01f;
    float                sleepAngularThreshold = 0.01f;
    float                sleepTimeThreshold    = 0.5f;
    // Non-owning — caller manages lifetime; must outlive PhysicsWorld
    Dia::Geometry2D::ISpatialStructure<Body2DBase*>* broadPhase = nullptr;
};

} // namespace Dia::RigidBody2D
