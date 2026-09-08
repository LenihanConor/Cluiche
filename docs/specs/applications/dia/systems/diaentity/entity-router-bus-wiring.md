# Feature Spec: entity-router-bus-wiring

**System:** diaentitytemplate
**App:** Dia
**Status:** Approved

## Summary

`Domain` already owns a private `Dia::Mailbox::Mailbox` and auto-registers its `EntityRouter` against it in the constructor (see [mailbox-router.md](mailbox-router.md), Approved/Done). That registration only makes entities addressable *within* a single `Domain`'s own mailbox — nothing outside `Domain` can address an entity through DiaMessageBus's shared `Bus`, even though the system spec for DiaMessageBus already reserves `kEntityRouterId` for exactly that purpose ([entity-router-registration.md](../diamessagebus/entity-router-registration.md) built only the plug-in point, against a mock router).

This feature registers the same `EntityRouter` instance on the Bus's mailbox as well, so System→Entity and Entity→System gameplay messages (per DiaMessageBus SD-MBX2-003/system spec) actually resolve to real entity subscribers, not just a mock. Domain's private mailbox is untouched — it continues to serve entity-internal, ECS-level traffic (e.g. `EntityDestroyedMessage`) exactly as it does today. This feature only adds a second registration of the existing router.

## Traceability

| Level | Spec |
|---|---|
| Platform | [platform.md](../../../../platform/Cluiche.md) |
| Application | [dia.md](../../dia.md) |
| System | diaentitytemplate |
| Depends on feature | [mailbox-router.md](mailbox-router.md) (Approved — `EntityRouter` already exists) |
| Depends on system | [diamessagebus.md](../diamessagebus/diamessagebus.md) — requires `core-bus` (`Bus::RegisterRouter`, `kEntityRouterId`) and `entity-router-registration` (plug-in point) to be built first |

## Goals

- Any system holding a reference to the shared `Bus` can address a specific entity via `{Bus::kEntityRouterId, handle.bits}` and have it resolve against that entity's real component subscriptions — not a mock.
- Components can subscribe on the Bus for entity-addressed gameplay messages using a well-defined `SubscriberId` derived from their owning entity's handle.
- Domain's private mailbox and its existing entity-internal traffic (`EntityDestroyedMessage`) are unaffected — no dual-posting, no behavior change to anything that already works.

## Non-Goals

- Moving `EntityDestroyedMessage` (or any other Domain-internal message) onto the shared Bus. That traffic stays exactly where it is.
- Any change to `EntityRouter::Resolve` decode logic (Entity/All/ComponentType/Self) — it is reused as-is, unchanged, against a second `Mailbox`.
- Defining a general cross-mailbox bridging mechanism. This feature only registers one router object against two independently-owned mailboxes; it does not create any new infrastructure for that pattern.

## Acceptance Criteria

- At stage composition time, the same `EntityRouter` instance owned by `Domain` is registered on the `MessageBusModule`'s `Bus` via `Bus::RegisterRouter(&domain.GetEntityRouter())`, in addition to its existing registration on Domain's own `Mailbox`.
- `Domain` exposes a `GetEntityRouter()` accessor (mirrors the existing `GetMailbox()` accessor).
- A component can call `bus.Subscribe<T>(subscriberId, handler)` where `subscriberId` is derived from its owning entity's handle bits, and receive delivery when another system posts `bus.Post<T>({Bus::kEntityRouterId, handle.bits}, msg)`.
- A system posting to `{Bus::kEntityRouterId, handle.bits}` for an entity with no Bus-side subscribers is a silent no-op (matches existing `EntityRouter::Resolve` behavior for zero matches — see mailbox-router.md AI Review Q5).
- A component wanting to filter by component type (not just by entity) uses `MakeComponentTypeAddress` / the `ComponentType` address kind already defined in `EntityAddress.h` — no new per-component `SubscriberId` scheme is introduced.
- Existing `EntityDestroyedMessage` behavior on Domain's private mailbox is unchanged; no test covering it should need modification.
- Registration order: the composition-root wiring must register the router on the Bus only after both `Domain` and `MessageBusModule` exist and before any Bus traffic addressed to `kEntityRouterId` is expected to resolve (i.e., during stage `OnStart`, not `OnUpdate`).

