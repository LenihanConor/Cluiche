# Feature Spec: mailbox-router

**System:** diaentitytemplate
**App:** Dia
**Status:** Draft

## Summary

Wire diaentitytemplate into DiaMailbox by implementing `EntityRouter` — a `IMailboxRouter` that decodes a packed 64-bit address payload into one of four address kinds (Entity, All, ComponentType, Self) and resolves each to the correct subscriber set. Auto-registered with the domain's `Mailbox` on construction.

## Traceability

| Level | Spec |
|---|---|
| Platform | [platform.md](../../../../platform/Cluiche.md) |
| Application | [dia.md](../../dia.md) |
| System | diaentitytemplate |
| Depends on feature | [foundation.md](foundation.md) |
| Depends on feature | [component-deps-and-refs.md](component-deps-and-refs.md) |
| Depends on system | [diamailbox.md](../diamailbox/diamailbox.md) |

## Goals

- Every entity in a domain is addressable via a single packed 64-bit payload — no secondary lookup tables
- Four address kinds cover all v1 use cases: single entity, broadcast, component-type fan-out, self-send
- Zero cost for entities that never use messaging — subscription is opt-in

## Acceptance Criteria

- `kEntityRouterId` = `StringCRC("dia.entity.router")`
- `EntityRouter` implements `Dia::Mailbox::IMailboxRouter` and is auto-registered with the domain's `Mailbox` on `Domain` construction
- `MakeEntityAddress(Entity)` encodes kind=Entity + 32-bit index + 24-bit generation into a packed `Address`
- `MakeAllAddress()` encodes kind=All
- `MakeComponentTypeAddress(StringCRC componentTypeId)` encodes kind=ComponentType + 32-bit type CRC
- `MakeSelfAddress(Entity sender)` encodes kind=Self + sender handle (same layout as Entity kind)
- `GetAddressKind`, `GetAddressEntity`, `GetAddressComponentType` correctly decode any packed address
- `EntityRouter::Resolve` for kind=Entity: adds the matching subscriber (if alive and subscribed) to `outMatched`
- `EntityRouter::Resolve` for kind=All: adds all subscribers in the domain to `outMatched`
- `EntityRouter::Resolve` for kind=ComponentType: adds all subscribers whose entity has the named component
- `EntityRouter::Resolve` for kind=Self: resolves sender entity from dispatch context, delegates to Entity resolution
- `SubscriberSet` capacity is `kMaxEntitiesPerDomain` (1024)
- Generation mismatch on Entity-kind address (stale handle) → silently no-op, no assert

## Payload Layout

```
bits 63–56 : AddressKind (uint8)
bits 55–0  : body (kind-specific)

Kind = Entity (1):
  bits 55–24 : entity index (uint32)
  bits 23–0  : entity generation (24-bit, truncated from Handle's 32-bit generation)

Kind = All (2):
  bits 55–0  : unused

Kind = ComponentType (3):
  bits 31–0  : component type CRC (StringCRC value)
  bits 55–32 : unused

Kind = Self (4):
  bits 55–24 : sender entity index (uint32)
  bits 23–0  : sender entity generation (24-bit)
```

## Data Model

### AddressKind

```cpp
namespace Dia::Entity {
    enum class AddressKind : uint8_t {
        Entity        = 1,
        All           = 2,
        ComponentType = 3,
        Self          = 4
    };
}
```

### Address helpers

```cpp
namespace Dia::Entity {
    extern const Dia::Core::StringCRC kEntityRouterId;

    Dia::Mailbox::Address MakeEntityAddress(Entity target);
    Dia::Mailbox::Address MakeAllAddress();
    Dia::Mailbox::Address MakeComponentTypeAddress(Dia::Core::StringCRC componentTypeId);
    Dia::Mailbox::Address MakeSelfAddress(Entity sender);

    AddressKind          GetAddressKind(const Dia::Mailbox::Address& addr);
    Entity               GetAddressEntity(const Dia::Mailbox::Address& addr);
    Dia::Core::StringCRC GetAddressComponentType(const Dia::Mailbox::Address& addr);
}
```

### EntityRouter

