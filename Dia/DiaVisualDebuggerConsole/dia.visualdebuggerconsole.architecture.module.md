---
schema: dia.module.v1
module_id: dia.visualdebuggerconsole
name: DiaVisualDebuggerConsole
layer: domain/visual/tools
path: Dia/DiaVisualDebuggerConsole
status: active
maturity: dev
parent_module_id: dia.root

summary: >
  ImGui-based console overlay rendered on top of the game viewport —
  layer toggles, scale sliders, and live metric display driven by DiaVisualDebugger.

responsibilities:
  - ImGui panel for debug layer toggle / scale controls
  - Live metric readout (FPS, draw call count, etc.)
  - Wiring DiaAPI commands to panel state

non_responsibilities:
  - Debug draw submission (DiaDebugDraw)
  - Rendering implementation (DiaVisualDebugger / DiaBgfx)

dependencies:
  required:
    - dia.core
    - dia.maths
    - dia.graphics
    - dia.api
    - dia.imgui
    - dia.debug.visualdebugger
    - dia.observation
  forbidden: []
---
