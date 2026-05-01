#pragma once

#include "DiaRigidBody2D/Bodies/PointBody2D.h"
#include "DiaRigidBody2D/Bodies/RigidBody2D.h"
#include "DiaRigidBody2D/World/PhysicsWorldCapacities.h"
#include "DiaMaths/Vector/Vector2D.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia::RigidBody2D {

void IntegrateLinearForces(Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies>& bodies,
                           const Dia::Maths::Vector2D& gravity, float dt);
void IntegrateLinearForces(Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies>& bodies,
                           const Dia::Maths::Vector2D& gravity, float dt);

void IntegrateAngularForces(Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies>& bodies, float dt);

void IntegrateLinearVelocities(Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies>& bodies, float dt);
void IntegrateLinearVelocities(Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies>& bodies, float dt);

void IntegrateAngularVelocities(Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies>& bodies, float dt);

void ClearForceAccumulators(Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies>& bodies);
void ClearForceAccumulators(Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies>& bodies);

} // namespace Dia::RigidBody2D
