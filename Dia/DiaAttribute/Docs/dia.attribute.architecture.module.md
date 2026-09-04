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
  - AttributeSchema JSON asset loading, attribute definitions, range validation
  - AttributeSet runtime state — CreateFromSchema, GetValue/GetBaseValue/SetBaseValue
  - Modifier stack — Add (sum), Multiply (product), Override (exclusive slot) resolution pipeline
  - ModifierHandle-based AddModifier/RemoveModifier with generational-handle safety
  - AttributeSetComponent — single component wrapping one AttributeSet per entity
  - DIA_LOG_INFO on schema load and AddModifier/RemoveModifier; DIA_LOG_WARNING on schema validation failure
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

dependent_modules: []

public_api:
  headers:
    - Dia/DiaAttribute/AttributeSchema.h
    - Dia/DiaAttribute/AttributeSet.h
    - Dia/DiaAttribute/AttributeSetComponent.h
    - Dia/DiaAttribute/IAttributeObserver.h
    - Dia/DiaAttribute/AttributeObserverSubject.h
    - Dia/DiaAttribute/AttributeAccessorBridge.h
  namespaces:
    - Dia::Attribute
  entry_points:
    - AttributeSchema
    - AttributeSet
    - AttributeSetComponent
    - AttributeAccessorBridge

dependencies:
  required:
    - dia.core
    - dia.observation
    - dia.entity
    - dia.condition
  optional: []
  forbidden:
    - dia.statemachine
    - dia.streams
    - dia.graphics
    - dia.application
---
