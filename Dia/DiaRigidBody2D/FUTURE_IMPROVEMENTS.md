# DiaRigidBody2D — Future Improvements

## Unified Contact/Constraint Solver

**Problem:** Constraints are solved after collision response in a separate pass.
Collision impulses and constraint impulses can fight each other, reducing
convergence for stacking or joint-heavy scenes.

**Fix:** Merge contacts into the constraint solver loop. Treat each contact as
an implicit constraint with its own PreStep/ApplyImpulse, then iterate all
constraints and contacts together for N iterations. This is how Box2D and
Chipmunk handle it.

**Files affected:** `PhysicsWorld::StepOnce`, `ResolveCollisions.cpp`,
`ConstraintSolver.cpp`.

## Persistent Contact Manifold

**Problem:** Contacts are fully recreated each frame. This prevents warm-starting
collision impulses (accumulated lambda carry-over) and causes rolling/sliding
friction to lose frame-to-frame coherence.

**Fix:** Maintain a contact cache keyed by body pair. On each frame, match new
contacts to cached contacts by feature ID or nearest-point heuristic. Carry
over the accumulated normal and friction impulses for warm-starting. Expire
unmatched entries after one frame.

**Files affected:** `DetectCollisions.cpp`, `Contact.h` (add contact ID/feature),
`PhysicsWorld` (store persistent manifold table).
