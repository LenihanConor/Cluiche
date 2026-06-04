#include <diaentitytemplate/Hierarchy/Hierarchy.h>
#include <diaentitytemplate/Hierarchy/ParentComponent.h>
#include <diaentitytemplate/Hierarchy/ChildBufferComponent.h>
#include <diaentitytemplate/Domain.h>
#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia::Entity::Hierarchy {

void QueueSetParent(Dia::Entity::Domain& domain, Dia::Entity::Entity child, Dia::Entity::Entity parent) {
    DIA_ASSERT(domain.IsAlive(child),  "QueueSetParent: child entity is not alive");
    DIA_ASSERT(domain.IsAlive(parent), "QueueSetParent: parent entity is not alive");

    if (!domain.IsAlive(child) || !domain.IsAlive(parent)) {
        return;
    }

#ifdef DEBUG
    // Cycle detection: walk the ParentComponent chain from parent upward.
    // If we encounter child, this would create a cycle.
    {
        Entity cursor = parent;
        while (cursor.IsValid() && domain.IsAlive(cursor)) {
            DIA_ASSERT(cursor != child,
                "QueueSetParent: cycle detected — child is an ancestor of parent");
            if (cursor == child) {
                return; // abort in non-assert builds
            }
            const ParentComponent* pc = domain.GetComponent<ParentComponent>(cursor);
            if (pc == nullptr) {
                break;
            }
            cursor = pc->GetParentEntity();
        }
    }
#endif

    // If child already has a ParentComponent, update it directly (immediate mutation) to
    // avoid the add-before-remove ordering problem in ApplyMutations.
    if (domain.HasComponent<ParentComponent>(child)) {
        ParentComponent* existingPc = domain.GetComponent<ParentComponent>(child);
        if (existingPc != nullptr) {
            // Remove child from old parent's ChildBufferComponent.
            Entity oldParent = existingPc->GetParentEntity();
            if (domain.IsAlive(oldParent)) {
                ChildBufferComponent* oldCb = domain.GetComponent<ChildBufferComponent>(oldParent);
                if (oldCb != nullptr) {
                    oldCb->RemoveChild(child);
                }
            }
            // Update ParentComponent fields in place.
            existingPc->parentIndex = parent.GetIndex();
            existingPc->parentGen   = parent.GetGeneration();
        }

        // Ensure new parent has a ChildBufferComponent.
        if (!domain.HasComponent<ChildBufferComponent>(parent)) {
            // Queue ChildBufferComponent. Its OnAttach will scan for entities whose
            // ParentComponent points to `parent` (including `child`, which was just updated).
            domain.QueueAddComponent<ChildBufferComponent>(parent, Json::Value());
        } else {
            // ChildBufferComponent is already live — add child directly.
            ChildBufferComponent* newCb = domain.GetComponent<ChildBufferComponent>(parent);
            if (newCb != nullptr) {
                newCb->AddChild(child);
            }
        }
        return;
    }

    // Queue ChildBufferComponent on parent FIRST so it is live when ParentComponent::OnAttach fires.
    if (!domain.HasComponent<ChildBufferComponent>(parent)) {
        domain.QueueAddComponent<ChildBufferComponent>(parent, Json::Value());
    }

    // Build config for ParentComponent.
    Json::Value cfg;
    cfg["parentIndex"] = static_cast<Json::UInt>(parent.GetIndex());
    cfg["parentGen"]   = static_cast<Json::UInt>(parent.GetGeneration());

    domain.QueueAddComponent<ParentComponent>(child, cfg);
}

void QueueDestroySubtree(Dia::Entity::Domain& domain, Dia::Entity::Entity root) {
    if (!domain.IsAlive(root)) {
        return;
    }

    // Iterative depth-first traversal using a fixed-size stack.
    // kMaxEntitiesPerDomain is 1024 — safe upper bound for traversal stack.
    Dia::Core::Containers::DynamicArrayC<Entity, kMaxEntitiesPerDomain> stack;
    stack.Add(root);

    while (!stack.IsEmpty()) {
        Entity current = stack.Back();
        stack.Remove(); // removes last element

        if (!domain.IsAlive(current)) {
            continue;
        }

        // Push children onto the stack before queuing destroy so we traverse them too.
        const ChildBufferComponent* cb = domain.GetComponent<ChildBufferComponent>(current);
        if (cb != nullptr) {
            for (uint32_t i = 0; i < cb->children.Size(); ++i) {
                if (domain.IsAlive(cb->children[i])) {
                    stack.Add(cb->children[i]);
                }
            }
        }

        domain.QueueDestroy(current);
    }
}

} // namespace Dia::Entity::Hierarchy
