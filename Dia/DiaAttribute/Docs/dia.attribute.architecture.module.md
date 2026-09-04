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

non_responsibilities:
  - when_condition evaluation (carried only — Feature 2 evaluates it)
  - Observer/event notifications on attribute change (later feature)
  - Accessor bridge to DiaCondition/DiaBlackboard (later feature)
  - Schema-asset registry / lookup-by-name (AttributeSetComponent::OnAttach is a no-op stub until this exists)
  - UI and rendering

dependent_modules: []

public_api:
  headers:
    - Dia/DiaAttribute/AttributeSchema.h
    - Dia/DiaAttribute/AttributeSet.h
    - Dia/DiaAttribute/AttributeSetComponent.h
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
  optional: []
  forbidden:
    - dia.statemachine
    - dia.streams
    - dia.graphics
    - dia.application
---