```cpp
namespace Dia::Entity {
    class EntityRouter final : public Dia::Mailbox::IMailboxRouter {
    public:
        explicit EntityRouter(Domain& domain);

        Dia::Core::StringCRC GetRouterId() const override { return kEntityRouterId; }

        void Resolve(const Dia::Mailbox::Address&    addr,
                     const Dia::Mailbox::SubscriberSet& live,
                     Dia::Mailbox::SubscriberSet&       outMatched) override;
    private:
        Domain& mDomain;
    };
}
```

`Domain` constructs an `EntityRouter` as a member and registers it with its `Mailbox` in the `Domain` constructor.

## Files Touched

| File | Change |
|---|---|
| `Dia/diaentitytemplate/EntityAddress.h` | New — `AddressKind`, address helpers, `kEntityRouterId` |
| `Dia/diaentitytemplate/EntityAddress.cpp` | New — helper implementations |
| `Dia/diaentitytemplate/EntityRouter.h` | New — `EntityRouter` declaration |
| `Dia/diaentitytemplate/EntityRouter.cpp` | New — `Resolve` implementation |
| `Dia/diaentitytemplate/Domain.h` / `.cpp` | Modified — add `EntityRouter` member, register in constructor |
| `diaentitytemplate.vcxproj` / `.filters` | Add new files |
| `Tests/GoogleTests/Entity/EntityRouterTests.cpp` | New |

## Binding Decisions Compliance

| Decision | Summary | Compliance |
|---|---|---|
| PD-001 | StringCRC for all IDs | `kEntityRouterId` is `StringCRC`. Component type addresses carry `StringCRC` CRC in body. Compliant. |
| PD-004 | No STL in public APIs | All helpers take/return `Entity`, `StringCRC`, `Dia::Mailbox::Address`. No STL. Compliant. |
| SD-ENT-010 | Entity router decodes tagged 64-bit payload | Payload layout: high byte = kind, low 56 bits = body. Compliant. |
| SD-ENT-011 | Entity-kind generation is 24-bit | High byte holds kind; 24 bits remain for generation. ~3 days worst-case churn on one slot at 60Hz. Accepted trade-off. Compliant. |
| SD-ENT-018 | Single-threaded per domain | `EntityRouter::Resolve` accesses `Domain` internals without locking. Compliant. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Self resolution | `Self` kind carries the sender's entity handle in the payload. How does `Resolve` get the sender context? | `IMailboxRouter::Resolve` receives the full `Dia::Mailbox::Message` which carries the sender's `Address`. The router decodes the sender address using `GetAddressEntity` to get the sender entity, then delegates to Entity-kind resolution. No additional context parameter needed. |
| 2 | Stale Entity-kind address | Generation mismatch — silently no-op. Should this emit `DIA_LOG_WARNING`? | Yes in debug builds — a stale address likely indicates a use-after-free bug worth surfacing. Silent in release to avoid log spam in normal entity churn scenarios. |
| 3 | ComponentType resolve cost | Kind=ComponentType iterates all live subscribers to find those whose entity has the named component. At v1 scale is this fine? | Yes. At <1024 entities with sparse subscriptions the inner loop is cheap. The bigger risk is `SubscriberSet` overflow — capped at `kMaxEntitiesPerDomain = 1024` which covers the worst case (every entity subscribed). |
| 4 | SubscriberSet resize | DiaMailbox's `SubscriberSet` was placeholder-sized at 64. Does this feature resize it? | Yes — this feature is the consumer that sets `SubscriberSet` capacity to `kMaxEntitiesPerDomain`. Per AI Q10 in the system spec. Coordinate with DiaMailbox if the capacity is a compile-time constant in its header. |
| 5 | All-kind and ComponentType-kind with no subscribers | If no entities are subscribed, `outMatched` is empty. Is that a silent success or a warning? | Silent success. Broadcasting to zero subscribers is a valid no-op (e.g. early in a stage before components have attached). No warning. |
| 6 | Router lifetime | `EntityRouter` holds a `Domain&` reference. Is there a lifetime risk if the router outlives the domain? | No — `EntityRouter` is a member of `Domain` (not heap-allocated externally), so it is destroyed before `Domain`'s other members in the destructor. No dangling reference. |

## Open Questions

None.

## Status

`Approved`
