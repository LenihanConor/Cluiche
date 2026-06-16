# System Spec: DiaPicking

## Parent Application
@docs/specs/applications/dia/dia.md

---

## Purpose

DiaPicking is the picking protocol layer for the Dia engine. It defines the message types, trigger semantics, routing strategy, and result containers used to communicate "what was picked?" from the picking service to consumers. It builds on DiaMailbox for transport — subscribers drain typed pick events and react independently.

The system is dimension-agnostic at its core — it owns the protocol that both 2D and 3D picking projects share. The spatial query itself (point-in-shape, ray-mesh) lives in dimension-specific child projects.

Picking is explicitly separated from three adjacent concerns:

| Concern | Owner | Not DiaPicking |
|---|---|---|
| **Transport** (how are messages delivered?) | DiaMailbox | DiaPicking defines the message shape and router, DiaMailbox delivers them |
| **Input** (where did the user click?) | DiaInput / InputStreamModule | DiaPicking never reads raw input |
| **Query** (what's at this world position?) | DiaGeometry2DPicking / DiaGeometry3DPicking | DiaPicking owns the protocol, not the spatial math |
| **Reaction** (what happens after a pick?) | Consumer modules (selection, tooltip, targeting) | DiaPicking routes results, never interprets them |

**Architectural role:**

```
InputStreamModule (mouse state)
    ↓ screen pos + button
PickingModule (CluicheGameBaseline, SimPU)
    ↓ worldPos via ViewportTransform
PickingService2D / PickingService3D (spatial query)
    ↓ PickResult<THit>
PickingModule constructs PickEvent<THit>, sends via DiaMailbox
    ↓ DiaMailbox::Send<PickEvent<THit>>(address, event)
PickRouter resolves address → subscribers by trigger type
    ↓ DiaMailbox::Drain
Consumer modules (selection state, tooltip, targeting, drag, etc.)
```

---

## Responsibilities

- Define `PickResult<THit, MaxHits>` — sorted hit collection returned by picking services (dimension-agnostic)
- Define `PickEvent<THit>` — the message type sent through DiaMailbox (wraps trigger + hits)
- Define `PickTrigger` — enum: `kClick`, `kHover`, `kRightClick`, `kAreaSelect`
- Define `PickLayer` / `PickLayerMask` — bit-flag enum for filtering pickable objects by category
- Define `PickAddress` — helper to construct `DiaMailbox::Address` for picking (routerId = "picking", payload encodes trigger type)
- Provide `PickRouter` — `IMailboxRouter` implementation that resolves pick addresses by trigger type to subscriber sets
- Provide the shared protocol types that `DiaGeometry2DPicking` and future `DiaGeometry3DPicking` depend on

## Non-Responsibilities

- Message delivery — owned by DiaMailbox
- Spatial math (point-in-shape, ray-mesh, cell resolution) — owned by dimension-specific picking projects
- Input reading — owned by DiaInput / InputStreamModule
- Coordinate transforms (screen → world) — owned by DiaGraphics / ViewportTransform
- Selection state, highlight rendering, tooltips — owned by consumers
- Entity queries — owned by `EntityPickSystem` (future diaentitytemplate integration)
- Gesture lifecycle (drag tracking, double-click) — owned by consumer or future DiaInput gesture system

---

## Public Interfaces

### PickResult

```cpp
// Dia/DiaPicking/PickResult.h
namespace Dia::Picking {

template<typename THit, unsigned int MaxHits = 16>
class PickResult {
public:
    void Add(const THit& hit);  // insert-sorted by priority descending
    bool HasHit() const;
    const THit& Best() const;   // highest priority hit
    unsigned int Count() const;
    const THit& operator[](unsigned int index) const;

private:
    Dia::Core::Containers::DynamicArrayC<THit, MaxHits> mHits;
};

} // namespace Dia::Picking
```

### PickEvent

```cpp
// Dia/DiaPicking/PickEvent.h
namespace Dia::Picking {

template<typename THit, unsigned int MaxHits = 16>
struct PickEvent {
    PickTrigger              trigger;
    PickResult<THit, MaxHits> hits;
};

} // namespace Dia::Picking
```

### PickTrigger

```cpp
// Dia/DiaPicking/PickTrigger.h
namespace Dia::Picking {

enum class PickTrigger : unsigned int
{
    kClick       = 0,
    kHover       = 1,
    kRightClick  = 2,
    kAreaSelect  = 3
};

} // namespace Dia::Picking
```

### PickLayer

```cpp
// Dia/DiaPicking/PickLayer.h
namespace Dia::Picking {

enum class PickLayer : unsigned int
{
    kDefault  = 1 << 0,
    kUnit     = 1 << 1,
    kTerrain  = 1 << 2,
    kUI       = 1 << 3,
    kDebug    = 1 << 4,
    kAll      = 0xFFFFFFFF
};

using PickLayerMask = unsigned int;

} // namespace Dia::Picking
```

### PickAddress

```cpp
// Dia/DiaPicking/PickAddress.h
namespace Dia::Picking {

struct PickAddress {
    static Dia::Mailbox::Address ForTrigger(PickTrigger trigger);
    static PickTrigger FromAddress(const Dia::Mailbox::Address& addr);

    static constexpr Dia::Core::StringCRC kRouterId = Dia::Core::StringCRC("picking");
};

} // namespace Dia::Picking
```

### PickRouter

```cpp
// Dia/DiaPicking/PickRouter.h
namespace Dia::Picking {

// Resolves pick addresses: subscribers register interest in specific triggers.
// When a PickEvent is sent with PickAddress::ForTrigger(kClick), only
// subscribers who subscribed to kClick receive it.
class PickRouter : public Dia::Mailbox::IMailboxRouter {
public:
    Dia::Core::StringCRC GetRouterId() const override;

    void Resolve(const Dia::Mailbox::Address& addr,
                 const Dia::Mailbox::SubscriberSet& liveSubscribers,
                 Dia::Mailbox::SubscriberSet& outMatched) override;

    // Registration for trigger-specific subscriptions
    void SubscribeToTrigger(PickTrigger trigger, Dia::Mailbox::SubscriberId subscriber);
    void UnsubscribeFromTrigger(PickTrigger trigger, Dia::Mailbox::SubscriberId subscriber);
};

} // namespace Dia::Picking
```

---

## Dependencies

```
DiaPicking
  depends on: DiaCore (containers, StringCRC, assert), DiaMailbox (Address, IMailboxRouter, SubscriberSet)
```

---

## Features

| Feature | Spec | Status |
|---|---|---|
| 2D Picking Service | @docs/specs/applications/dia/systems/diapicking/geometry2d-picking.md | Approved |
| 3D Picking Service | (future) | — |

---

## Decisions

| ID | Decision | Binding | Rationale |
|---|---|---|---|
| SD-PICK-001 | Pick events are delivered via DiaMailbox — PickingModule sends, consumers drain | Yes | Reuses existing transport; enables routed delivery per-trigger; consumers react at their chosen point in the frame |
| SD-PICK-002 | Layer filtering is caller-side (mask passed to Pick/PickArea call on the service), not on the message | Yes | Same trigger can produce different filtered results for different callers; filtering at query time is cheaper than post-hoc filtering on drain |
| SD-PICK-003 | PickResult sorts hits by priority descending (Best() = highest) | Yes | Consumers that want all hits iterate; most only need Best() |
| SD-PICK-004 | DiaPicking has no dimension-specific types — no Vector2D, no Ray3D | Yes | Keeps the project pure protocol; dimension lives in child projects |
| SD-PICK-005 | One message per trigger type per frame — PickRouter routes by trigger to subscribers | Yes | Consumers subscribe to the triggers they care about; no broadcast-everything |
| SD-PICK-006 | PickEvent is templated on THit — 2D sends PickEvent<PickHit2D>, 3D will send PickEvent<PickHit3D> | Yes | Keeps dimension separation clean; DiaMailbox handles them as separate registered types |
