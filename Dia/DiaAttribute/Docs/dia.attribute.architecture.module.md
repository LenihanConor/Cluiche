---
schema: dia.module.v1
module_id: dia.attribute
name: DiaAttribute
owner_team: TBD
layer: domain/gameplay/core
status: active
maturity: dev

path: Dia/DiaAttribute
language: cpp
parent_module_id: dia.root

summary: >
  Generic data-driven gameplay attribute/stat framework — named attributes (Health, Strength, etc.)
  resolved from a base value plus a modifier stack (Add / Multiply / Override), clamped to
  schema-defined bounds.

intent: >
  Provides AttributeSchema (JSON asset loading + validation) and AttributeSet (runtime resolution)
  so games define their attribute vocabulary in data. AttributeSetComponent wraps one AttributeSet
  per entity as a single diaentitytemplate component, regardless of how many attributes the schema
  defines.

responsibilities:
  - AttributeSchema JSON asset loading, attribute definitions, range validation, band_schema_version
  - Band-based initialization — AttributeSchema.bands (StringCRC→BandRange map per attribute), 
    AttributeSet::InitializeFromBands(DynamicArrayC<BandValue>) resolves band names to midpoint of [min, max],
    stores as base value; schema-versioned (band_schema_version); fallback to default_value on missing band
  - AttributeSet runtime state — CreateFromSchema, GetValue/GetBaseValue/SetBaseValue, O(1) index-based access
    (GetBaseValueByIndex, SetBaseValueByIndex, GetAttributeNameByIndex)
  - Modifier stack — Add (sum), Multiply (product), Override (exclusive slot) resolution pipeline
  - Kind-aware modifiers — Add and Multiply only on float and int (int result rounded half away from zero then clamped);
    Override on all kinds with value validated per kind (bool 0 or 1, enum in-range ordinal, int whole number);
    illegal calls return invalid handle and log DIA_LOG_WARNING naming the attribute
  - ModifierHandle-based AddModifier/RemoveModifier with generational-handle safety
  - AttributeSetComponent — single component wrapping one AttributeSet per entity
  - DIA_LOG_INFO on schema load and AddModifier/RemoveModifier; DIA_LOG_WARNING on schema validation failure and
    illegal modifier operations
  - Conditional modifier gating — when_condition (DiaCondition ConditionExpr JSON) evaluated per-resolve
    via a per-AttributeSet ConditionRegistry (SetConditionRegistry), without RemoveModifier churn
  - Change notifications — AttributeObserverSubject/IAttributeObserver push OnAttributeChanged and
    edge-triggered OnAttributeReachedMaximum/OnAttributeReachedMinimum synchronously from SetBaseValue,
    AddModifier, and RemoveModifier when a mutation actually changes the resolved value
  - AttributeAccessorBridge — exposes an AttributeSet's live resolved values to DiaRules/DiaUtilityAI
    via DiaCondition's ConditionRegistry, registering one float accessor per schema attribute under a
    caller-chosen slot name (resolvable as "slot_name.attribute_name" from condition JSON). Works
    around ConditionRegistry's non-capturing float(*)(void*) accessor signature using a compile-time
    trampoline table of kMaxBridgedAttributesPerSet (64) index-templated accessors
  - Index-stable attribute access — AttributeSet::GetAttributeCount/GetValueByIndex/
    GetAttributeNameByIndex; attribute slots keep their schema index for the AttributeSet's lifetime
    (slots are never removed after InitializeFromSchema, only modifiers are)
  - Save/load — AttributeSet implements Dia::SaveGame::ISaveable: Serialize/Deserialize round-trip
    every attribute's base value and its full active modifier list; a saved modifier whose
    attribute no longer exists in the current schema is dropped with a DIA_LOG_WARNING rather than
    a crash; deserialized modifiers always receive freshly-issued ModifierHandle values
  - ParameterFile — strict/lenient schema loading from DiaValueSchema sidecars (with bands, map defaults,
    and cross-field constraints); applies override packs atomically; single value setting; reload from sidecar
    plus pack list; JSON export (full or diff pack format); deterministic FNV-1a-64 hash (schema-aware, excludes
    modifiers); non-copyable, non-movable lifetime (Load remembers non-owning IMapKeySource*, Reload reuses it)
  - ParamBinding<T> — template to bind C++ struct members to parameter file entries by name once per step;
    Resolve reports unbound parameters and entries with no binding; Snapshot copies resolved values (modifiers
    included) by index with zero allocations; template specializations for float, int32_t, bool; BindEnum and
    BindMap for enum ordinals and multi-value maps; max 256 bindings per instance
  - AttributeSchema::FromValueSchema — builds a schema from a DiaValueSchema sidecar (kind: parameter entries);
    expands map entries to one definition per key from an IMapKeySource; resolves constraints to indices;
    checks defaults and constraints on the sidecar; reports all failures by key
  - AttributeSchema::CheckConstraints — validates numeric-field constraints (a op b, op ∈ {<,<=,>,>=,==,!=});
    names both keys on violation; used by FromValueSchema, ParameterFile strict load/Set, and ApplyPack (atomic)
  - AttributeSchema additions — GetBandSchemaVersion() returning the optional sidecar field bandSchemaVersion
    (default 1); AttributeKind enum (Float, Int, Bool, Enum) in kind-aware modifiers and ParameterFile snapshot
  - IMapKeySource / MapKeys — registry interface for resolving parameter map entries (keyRef) to numeric IDs
    and string names; null source allowed when there are no maps

