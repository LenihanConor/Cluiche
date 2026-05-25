#pragma once
#include <DiaEntity/Entity.h>

namespace Dia::Entity {
    class Domain;
}

namespace Dia::Entity::Hierarchy {

    // QueueSetParent — queues a parent-child relationship.
    // After EndOfFrame:
    //   - child has ParentComponent pointing to parent
    //   - parent has ChildBufferComponent containing child
    // If child already had a different parent, removes child from old parent's
    // ChildBufferComponent immediately (before EndOfFrame).
    // Debug: asserts if the relationship would create a cycle.
    // Both entities must be alive at call time.
    void QueueSetParent(Dia::Entity::Domain& domain, Dia::Entity::Entity child, Dia::Entity::Entity parent);

    // QueueDestroySubtree — queues QueueDestroy for root and all descendants depth-first.
    // Walks ChildBufferComponent chain; all entities in the subtree are destroyed at EndOfFrame.
    void QueueDestroySubtree(Dia::Entity::Domain& domain, Dia::Entity::Entity root);

} // namespace Dia::Entity::Hierarchy
