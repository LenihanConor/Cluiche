---
schema: dia.module.v1
module_id: dia.diaflowfieldvisualdebugger
display_name: DiaFlowFieldVisualDebugger
layer: domain/gameplay/tools
status: active
maturity: dev
parent_module: ~
dependent_modules:
  - dia.core
  - dia.maths
  - dia.pathfinding
  - dia.flowfield
  - dia.visualdebugger
forbidden_dependencies:
  - dia.entity
  - dia.application
  - dia.graphics
  - dia.aibudget
  - dia.steering
  - imgui
public_api:
  headers:
    - DiaFlowFieldVisualDebugger/FlowFieldVisualDebugger.h
  namespaces:
    - Dia::FlowField
  entry_points:
    - FlowFieldVisualDebugger
build:
  type: StaticLibrary
  configurations: [Debug, Debug-Asan, Debug-Ubsan, Release]
  debug_only: true
  vcxproj: Dia/DiaFlowFieldVisualDebugger/DiaFlowFieldVisualDebugger.vcxproj
  sln_folder: 3.1-Gameplay-Tools
spec: docs/specs/applications/dia/systems/diaflowfieldvisualdebugger/diaflowfieldvisualdebugger.md
---

## Responsibilities

- Implement `IDebugDomain` for `DiaFlowField` — group Navigation, domain ID `"flowfield"`
- Two world-space drawers: `DirectionArrowsDrawer` (one line per reachable cell) and `ReachabilityOverlayDrawer` (one quad per cell)
- Forward drawer enable/disable and arrowLength scale via `OnCommand`
- Emit `drawers` + `stats` (cellCount, reachableCount, isComplete) via `GetJSONState`
- Entire module guarded by `#ifdef DIA_DEBUG`

## Non-Responsibilities

- Flow field computation — `DiaFlowField`
- Grid geometry — `DiaPathfinding`
- Any runtime behaviour in Release builds
