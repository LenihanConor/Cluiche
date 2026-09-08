# Feature Spec: DiaAttribute Core

## Parent System
@docs/specs/applications/dia/systems/diaattribute/diaattribute.md

## Problem Statement

Nothing else in this system can exist without a working `AttributeSchema`/`AttributeSet`/modifier pipeline and a way to attach one to an entity. This feature delivers that foundation: JSON-loaded attribute definitions, per-entity resolved values composed from a base value plus a modifier stack through a fixed evaluation pipeline, and `AttributeSetComponent` as the single `diaentitytemplate` component that makes an `AttributeSet` usable on an entity at all (respecting `kMaxComponentTypesPerDomain = 64` — one component per entity, never one per attribute). Every other feature (conditional modifiers, change notifications, the accessor bridge, the visual debugger, serialization) targets the API this feature defines.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | `AttributeSchema::LoadFromJson` loads a valid schema file; `IsValid()` returns `true`; `FindAttribute` returns definitions with correct `minimum_value`/`maximum_value`/`default_value` | Load the dragon schema JSON from the Public Interfaces example; assert all four fields per attribute |
| AC-2 | `LoadFromJson` returns an invalid schema (`IsValid() == false`) for a missing file or one violating `minimum_value ≤ default_value ≤ maximum_value`; `DIA_LOG_WARN` fires on the range violation | Load a schema with `default_value` above `maximum_value`; assert `IsValid() == false` and the warning log |
| AC-3 | `AttributeSet::CreateFromSchema` initializes every attribute's base value to its schema `default_value` | Create a set from the dragon schema; assert `GetBaseValue("health") == 1000.0f` |
| AC-4 | With zero modifiers, `GetValue(attr) == GetBaseValue(attr)` exactly — an empty pipeline introduces no drift | Create a set, call `GetValue`/`GetBaseValue` for every attribute, assert equality |
| AC-5 | A single `Add` modifier shifts `GetValue()` by exactly its `value` relative to base, pre-clamp | `AddModifier({Add, +15})` on Strength (base 50, max 200); assert `GetValue("strength") == 65.0f` |
| AC-6 | Two `Add` modifiers on the same attribute sum: `GetValue() == base + valueA + valueB` | Add two Add modifiers; assert the sum |
| AC-7 | `Multiply` modifiers apply to `(base + sum of Add modifiers)`; multiple `Multiply` modifiers combine multiplicatively (product), not additively | Base 50, Add +10, two Multiply ×1.2 and ×1.5 → assert `GetValue() == 60 * 1.2 * 1.5` |
| AC-8 | An `Override` modifier replaces the fully-resolved value (post Add/Multiply), before Clamp | Add an Add modifier, then an Override; assert `GetValue()` equals the Override's value (pre-clamp), not the Add-inflated value |
| AC-9 | Adding a second `Override` modifier to the same attribute while one is already active triggers `DIA_ASSERT` in Debug — Override is an exclusive slot, not a stack | Add one Override, add a second; assert Debug-build assertion fires |
| AC-10 | `GetValue()` is always clamped to `[minimum_value, maximum_value]`, even when the pre-clamp resolved value would exceed bounds | Stack Add/Multiply modifiers to push a resolved value past `maximum_value`; assert `GetValue()` returns exactly `maximum_value` |
| AC-11 | `AddModifier` returns a `ModifierHandle`; `RemoveModifier(handle)` removes exactly that modifier; `GetValue()` reflects the removal immediately (no stale cache) | Add, read `GetValue()`, remove, read again; assert the second read matches the pre-add value |
| AC-12 | `RemoveModifier` with an already-removed or invalid handle is a no-op — does not crash and does not remove an unrelated modifier; `DIA_ASSERT` in Debug on double-remove | Remove a handle twice; assert no crash, Debug assertion on the second call |
| AC-13 | `SetBaseValue` updates the value the pipeline reads from and is reflected in `GetValue()` immediately, without touching the modifier stack | `SetBaseValue("health", 500)` with an existing Add +50 modifier; assert `GetValue() == 550` |
| AC-14 | `AttributeSetComponent` is a single `diaentitytemplate` component wrapping one `AttributeSet`, regardless of which attachment mechanism Open Design Question #1 resolves to; attaching a schema with N attributes registers exactly one component type, not N | Attach a component for a schema with 10 attributes; assert the entity's component-type count increased by exactly 1 |
| AC-15 | `DIA_LOG_INFO` fires exactly once per successful schema load and once per `AddModifier`/`RemoveModifier` call | Load a schema, add a modifier, remove it; assert 3 log lines |

## Design

### Storage

`AttributeSet` owns a `Dia::Core::Containers::HashTable<StringCRC, AttributeSlot>` keyed by attribute name. Each `AttributeSlot` holds:

```cpp
struct AttributeSlot {
    float                                          base_value;
    Dia::Core::Containers::DynamicArrayC<ModifierEntry, kMaxModifiersPerAttribute> modifiers;
};

struct ModifierEntry {
    ModifierHandle      handle;
    AttributeModifier   modifier;
};
```

`kMaxModifiersPerAttribute` is a small fixed cap (proposed: 16) — attributes are not expected to accumulate unbounded buff stacks; exceeding the cap is a `DIA_ASSERT` in Debug, a dropped-and-logged `AddModifier` failure in Release.

### ModifierHandle

