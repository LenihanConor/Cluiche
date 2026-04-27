#pragma once

#include "DiaRigidBody2D/Bodies/PointBody2D.h"
#include "DiaRigidBody2D/Bodies/RigidBody2D.h"
#include "DiaRigidBody2D/Detection/Contact.h"
#include "DiaRigidBody2D/World/PhysicsWorldCapacities.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaGeometry2D/Spatial/ISpatialStructure.h"

namespace Dia::RigidBody2D {

// Common layer bit constants. Games define additional layers in their own headers
// using free bits (1u << 5 and above). The uint32_t bitmask supports up to 32 layers.
namespace Layers {
    constexpr uint32_t kNone       = 0;
    constexpr uint32_t kDefault    = 1u << 0;
    constexpr uint32_t kPlayer     = 1u << 1;
    constexpr uint32_t kEnemy      = 1u << 2;
    constexpr uint32_t kProjectile = 1u << 3;
    constexpr uint32_t kTrigger    = 1u << 4;
    constexpr uint32_t kAll        = 0xFFFFFFFF;
} // namespace Layers

bool ShouldCollide(const Body2DBase& a, const Body2DBase& b);

void DetectCollisions(
    Dia::Core::Containers::DynamicArrayC<PointBody2D*, kMaxPointBodies>& pointBodies,
    Dia::Core::Containers::DynamicArrayC<RigidBody2D*, kMaxRigidBodies>& rigidBodies,
    Dia::Geometry2D::ISpatialStructure<Body2DBase*>*                      broadPhase,
    Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts>&          outContacts);

} // namespace Dia::RigidBody2D
