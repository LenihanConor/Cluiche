---
schema: dia.module.v1
module_id: dia.core.ui
name: DiaCore.UI
owner_team: TBD
layer: foundation/core
status: active
maturity: dev

path: Dia/DiaCore/UI
language: cpp
parent_module_id: dia.ui

summary: >
  JS<->C++ bridge contract hosted under DiaCore so foundation-tier consumers (e.g.
  DiaEditor's WebUIBridge) can register/invoke JS handlers without depending on the
  domain-tier DiaUI module (Page, render overlay, input injection). Mirrors
  DiaCore/DebugDraw/IDebugContext's decoupling pattern. Zero dependencies beyond
  the standard library.

responsibilities:
  - IJSBridge — RegisterJSHandler/CallJSFunction contract, implemented by Dia::UI::IUISystem

non_responsibilities:
  - Page management, rendering overlay, input injection — Dia::UI::IUISystem
  - Any concrete UI backend (CEF, Ultralight) — DiaUICEF, DiaUIUltralight

dependent_modules:
  - dia.ui

dependencies:
  required: []
  forbidden: []
---
