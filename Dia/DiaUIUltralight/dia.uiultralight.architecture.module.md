---
schema: dia.module.v1
module_id: dia.uiultralight
name: UIUltralight
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaUIUltralight
language: cpp
parent_module_id: dia.root

summary: >
  Ultralight-based implementation of IUISystem. Drop-in replacement for DiaUIAwesomium.

intent: >
  Provide a maintained, lightweight HTML/CSS/JS UI renderer for Cluiche applications by
  implementing the IUISystem interface using the Ultralight SDK. Replaces the deprecated
  Awesomium SDK (EOL ~2017) with an actively maintained WebKit-based renderer that is
  lighter than CEF and free for indie-scale projects.

responsibilities:
  - Implement Dia::UI::IUISystem using Ultralight as the rendering backend
  - Initialise the Ultralight Platform (FileSystem, FontLoader, Logger, Config)
  - Create and manage an Ultralight Renderer and View
  - Load HTML pages from file:// URLs derived from Dia::Core::FilePath
  - Bind C++ BoundMethod callbacks to the JavaScript 'app' global object via OnDOMReady
  - Convert BoundMethodArgs ↔ JSArgs for JS→C++ and C++→JS value passing
  - Render the current page to a CPU pixel buffer and expose it via UIDataBuffer
  - Forward mouse and scroll input events to the Ultralight View
  - Deploy Ultralight runtime DLLs alongside the executable via pre-build xcopy

non_responsibilities:
  - HTML/CSS/JS content authoring (owned by the consuming application)
  - GPU-accelerated rendering (CPU renderer only; no GPUDriver implementation)
  - Keyboard input injection (not required by existing IUISystem interface)
  - Window handle management (owned by DiaWindow)
  - Clipboard support (not required by existing IUISystem interface)

dependent_modules: []

public_api:
  headers:
    - Dia/DiaUIUltralight/UltralightUISystem.h
  namespaces:
    - Dia::UI::Ultralight
  entry_points:
    - UISystem

dependencies:
  required:
    - dia.core.core
    - dia.core.memory
    - dia.core.strings
    - dia.core.log
    - dia.ui
    - dia.window.interface
  forbidden:
    - dia.uiawesomium  # separate implementation; must not mix
---