## Design

### Where the registration call lives

Neither `MessageBusModule` nor `Domain`/`EntityModule` may own this call:

- DiaMessageBus's system spec explicitly excludes any dependency on diaentitytemplate ("DiaMessageBus depends on none of it").
- diaentitytemplate's dependency chain (per mailbox-router.md) only reaches DiaMailbox, not DiaMessageBus.

So the call belongs in the **stage composition root** — wherever a stage already constructs both its `MessageBusModule` and its entity `Domain`/owning module together. This is the same shape as how `PhysicsBusAdapter`/`InputBusAdapter` register themselves, except those adapters live inside a module that *does* declare a `DiaMessageBus` dependency; `Domain` deliberately does not, so a third party (the stage itself) makes the introduction:

```cpp
// Stage OnStart, after both mDomain and mMessageBusModule exist:
mMessageBusModule.GetBus().RegisterRouter(&mDomain.GetEntityRouter());
```

### SubscriberId convention for entity-addressed Bus subscriptions

`SubscriberId` is opaque to DiaMailbox (`uint64_t`). For Bus-side entity subscriptions, this feature defines the convention: `SubscriberId{ entityHandle.bits() }` — one subscriber identity per entity, matching the bits already carried in `Address::payload` for the `Entity` address kind. Component-level filtering (if a component only wants messages relevant to it, not every message addressed to its owning entity) is handled by the existing `ComponentType` address kind, not by inventing per-component subscriber identities.

### What does not change

- `EntityRouter::Resolve` — unchanged. It is registered against a second `Mailbox` (the Bus's), but its decode logic (`AddressKind` dispatch against `mDomain`) is identical either way.
- Domain's private `Mailbox` and `EntityDestroyedMessage` — unchanged. This feature does not touch teardown-notification plumbing. (Whether `EntityDestroyedMessage` should *also* reach the Bus is an open question, explicitly deferred — see Open Questions.)

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaEntity/Domain.h` | Add `EntityRouter& GetEntityRouter()` accessor |
| Stage composition root (CluicheTest stage `OnStart`, or a shared stage-boot helper if one exists) | Add `bus.RegisterRouter(&domain.GetEntityRouter())` call |
| `Tests/GoogleTests/Entity/EntityRouterBusWiringTests.cpp` | New — integration test: subscribe a stub via `SubscriberId{handle.bits}` on the Bus, post to `{kEntityRouterId, handle.bits}` from a second "system", assert delivery |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | StringCRC for IDs | `kEntityRouterId` already `StringCRC` (unchanged from mailbox-router.md); no new string-keyed identity introduced. |
| PD-004 | No STL in public APIs | `GetEntityRouter()` returns a reference to an existing type; no new containers. |
| SD-ENT-018 | Single-threaded per domain | `EntityRouter::Resolve` is still called synchronously from whichever thread drives the Bus's flush (sim thread) — same constraint as Domain's own mailbox. |
| SD-MBX2-003 | Sender identity irrelevant, only Address determines receivers | This feature is what makes System→Entity and Entity→System actually resolve against real subscribers instead of a mock. |

## Open Design Questions

1. **Should `EntityDestroyedMessage` also post to the shared Bus?** Raised during design review — teardown notification is arguably something other systems want to react to, not just same-Domain components. Explicitly out of scope for this feature (ordering-sensitive, fires before the entity slot is freed, inside Domain's own update loop). Revisit as its own feature if a real cross-system need for "entity died" notifications shows up.
2. **Is there a generic stage-boot wiring point, or does each stage hand-wire this call?** If CluicheTest's `TestStageModuleBase` (or equivalent) already has a designated cross-module wiring hook, this call should go there. If not, this feature may need to define one rather than have every stage duplicate the wiring by hand.

## Status

`Approved`
