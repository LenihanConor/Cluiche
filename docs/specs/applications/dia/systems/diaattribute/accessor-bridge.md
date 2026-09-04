# Feature Spec: AI/Blackboard Float Accessor Bridge

## Parent System
@docs/specs/applications/dia/systems/diaattribute/diaattribute.md

## Problem Statement

`DiaRules` and `DiaUtilityAI` read gameplay state through `DiaCondition::ConditionRegistry` — JSON conditions and utility scorers reference named float/bool accessors by `(slot, field)` `StringCRC` pairs. This feature exposes an entity's resolved attribute values (`AttributeSet::GetValue`) through that same registry, so AI code can read `Health`/`Aggression`/etc. without a game writing bespoke glue per attribute.

**This spec was drafted against the actual `ConditionRegistry`/`ConditionExpr` headers** (`Dia/DiaCondition/ConditionRegistry.h`, `ConditionExpr.h`), not an assumed API. Two properties of the real contract make this harder than "just register accessors" and drive most of this feature's design:

1. **`FloatAccessorFn = float(*)(void* data)` is a plain, non-capturing C function pointer.** `data` is fixed once, at `ConditionRegistry`'s constructor, and shared by every accessor registered on that instance. There is no per-registration parameter — an accessor function cannot be told at registration time "read field X specifically" except by what's baked into the function pointer itself at compile time. `AttributeSet`'s attributes are schema/JSON-defined at runtime, not compile-time struct members, so a naive "one hand-written accessor function per attribute name" approach is impossible — attribute names aren't known until a JSON schema loads.
2. **`ConditionRegistry` has no unregister method.** Only `RegisterFloat`/`RegisterBool`/`HasFloat`/`HasBool`/`GetFloat`/`GetBool` exist. Once an accessor is registered, there is currently no DiaCondition-native way to remove it.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | `RegisterAccessors(registry, set, slot_name)` registers a float accessor for every attribute in `set`'s schema, resolvable via `registry.GetFloat(slot_name, attribute_name)` | Register accessors for a schema of N attributes; call `GetFloat` for each; assert correct values |
| AC-2 | A registered accessor always returns the attribute's *live* resolved `GetValue()` at query time, not a value snapshotted at registration time | Register, mutate the attribute via `SetBaseValue`/`AddModifier`, call `GetFloat` again; assert it reflects the mutation |
| AC-3 | `RegisterAccessors` supports at most `kMaxBridgedAttributesPerSet` attributes per call (see Design — this is a hard cap imposed by the compile-time trampoline table, not a soft limit); exceeding it is a `DIA_ASSERT` in Debug and a logged partial registration in Release | Register a schema with more than the cap; assert Debug assertion / Release warning |
| AC-4 | A `DiaCondition` JSON expression referencing `slot_name.attribute_name` resolves to the correct live value through the real `ConditionRegistry`/`ConditionExpr` types (integration test, not a mock) | Load a `ConditionExpr` referencing a bridged attribute; `Evaluate()` against the real registry; assert correct result |
| AC-5 | Re-registering the same `(slot_name, attribute_name)` pair is documented and enforced as a hard error (`DIA_ASSERT` in Debug) — not a silent overwrite — since `ConditionRegistry` provides no way to detect or safely replace an existing registration | Call `RegisterAccessors` twice for the same slot; assert Debug assertion on the second call |

## Design

### The core problem: dynamic field names against a static accessor contract

Because `FloatAccessorFn` cannot capture which attribute it should read, this feature cannot write N accessor functions for N JSON-defined attribute names at runtime. The resolvable approach: give `AttributeSet` an **index-based** accessor (`GetValueByIndex(unsigned int index)`, stable per schema load) and pre-instantiate a **fixed-size compile-time table of trampoline functions**, each hardcoding a different index via template instantiation:

