---
module: dia.entity
version: 1
layer: assets/core
namespace: Dia::Entity
path: Dia/DiaEntity
project: Dia/diaentitytemplate/diaentitytemplate.vcxproj
dependent_modules:
  - dia.core
  - dia.maths
  - dia.mailbox
public_headers:
  - Dia/diaentitytemplate/Domain.h
  - Dia/diaentitytemplate/Entity.h
  - Dia/diaentitytemplate/IComponent.h
  - Dia/diaentitytemplate/ComponentTypeDesc.h
  - Dia/diaentitytemplate/ComponentRegistry.h
  - Dia/diaentitytemplate/ComponentMacros.h
  - Dia/diaentitytemplate/EntityRef.h
  - Dia/diaentitytemplate/EntityAddress.h
  - Dia/diaentitytemplate/EntityRouter.h
  - Dia/diaentitytemplate/IBlueprintLoader.h
  - Dia/diaentitytemplate/JsonBlueprintLoader.h
  - Dia/diaentitytemplate/IEntityInspectable.h
  - Dia/diaentitytemplate/QueryView.h
  - Dia/diaentitytemplate/Hierarchy/ParentComponent.h
  - Dia/diaentitytemplate/Hierarchy/ChildBufferComponent.h
  - Dia/diaentitytemplate/Hierarchy/Hierarchy.h
  - Dia/diaentitytemplate/Messages/EntityDestroyedMessage.h
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
