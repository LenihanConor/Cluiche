---
module_id: dia.sdl
display_name: DiaSDL
parent: dia
layer: platform-adapter
version: "1.0"
status: complete

dependencies:
  required:
    - dia.core
    - dia.window
    - dia.input
  forbidden:
    - dia.bgfx
    - dia.graphics

public_api:
  headers:
    - DiaSDL/Window.h
    - DiaSDL/WindowFactory.h
    - DiaSDL/InputSource.h
  namespaces:
    - Dia::SDL
  entry_points:
    - Dia::SDL::WindowFactory::Create
    - Dia::SDL::WindowFactory::Destroy

responsibilities:
  - SDL3 window creation and lifecycle (SDL_Init / SDL_Quit in WindowFactory)
  - IWindow implementation via SDL_Window
  - IInputSource implementation via SDL_PollEvent
  - SDL_Keycode → Dia::Input::EKey translation (lookup table, private)
  - Cross-platform native window handle extraction via SDL_GetPointerProperty

non_responsibilities:
  - Touch input (deferred to follow-on feature)
  - Audio (out of scope)
  - Platform entry-point bootstrap (SDL_main / SDL_UIKitRunApp)
  - CluicheEditor window (Win32Window stays)
---

# DiaSDL

SDL3-based window and input backend for Dia engine. Replaces DiaSFML on all game targets (Windows, Linux, Android, iOS).

`WindowFactory::Create` calls `SDL_Init(SDL_INIT_VIDEO)` and returns a `Window` that implements both `IWindow` and `IInputSource`. `WindowFactory::Destroy` calls `SDL_Quit`.

No SDL module or global init object is needed — lifecycle is tied to the window factory call-site (KernelModule).
