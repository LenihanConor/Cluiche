---
schema: dia.module.v1
module_id: dia.uicef
name: DiaUICEF
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaUICEF
language: cpp
parent_module_id: dia.ui

summary: >
  CEF (Chromium Embedded Framework) implementation of the IUISystem interface.

intent: >
  Provide a modern, actively-maintained web rendering engine for building UI
  in Cluiche applications. Replaces the deprecated DiaUIAwesomium system.
  Supports multi-page rendering, custom URL schemes, JavaScript bridging,
  and offscreen rendering for game engine integration.

responsibilities:
  - Implement IUISystem and IPage interfaces using CEF
  - Manage CEF multi-process lifecycle (browser, renderer, GPU processes)
  - Offscreen rendering with BGRA-to-RGBA conversion
  - Custom dia:// URL scheme for loading local assets
  - JavaScript bridge via window.dia object and IPC
  - Mouse and keyboard input injection
  - Multi-page support with isolated browser instances
  - Chrome DevTools integration for debugging (Debug builds only)

non_responsibilities:
  - UI layout and styling (done in HTML/CSS/React)
  - Application logic
  - Network requests beyond what CEF handles internally
  - 3D graphics rendering

dependent_modules: []

public_api:
  headers:
    - Dia/DiaUICEF/CEFUISystem.h
    - Dia/DiaUICEF/CEFPage.h
    - Dia/DiaUICEF/CEFUtils.h
  namespaces:
    - Dia::UICEF
  entry_points:
    - CEFUISystem

dependencies:
  required:
    - dia.ui
    - dia.core.core
    - dia.core.memory
    - dia.core.strings
    - dia.core.containers
  optional:
    - dia.window.interface
  forbidden:
    - dia.uiawesomium
    - dia.uiultralight
---
