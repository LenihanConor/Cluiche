#pragma once

#include "DiaRigidBody2D/Bodies/PointBody2D.h"
#include "DiaRigidBody2D/Bodies/RigidBody2D.h"
#include "DiaRigidBody2D/World/PhysicsWorldCapacities.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia::RigidBody2D {

void UpdateSleepTimers(
    Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies>& bodies,
    float dt, float linearThreshold, float sleepTimeThreshold);

void UpdateSleepTimers(
    Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies>& bodies,
    float dt, float linearThreshold, float angularThreshold, float sleepTimeThreshold);

} // namespace Dia::RigidBody2D