```cpp
namespace Dia::Attribute {

    inline constexpr unsigned int kMaxBridgedAttributesPerSet = 64;

    template <unsigned int Index>
    float BridgedAttributeAccessor(void* data) {
        return static_cast<const AttributeSet*>(data)->GetValueByIndex(Index);
    }

    // Compile-time table of kMaxBridgedAttributesPerSet distinct real function pointers —
    // this is what makes runtime "register every attribute" possible without per-name codegen.
    template <unsigned int... Is>
    constexpr auto MakeAccessorTable(std::integer_sequence<unsigned int, Is...>) {
        return std::array<Dia::Condition::FloatAccessorFn, sizeof...(Is)>{ &BridgedAttributeAccessor<Is>... };
    }
    inline constexpr auto kAccessorTable =
        MakeAccessorTable(std::make_integer_sequence<unsigned int, kMaxBridgedAttributesPerSet>{});

    class AttributeAccessorBridge {
    public:
        // `set` must outlive every ConditionRegistry it was bridged into (see Open Design
        // Questions — there is no unregister, so this is a hard lifetime requirement).
        static void RegisterAccessors(Dia::Condition::ConditionRegistry& registry,
                                       const AttributeSet& set,
                                       StringCRC slot_name);
    };
}
```

`RegisterAccessors` iterates the schema's attributes in index order and calls `registry.RegisterFloat(slot_name, attribute_definition.attribute_name, kAccessorTable[index])` for each — `data` for the whole `registry` must be `const_cast<AttributeSet*>(&set)` (or the registry must have been constructed with `&set` as its `data` pointer in the first place; see below). `GetValueByIndex` is a small Core addition alongside `GetValue(StringCRC)`.

**This only works if the `ConditionRegistry` instance's `data` pointer *is* the bridged `AttributeSet`.** If a game's per-entity `ConditionRegistry` is already constructed with `data` pointing at some other aggregate (e.g. a Blackboard slot), this feature's accessors cannot be registered onto that same registry instance — they need their own `ConditionRegistry` whose `data` is the `AttributeSet*`. This means, in practice, **one dedicated `ConditionRegistry` per bridged `AttributeSet`**, not accessors merged into an existing shared per-entity registry. Whatever evaluates conditions against "this entity's state" needs to either evaluate against multiple registries (one per bridged system) or DiaCondition needs a composite-context concept — neither of which this feature can resolve unilaterally (see Open Design Questions).

### Lifetime

Given no unregister exists, `AttributeAccessorBridge::RegisterAccessors` documents (and asserts where feasible) that the bridged `AttributeSet`/its owning `AttributeSetComponent` must outlive the `ConditionRegistry` it was registered into. Entity despawn before registry teardown is a dangling-pointer risk this feature cannot close with DiaCondition's current API.

## Open Design Questions

1. **Should `ConditionRegistry` gain an unregister method instead of working around its absence?** This is the highest-leverage fix — it would remove the dangling-pointer lifetime hazard above entirely. This is a change to `DiaCondition`, a different system; raising it there is likely more valuable than any workaround DiaAttribute can build alone. Recommend: flag to whoever owns `DiaCondition` before implementing this feature; if accepted, this feature's design simplifies substantially (register on attach, unregister on entity despawn, no dangling risk).
2. **One dedicated `ConditionRegistry` per bridged `AttributeSet`, vs. a composite-context design in `DiaCondition` that lets multiple systems (Blackboard, Attribute, others) contribute accessors to one shared per-entity context.** The per-`AttributeSet`-registry approach above is buildable today with zero changes elsewhere, but means AI code evaluating a condition that mixes a Blackboard tag and an Attribute value needs two registries/contexts, not one. Confirm whether existing AI condition usage already needs multi-context composition (check `DiaRules`/`DiaUtilityAI` call sites) before committing to the single-registry-per-system pattern.
3. **`kMaxBridgedAttributesPerSet` cap value** — 64 is a placeholder matching `kMaxComponentTypesPerDomain`; there's no structural reason it must match that constant. Confirm a real cap against expected schema sizes (a football player schema might have 30-40 attributes; a dragon fewer) before implementation.

## Status

**Status:** `Approved`