`ModifierHandle` is a `Dia::Core::HandlePool<ModifierTag>`-issued handle, one pool per `AttributeSet` instance — same generational-handle pattern already used by `Dia::Entity::Entity` (`Handle<EntityTag>`). This gives use-after-remove detection for free (AC-12) without a bespoke validity scheme.

### Resolution pipeline

```
resolved = base_value
resolved += sum(m.value for m in modifiers if m.operation == Add)
resolved *= product(m.value for m in modifiers if m.operation == Multiply)   // product = 1.0 if none
if any(m.operation == Override for m in modifiers):
    resolved = the_one_override_modifier.value   // exactly one, enforced at AddModifier time (AC-9)
resolved = clamp(resolved, minimum_value, maximum_value)
```

Add and Multiply are commutative within their own stage (AC-6, AC-7) — insertion order never matters for them. Override is the one non-commutative case, resolved by making it an exclusive slot rather than a stack: `AddModifier` asserts if an Override modifier is already present for that attribute (AC-9). This fully resolves the system spec's Open Design Question #1 for Add/Multiply, and gives Override a concrete, testable rule instead of leaving "what if two Overrides stack" undefined.

Conditional modifiers (`when_condition` non-empty) are a field that already exists on `AttributeModifier` per the system spec's struct definition, but this feature does **not** evaluate it — every modifier added via Core is treated as always-on regardless of `when_condition` content. Evaluation is Feature 2's responsibility; Core only carries the field so Feature 2 doesn't need a struct migration.

### Entity integration

```cpp
namespace Dia::Attribute {
    class AttributeSetComponent {
    public:
        AttributeSet& GetAttributeSet();
        const AttributeSet& GetAttributeSet() const;
    private:
        AttributeSet mAttributeSet;
    };
}
```

One component instance per entity regardless of how many attributes its schema declares (AC-14) — this is the constraint that rules out a "one component per attribute" design entirely, not an optimization choice.

**The exact attachment mechanism needs confirming at implementation time — the two closest existing precedents disagree.** `ParentComponent` (`Dia/DiaEntity/Hierarchy/ParentComponent.h`) is a full `DIA_COMPONENT`/`FIELD`-reflected component: it inherits `IComponent`, declares primitive fields via `FIELD(type, name, default)`, and gets JSON load/save thunks generated by `DIA_COMPONENT_REGISTER` (using `Dia::Reflect::JsonReadArchive`/`JsonWriteArchive`) — this is what makes a component spawnable from blueprint JSON. `BlackboardComponent` (`Dia/DiaBlackboard/BlackboardComponent.h`) — the system spec's own cited precedent — is the opposite: a bare class with a `kUniqueId` and a getter, no macros, no `IComponent` inheritance visible, no reflection. `AttributeSetComponent` wraps a non-trivial runtime object (`AttributeSet`, containing a `HashTable` of per-attribute modifier stacks) that isn't a flat set of primitive fields, so it doesn't fit `FIELD`'s primitive/`FieldKind::Nested` model cleanly either way. Two candidate resolutions:

1. **Follow `ParentComponent`'s pattern**, but only reflect one field — a `StringCRC schema_name` — and resolve the actual `AttributeSchema`/populate `AttributeSet` in `OnAttach()` rather than trying to reflect the whole modifier-stack structure field-by-field. This gets blueprint-JSON spawning (`{"attribute_set": {"schema_name": "dragon"}}`) essentially for free.
2. **Follow `BlackboardComponent`'s pattern** (plain wrapper, no reflection) and accept that `AttributeSetComponent` must be constructed and attached programmatically (e.g. by whatever spawns the entity, calling `CreateFromSchema` explicitly) rather than through blueprint JSON directly.

Recommend option 1 — blueprint-driven schema selection is exactly the kind of data-driven authoring this whole system is built around, and losing it would undercut the "everything expressible as data is" principle stated in the system spec's Purpose. Confirm against `diaentitytemplate`'s current blueprint-loading code path before implementation, since `BlackboardComponent`'s lack of macros may simply predate `diaentitytemplate` (AD-005's supersession) rather than reflect a deliberate simpler-is-fine choice.

### Inherited constraints active for this feature

- **PD-001 / AD-003** — all keys (attribute names, modifier names, schema names) are `StringCRC`; namespace `Dia::Attribute::`.
- **PD-004 / AD-002** — `HashTable`/`DynamicArrayC` only; no STL in any public header.
- **PD-007** — `[[nodiscard]]` on `AddModifier`; `constexpr` for any `StringCRC` constants.
- **AD-005 (superseded)** — `AttributeSetComponent` attaches via `diaentitytemplate`, not the retired `IComponent`/`IComponentFactory` path — exact mechanism per Open Design Question #1.

## Open Design Questions

1. **Component attachment mechanism (see Design — Entity integration)** — `ParentComponent`-style full reflection with a resolved `schema_name` field, vs. `BlackboardComponent`-style bare wrapper with programmatic construction. Resolve before implementation; this determines whether blueprint-JSON spawning works out of the box.
2. **Time-limited modifiers** — Core carries no duration field and no tick/expiry (per the system spec's Non-Responsibilities); this remains open at the system level (system spec ODQ #2). Core's `AddModifier`/`RemoveModifier` API is sufficient either way — a caller-owned timer just calls `RemoveModifier` when a buff should end — so this feature does not need the question resolved to ship.
3. **`kMaxModifiersPerAttribute` cap value** — proposed 16 above is a placeholder; confirm against a realistic worst case (a heavily-buffed/debuffed unit) before implementation, or make it a schema-level override rather than a global constant.

## Status

**Status:** `Approved`
