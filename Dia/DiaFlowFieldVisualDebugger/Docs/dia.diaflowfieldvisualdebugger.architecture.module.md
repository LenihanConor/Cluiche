---
schema: dia.module.v1
module_id: dia.flowfieldvisualdebugger
name: DiaFlowFieldVisualDebugger
layer: domain/gameplay/tools
path: Dia/DiaFlowFieldVisualDebugger
status: active
maturity: dev
parent_module_id: dia.flowfield
dependent_modules:
  - dia.core
  - dia.maths
  - dia.pathfinding
  - dia.flowfield
dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.pathfinding
    - dia.flowfield
  forbidden:
    - dia.entity
    - dia.application
    - dia.graphics
    - dia.aibudget
    - dia.steering
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
