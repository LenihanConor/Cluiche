---
schema: dia.module.v1
module_id: dia.sfml
name: SFML
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaSFML
language: cpp
parent_module_id: dia.root

summary: >
  Window creation and input event handling via SFML. Provides IWindow and IInputSource
  implementations (Window, WindowFactory, InputSource) backed by sf::Window.
  No rendering — DiaBgfx owns the render surface.

intent: >
  Provide an SFML-backed window and input adapter. Keep SFML's graphics surface entirely
  out of scope; the rendering path belongs to DiaBgfx.

responsibilities:
  - Create and manage a native OS window via sf::Window (IWindow implementation)
  - Poll and translate SFML window/keyboard/mouse events to Dia::Input::Event (IInputSource)
  - Expose Win32WndProcChain for DIA_DEBUG consumers that need WndProc intercept

non_responsibilities:
  - Rendering (ICanvas — owned by DiaBgfx)
  - Texture loading or decoding (owned by DiaAssetRuntime)
  - Audio or font handling
  - SDL/Win32 window migration (future research)

public_api:
  headers:
    - Dia/DiaSFML/Window.h
    - Dia/DiaSFML/WindowFactory.h
    - Dia/DiaSFML/InputSource.h
    - Dia/DiaSFML/Win32WndProcChain.h
  namespaces:
    - Dia::SFML
  entry_points:
    - Window
    - WindowFactory
    - InputSource
    - Win32WndProcChain  # DIA_DEBUG only

dependencies:
  required:
    - dia.core.containers.bitflag
    - dia.core.memory
    - dia.core.strings
    - dia.input
    - dia.sfml.sfml.system
    - dia.sfml.sfml.window
    - dia.window.interface
  forbidden:
    - dia.graphics
    - dia.bgfx
---