lifetime_hazards:
  - A bridged AttributeSet MUST outlive every ConditionRegistry it was registered into.
    ConditionRegistry has no unregister method, so there is nothing to call on entity despawn —
    registered accessors keep dereferencing the registry's `data` pointer for the registry's whole
    life. A registry bound to a per-entity AttributeSet must therefore be owned by, and destroyed
    with, that entity. Known and accepted; see Open Design Question #1 in the accessor-bridge spec.
  - AttributeAccessorBridge::RegisterAccessors cannot verify that `registry` was constructed with
    `data` pointing at the AttributeSet being bridged (ConditionRegistry exposes no getter for
    `data`). Violating that precondition silently reads through the wrong object.

non_responsibilities:
  - when_condition parsing/resolvability is validated eagerly at AddModifier time (invalid JSON or an
    unresolvable accessor rejects the modifier before it is ever added — see ConditionExpr::Validate)
  - Unregistering bridged accessors — ConditionRegistry has no unregister API; DiaAttribute does not
    add one (that would be a DiaCondition change)
  - Bool accessors — AttributeAccessorBridge bridges float attributes only
  - Schema-asset registry / lookup-by-name (AttributeSetComponent::OnAttach is a no-op stub until this exists)
  - UI and rendering

dependent_modules:
  - dia.value_schema

public_api:
  headers:
    - Dia/DiaAttribute/AttributeSchema.h
    - Dia/DiaAttribute/AttributeSet.h
    - Dia/DiaAttribute/AttributeSetComponent.h
    - Dia/DiaAttribute/IAttributeObserver.h
    - Dia/DiaAttribute/AttributeObserverSubject.h
    - Dia/DiaAttribute/AttributeAccessorBridge.h
    - Dia/DiaAttribute/ParameterFile.h
    - Dia/DiaAttribute/ParamBinding.h
    - Dia/DiaAttribute/IMapKeySource.h
  namespaces:
    - Dia::Attribute
  entry_points:
    - AttributeSchema
    - AttributeSet
    - AttributeSetComponent
    - AttributeAccessorBridge
    - ParameterFile
    - ParamBinding<T>
    - IMapKeySource

dependencies:
  required:
    - dia.core
    - dia.value_schema
    - dia.observation
    - dia.entity
    - dia.condition
    - dia.savegame
  optional: []
  forbidden:
    - dia.statemachine
    - dia.streams
    - dia.graphics
    - dia.application
---

## Removed API (Feature 8, 2026-11-18)

The old AttributeSchema JSON format has been removed in favor of DiaValueSchema sidecars (dia.value_schema v1):

- `AttributeSchema::LoadFromJson(const char* path)` — removed; use `ValueSchemaLoader::LoadFromText()` then `AttributeSchema::FromValueSchema()`
- `AttributeSchema::LoadFromJsonValue(const Json::Value&)` — removed; same replacement path
- `band_schema_version` JSON key in the old format — replaced by the optional sidecar field `bandSchemaVersion` (whole number, default 1 when absent)

The old format was understood by the loader and three public methods (`ParseAttributes`, `ParseBands`, `ValidateBandRange`); those are also deleted. Callers must migrate to sidecars.

