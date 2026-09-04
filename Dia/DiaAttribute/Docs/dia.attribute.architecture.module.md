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

non_responsibilities:
  - when_condition parsing/resolvability is validated eagerly at AddModifier time (invalid JSON or an
    unresolvable accessor rejects the modifier before it is ever added — see ConditionExpr::Validate)
  - Accessor bridge to DiaCondition/DiaBlackboard (later feature)
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
  namespaces:
    - Dia::Attribute
  entry_points:
    - AttributeSchema
    - AttributeSet
    - AttributeSetComponent

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
