---
schema: dia.module.v1
module_id: dia.aicallout.visualdebugger
name: DiaAICalloutVisualDebugger
owner_team: TBD
layer: domain/gameplay/tools
status: active
maturity: dev

path: Dia/DiaAICalloutVisualDebugger
language: cpp
parent_module_id: dia.aicallout

summary: >
  IDebugDomain implementation for DiaAICallout. Displays live callout table (kind, position,
  radius, faction, TTL, claimed state) in the debug panel, and draws callout radius circles
  in world space. Entire module is #ifdef DIA_DEBUG guarded.

intent: >
  Provides CalloutRegistryDebugger wrapping a const CalloutRegistry& and implementing
  IDebugDomain. Panel shows per-callout rows; world drawers show circles at callout positions.

responsibilities:
  - CalloutRegistryDebugger IDebugDomain implementation
  - GetJSONState emits live callout table
  - World circle drawers for callout radii

non_responsibilities:
  - Claiming or modifying callouts — read-only observer
  - Persisting debug state

dependent_modules:
  - dia.aicallout
  - dia.visualdebugger

public_api:
  headers:
    - Dia/DiaAICalloutVisualDebugger/CalloutRegistryDebugger.h
  namespaces:
    - Dia::AICallout

dependencies:
  required:
    - dia.core
    - dia.aicallout
    - dia.visualdebugger
  forbidden:
    - dia.blackboard
    - dia.rules
---