## Usage Notes (Feature 8, 2026-11-18)

- **ParameterFile lifetime:** Not copyable or movable (owns an `AttributeSet`). `Load` remembers a NON-OWNING `IMapKeySource*` pointer; the source must outlive the file because `Reload` reuses that pointer. Pass the source once to `Load` and it is stored for all future `Reload` calls.
- **JSON shape:** `ToJson()` returns `Json::Value` with members in alphabetical order (jsoncpp default), not schema order. Only the C++ base-value layout (`Hash`, `Snapshot`) respects schema order. Floats are serialized as shortest decimal round-trip form. `-0.0` is exact in memory only; jsoncpp text serializes it as `-0` and reads it back as `+0.0`.
- **Lenient pack atomicity:** `ApplyPack` validates a candidate copy of the base values first (against type, range, and constraints). If any constraint fails after the merge, the whole pack is rejected and base values remain byte-identical. A missing or mismatching `basedOn` is treated as a stale pack: one warning (key `basedOn`) and the pack applies leniently.
- **Modifier survival across Reload:** Modifiers survive `Reload` by matching the attribute name (`AttributeDefinition::fullKey`). Modifiers on attributes that disappear from the new schema are dropped with a warning. Modifiers on new attributes in the reloaded schema do not exist (they were not active before).
- **Modifier legality by kind:** `AddModifier` checks the attribute's `AttributeKind`: Add and Multiply are illegal on bool and enum (return invalid handle and log DIA_LOG_WARNING naming the attribute). Override is legal on all kinds if the value is valid for that kind (bool: 0 or 1; enum: ordinal in range; int: whole number).
- **ParamBinding kind check:** `ParamBinding<T>::Resolve` compares each binding's member kind with the attribute's `AttributeKind` (float to Float, int32_t to Int, bool to Bool, `BindEnum` to Enum; map columns likewise) and lists mismatches in `BindingReport::kindMismatches` (the binding is then skipped by `Snapshot`; `IsClean()` is false). The one explicit cross-kind form is `BindIntFromFloat(key, int32_t T::*)`, which feeds an int32_t member from a Float attribute: rounds half away from zero, saturates to the int32 range, NaN becomes 0. Plain `Bind(int32_t)` on a Float attribute is a mismatch. TerrainRules uses `BindIntFromFloat` for its 12 int rules.
- **Hash stability:** `Hash()` (base values) and `SchemaHash()` (schema definition) are both FNV-1a 64. Base-value `Hash()` uses float32 bit patterns with `-0.0` normalized to `+0.0`. `SchemaHash()` hashes the sidecar's parameter entry keys, kinds, min/max, defaults, enum value lists (ids and order), bands, and constraints; it excludes presentation fields (label, description, hint, unit, ui, color). The exact byte layout is documented in `ParameterFile.cpp`.
- **Map defaults:** A map entry's `default` can be an object keyed by map key (e.g., `{"Biome.Grassland": 0.6}`). Every key in the object must resolve through the `IMapKeySource`; a missing or unknown key fails `FromValueSchema`. The CLI mirrors this as `data.unresolved-map-key` and `data.missing-map-key`. In a pack, map values are a nested object. An enum-valued map (`valueType: enum`) takes `mapKey -> enum id string` in its object default; the loader checks each id against the entry's `values` (failure reason starts `map default '<key>'`) and `FromValueSchema` converts the ids to ordinals for the expanded `<key>.<mapKey>` definitions. `ToJson` writes, and `ApplyPack` reads, enum map columns as id strings. The loader now reads `default` after `valueType` and the enum values, so int, bool and string map defaults are checked against their real value type (a bool map default must be `true`/`false`, not 0/1).
- **Pack keys are flat (2026-11-20):** a pack addresses a parameter by its flat full key `<group>.<key>` (as `Set` does); a nested group object such as `{"grazing": {"rate": 2.5}}` is not a pack form. `ApplyPack` drops it with a warning naming the group, and the CLI reports `data.nested-group-pack` (naming the flat key to use). Only an object under a map parameter name (map columns) is accepted.
- **Reserved parameter keys:** a `kind: parameter` sidecar entry keyed `format`, `version` or `basedOn` is rejected by `ValueSchemaLoader` (and by the CLI rule `schema.reserved-key`), because the pack envelope owns those keys.


