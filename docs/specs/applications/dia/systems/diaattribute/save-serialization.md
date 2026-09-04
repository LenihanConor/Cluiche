# Feature Spec: Save/Serialization Support

## Parent System
@docs/specs/applications/dia/systems/diaattribute/diaattribute.md

## Problem Statement

`DiaEconomy` explicitly left save/game serialization as a non-responsibility. DiaAttribute makes the same call for Core (see system spec Non-Responsibilities), but — unlike Economy — closes the gap with its own feature rather than leaving it open indefinitely: without this, every buff/equipment-driven stat resets on save/load, which is a correctness requirement for any real game built on this system, not an optional extra.

**This spec was drafted against the actual save-game headers** (`Dia/DiaSaveGame/ISaveable.h`, `SaveContext.h`, `LoadContext.h`), not an assumed API — an earlier draft of this spec incorrectly assumed `DiaSerializer`'s `MetadataValue`/`MetadataArray` primitives were the right serialization mechanism. They are not: `MetadataArray` is hard-capped at `kMaxMetadataEntries = 8` total key/value entries and is meant for small generic metadata blobs, not bulk per-instance game state. A single attribute schema (a football player, say) can easily have 20-40 attributes, each potentially carrying several modifiers — nowhere close to fitting in 8 entries. The correct mechanism is `Dia::SaveGame::ISaveable` (`Serialize(SaveContext&)`/`Deserialize(LoadContext&)`/`GetVersion()`), which supports arbitrary nested objects and arrays via `BeginObject`/`BeginArray`/`Write`/`Read`.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | `AttributeSet::Serialize(SaveContext&)`/`Deserialize(LoadContext&)` round-trip every attribute's base value and its full active modifier list (`modifier_name`, `attribute_name`, `operation`, `value`, `when_condition`) with no data loss — save then load reproduces identical `GetValue()` results for every attribute | Build a set with several attributes and a mix of modifiers, serialize, deserialize into a fresh set, assert every `GetValue()` matches |
| AC-2 | A saved modifier whose `attribute_name` no longer exists in the current `AttributeSchema` (e.g. a schema changed between versions) is dropped on load with a `DIA_LOG_WARN`, not a crash or silent corruption of an unrelated attribute | Load a save referencing a removed attribute name; assert a warning and that all other attributes load correctly |
| AC-3 | `GetVersion()` is set on `AttributeSet`'s `ISaveable` implementation; a version mismatch between the save data and the current schema is detectable by whatever migration path `SaveManager`/`SaveRegistry` already uses for versioned saves — this feature does not invent its own migration mechanism | Save at version N, bump `GetVersion()` to N+1, load the old save; assert the existing `DiaSaveGame` migration path is invoked, not a hard failure |
| AC-4 | `ModifierHandle` values are **not** assumed stable across save/load — deserialized modifiers get freshly-issued handles from the `AttributeSet`'s own `HandlePool`; nothing about `Serialize`/`Deserialize` attempts to reproduce prior handle values | Save with a modifier at handle H1, load into a fresh set, assert the reconstructed modifier has a handle that is valid but not asserted to equal H1 |

## Design

### Shape

```cpp
namespace Dia::Attribute {

    class AttributeSet : public Dia::SaveGame::ISaveable {
    public:
        // ... Core API unchanged ...

        void     Serialize  (Dia::SaveGame::SaveContext& ctx) const override;
        void     Deserialize(Dia::SaveGame::LoadContext& ctx)       override;
        uint32_t GetVersion () const                           override;
    };
}
```

### Serialize

```cpp
void AttributeSet::Serialize(SaveContext& ctx) const {
    ctx.BeginObject(StringCRC("base_values"));
    for (each attribute in schema)
        ctx.Write(attribute.attribute_name, GetBaseValue(attribute.attribute_name));
    ctx.EndObject();

    ctx.BeginArray(StringCRC("modifiers"));
    for (each active ModifierEntry across all attribute slots) {
        ctx.BeginObject(StringCRC("")); // array element
        ctx.Write(StringCRC("modifier_name"),  modifier.modifier_name.AsChar());
        ctx.Write(StringCRC("attribute_name"), modifier.attribute_name.AsChar());
        ctx.Write(StringCRC("operation"),      static_cast<int32_t>(modifier.operation));
        ctx.Write(StringCRC("value"),          modifier.value);
        ctx.Write(StringCRC("when_condition"), modifier.when_condition); // empty string if unconditional
        ctx.EndObject();
    }
    ctx.EndArray();
}
```

`ModifierHandle` is deliberately not written (AC-4) — handles are `HandlePool`-issued, runtime-only identifiers with no meaning across a save/load boundary.

### Deserialize

Mirrors `Serialize` using `LoadContext::BeginObject`/`Read`/`BeginArray(key, countOut)`/`SetArrayIndex`/`EndArray`. For each modifier entry read: look up `attribute_name` against the current schema via `AttributeSchema::FindAttribute` — if not found, `DIA_LOG_WARN` and skip (AC-2), otherwise reconstruct the `AttributeModifier` and call the *existing* `AddModifier` path (reusing Core's validation, including the Feature 2 `when_condition` parse/validate step if a condition string is present and Feature 2 is linked) rather than writing modifiers directly into internal storage.

### Inherited constraints active for this feature

- **PD-004 / AD-002** — `SaveContext`/`LoadContext` are already DiaCore-container-based (`DynamicArrayC`); no STL introduced by this feature.
- Reuses `Serialize`/`Deserialize`'s `StringCRC` keys for every field, consistent with PD-001.

## Open Design Questions

1. **Orphaned modifier policy (AC-2)** — drop-with-warning is recommended above; an alternative is loading the modifier as inert/orphaned (kept but never applied) in case a later patch re-adds the attribute. Recommend drop-with-warning for simplicity; revisit only if a real game needs schema-attribute-removal-then-reintroduction round-tripping.
2. **Time-limited modifier duration** — blocked on system spec Open Design Question #2 (does `AttributeModifier` ever gain a `duration` field?). If it does, this feature's serialized modifier shape needs a `remaining_duration` field; if it doesn't, modifiers are assumed either permanent or externally re-applied after load by whatever status-effect system owns them, and this feature needs no change. Do not add a duration field speculatively before that question resolves.
3. **`SaveContext::kBufferSize = 65536`** — confirm a schema with many attributes and a full `kMaxModifiersPerAttribute` stack on several of them comfortably fits the existing 64KB buffer; if not, this is a `DiaSaveGame` capacity question, not something this feature can change unilaterally.

## Status

**Status:** `Approved`
