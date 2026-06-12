#pragma once

#include "DiaRigidBody2D/Detection/Contact.h"
#include "DiaRigidBody2D/World/PhysicsWorldCapacities.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"
#include "DiaMaths/Vector/Vector2D.h"

namespace Dia::RigidBody2D {

struct ResponseConfig {
    float baumgarteSlop           = 0.01f;  // Penetration below which no positional correction
    float baumgarteFactor         = 0.2f;   // Fraction of penetration corrected per step [0,1]
    float restitutionVelocitySlop = 0.5f;   // Relative velocity below which restitution = 0
};

// Resolves contacts with impulse-based response.
//
// `gravity` is the world gravity for this step. It is used to bias the
// restitution-suppression threshold: a resting contact gains an apparent
// approach velocity of |gravity . n| * dt from this step's force integration
// alone. Without accounting for that, restitution re-launches resting bodies
// every step and they never settle (sleep). The effective threshold becomes
// restitutionVelocitySlop + |gravity . n| * dt, so genuine impacts still bounce
// while resting contacts are treated as inelastic. Defaults to zero gravity for
// isolated impulse tests that exercise restitution directly.
void ResolveCollisions(
    const Dia::Core::Containers::DynamicArrayC<Contact, kMaxContacts>& contacts,
    const ResponseConfig&                                               config,
    float                                                               dt,
    const Dia::Maths::Vector2D&                                         gravity = Dia::Maths::Vector2D(0.0f, 0.0f));

} // namespace Dia::RigidBody2D
