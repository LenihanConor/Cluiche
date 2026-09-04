---
schema: dia.module.v1
module_id: dia.attributevisualdebugger
name: DiaAttributeVisualDebugger
owner_team: TBD
layer: domain/gameplay/ai
status: active
maturity: dev

path: Dia/DiaAttributeVisualDebugger
language: cpp
parent_module_id: dia.root

summary: >
  IDebugDomain implementation for DiaAttribute. Panel-only (no world drawers).
  Resolves the live selected entity, reads its AttributeSetComponent and emits
  every attribute's resolved value, base value and clamp range along with the
  full modifier stack including live when_condition state. Entire module is
  #ifdef DIA_DEBUG guarded.

intent: >
  Separate static library that adds per-entity attribute visibility without
  creating a dependency from DiaAttribute onto DiaVisualDebugger or DiaEntity's
  debug surface. Also the first panel-only domain that consumes IDebugContext
  selection state, which it obtains by capturing the DebugLayerManager in
  Register() rather than by registering any drawer.

responsibilities:
  - AttributeVisualDebugger — IDebugDomain implementation (panel-only)
  - Generation-correct resolution of IDebugContext::GetSelectedEntityId via Domain::GetAliveEntity
  - Attribute table emission — resolved value, base value, minimum/maximum range
  - Modifier stack emission — name, operation, value, conditional flag, live condition state
  - Push-driven refresh as an IAttributeObserver on the selected entity's AttributeSet
  - Bounded conditional-modifier re-poll, covering DiaAttribute's no-event-on-condition-change gap
  - Entire public API is DIA_DEBUG guarded

non_responsibilities:
  - Attribute resolution or clamping semantics — DiaAttribute
  - Condition parsing and evaluation — DiaCondition
  - Entity selection / picking — DiaPicking via the application layer
  - Panel rendering and layout — DiaVisualDebugger's DiaDebugPanel

dependent_modules:
  - dia.attribute

public_api:
  headers:
    - Dia/DiaAttributeVisualDebugger/AttributeVisualDebugger.h
  namespaces:
    - Dia::AttributeVisualDebugger

dependencies:
  required:
    - dia.core
    - dia.attribute
    - dia.condition
    - dia.entity
    - dia.visualdebugger
  forbidden:
    - dia.applicationflow
    - dia.statemachine
    - dia.aibudget
---
