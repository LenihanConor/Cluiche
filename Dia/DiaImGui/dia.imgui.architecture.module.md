---
schema: dia.module.v1
module_id: dia.imgui
name: DiaImGui
layer: foundation/services
path: Dia/DiaImGui
status: active
maturity: dev
parent_module_id: dia.root

summary: >
  Dear ImGui integration — wraps the ImGui context lifecycle and provides the
  Dia-standard entry points for begin-frame / end-frame / render within the
  bgfx render loop.

responsibilities:
  - ImGui context init / shutdown
  - Begin-frame and end-frame hooks
  - Input event forwarding (keyboard, mouse) from DiaInput to ImGui
  - bgfx draw-list submission

non_responsibilities:
  - Individual ImGui panel content (owned by debug/editor modules)
  - CEF-based editor UI (DiaEditor)

dependencies:
  required:
    - dia.core
  forbidden: []
---
