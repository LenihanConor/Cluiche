#pragma once

#include "DiaRigidBody2D/Detection/Contact.h"
#include "DiaRigidBody2D/World/PhysicsWorldCapacities.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

namespace Dia::RigidBody2D {

struct ResponseConfig {
    float baumgarteSlop           = 0.01f;  // Penetration below which no positional correction
    float baumgarteFactor         = 0.2f;   // Fraction of penetration corrected per step [0,1]
    float restitutionVelocitySlop = 0.5f;   // Relative velocity below which restitution = 0
};

void ResolveCollisions(
    const Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts>& contacts,
    const ResponseConfig&                                               config,
    float                                                               dt);

} // namespace Dia::RigidBody2D
