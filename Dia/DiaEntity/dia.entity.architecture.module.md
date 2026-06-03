---
module: dia.entity
version: 1
layer: assets/core
namespace: Dia::Entity
project: Dia/DiaEntity/DiaEntity.vcxproj
dependent_modules:
  - dia.core
  - dia.maths
  - dia.mailbox
public_headers:
  - Dia/DiaEntity/Domain.h
  - Dia/DiaEntity/Entity.h
  - Dia/DiaEntity/IComponent.h
  - Dia/DiaEntity/ComponentTypeDesc.h
  - Dia/DiaEntity/ComponentRegistry.h
  - Dia/DiaEntity/ComponentMacros.h
  - Dia/DiaEntity/EntityRef.h
  - Dia/DiaEntity/EntityAddress.h
  - Dia/DiaEntity/EntityRouter.h
  - Dia/DiaEntity/IBlueprintLoader.h
  - Dia/DiaEntity/JsonBlueprintLoader.h
  - Dia/DiaEntity/IEntityInspectable.h
  - Dia/DiaEntity/QueryView.h
  - Dia/DiaEntity/Hierarchy/ParentComponent.h
  - Dia/DiaEntity/Hierarchy/ChildBufferComponent.h
  - Dia/DiaEntity/Hierarchy/Hierarchy.h
  - Dia/DiaEntity/Messages/EntityDestroyedMessage.h
responsibilities:
  - Domain (entity container), Entity (generational handle), IComponent abstract base
  - Per-type component pools via HandlePool<T>
  - End-of-frame structural mutation pipeline
  - DIA_COMPONENT + FIELD + DIA_UPDATABLE macro reflection system
  - ComponentRegistry process-global type lookup
  - JsonBlueprintLoader three-pass entity graph instantiation
  - EntityRef<T> typed cross-entity reference slots
  - Parent/child hierarchy via opt-in components
  - EntityRouter IMailboxRouter implementation for Dia::Mailbox
  - Signature-keyed query cache system
  - IEntityInspectable reflection-driven editor inspection
  - Domain::Update(dt) per-frame component tick
non_responsibilities:
  - Application lifecycle and stage management (lives in application code)
  - System-side data (physics bodies, render objects, skeletons)
  - Cross-realm references or shared state
  - Network replication
  - General engine-wide reflection (that is DiaReflect)
---

`dia.entity` is the gameplay-level entity system for the Dia engine. It provides a `Domain` container that holds generational `Entity` handles and per-type component pools, an end-of-frame structural mutation pipeline to prevent mid-frame invalidation, a macro-based reflection system (`DIA_COMPONENT`, `FIELD`, `DIA_UPDATABLE`) for component registration and inspection, and a `JsonBlueprintLoader` for three-pass entity graph instantiation from JSON blueprint files. The module also implements `EntityRouter` as a `Dia::Mailbox::IMailboxRouter` to route mailbox messages by entity address, a signature-keyed query cache system for efficient component iteration, and `IEntityInspectable` for reflection-driven editor inspection.
