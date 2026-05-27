# Plan: Replace DiaSFML with DiaSDL (Window + Input)

**Spec:** docs/specs/features/dia/diasdl/replace-diasfml-with-sdl3.md
**Status:** Not started

## Session Notes

**Spec decisions summary (Platform → App → Feature):**
Platform binds: PD-002 (ProcessingUnit/Module lifecycle), PD-004 (no STL in public APIs), PD-006 (VS project files), PD-007 (C++20), PD-008 (Directory.Build.props owns OutDir). App binds: AD-001 (module YAML doc), AD-003 (Dia::SDL:: namespace). Feature decisions: SDL_Init/Quit in WindowFactory (not a Module); Win32WndProcChain moves to DiaBgfx/Imgui/ via PowerShell; SDL3 as git submodule under External/SDL3/; Linux/iOS SystemHandle is void*; EKey lookup table private to DiaSDL; dead EType entries removed; IWindow::SetActive removed; all DiaSFML doc references patched on deletion.

**Critical callsite to fix (KernelModule.cpp:47):**
`static_cast<Dia::SFML::Window*>(mWindow)` is used to call `ListenForInputSources()` and access `ESources`. This cast must be replaced — `ListenForInputSources` + `ESources` should move onto `IInputSource` or DiaSDL::InputSource exposes a factory-level call. Decided: add `ListenForInputSources(BitArray8)` to `IInputSource` interface so callers never need to downcast.

**KernelModule also includes:**
- `<DiaSFML/Window.h>` — replace with `<DiaSDL/Window.h>`
- `<DiaSFML/InputSource.h>` — replace with `<DiaSDL/InputSource.h>`
- `<DiaSFML/WindowFactory.h>` — replace with `<DiaSDL/WindowFactory.h>`
- `Dia::SFML::WindowFactory mWindowFactory` → `Dia::SDL::WindowFactory mWindowFactory`
- `Dia::SFML::InputSource::ESources::kSystem` etc. → `Dia::SDL::InputSource::ESources::kSystem`

## Implementation Patterns

### Win32WndProcChain move (T-01)
- PowerShell: `Move-Item` the two files into `Dia/DiaBgfx/Imgui/`
- Update `DiaSFML.vcxproj`: remove ClInclude/ClCompile entries
- Update `DiaBgfx.vcxproj`: add ClInclude/ClCompile entries under `Imgui` filter (Release-excluded like the other Imgui files)
- Update `BgfxImGuiBackend.cpp` include: `<DiaSFML/Win32WndProcChain.h>` → `<DiaBgfx/Imgui/Win32WndProcChain.h>`
- Update namespace: `Dia::SFML::Win32WndProcChain` → `Dia::Bgfx::Win32WndProcChain` (or `Dia::Bgfx::Imgui::`)

### IWindow::SetActive removal (T-02)
- Remove pure virtual from `DiaWindow/Interface/IWindow.h`
- Remove stub from `DiaWindow/Win32Window.h/.cpp`
- Remove stub from `DiaSFML/Window.h/.cpp`
- Remove the `mCanvas->SetActiveContext(false)` call comment in KernelModule (bgfx owns the context, call stays on Canvas not Window)

### DiaInput EType cleanup (T-03)
- Grep `kMouseWheelMoved`, `kMouseEntered`, `kMouseLeft` across whole repo
- If no consumers: remove from `CLASSEDENUM` in `Event.h`; remove matching structs if any
- Add `ListenForInputSources(Dia::Core::BitArray8)` to `IInputSource` interface

### DiaSDL module structure (T-05)
```
Dia/DiaSDL/
├── dia.sdl.architecture.module.md
├── Window.h / Window.cpp          — implements IWindow via SDL_Window
├── WindowFactory.h / WindowFactory.cpp  — SDL_Init in Create, SDL_Quit in Destroy
├── InputSource.h / InputSource.cpp — implements IInputSource via SDL_PollEvent
├── SDLKeyMap.h                     — private lookup table SDL_Keycode → EKey
├── DiaSDL.vcxproj
└── DiaSDL.vcxproj.filters
```
Namespace: `Dia::SDL`
vcxproj type: `StaticLibrary` — inherits `bin/sharedlibs/` from Directory.Build.props
Include dirs: `External/SDL3/include`, `./../`, `./`, `$(ProjectDir)../../DIA/`
ProjectReferences: DiaCore, DiaWindow, DiaInput

### DiaSDL::InputSource pattern
```cpp
// Mirrors DiaSFML::InputSource exactly:
class InputSource : public Dia::Input::IInputSource {
    CLASSEDENUM(ESourceIndex, ...)   // same enum shape as SFML
    CLASSEDENUM(ESources, ...)
    void SetWindowContext(SDL_Window* window);
    void ListenForInputSources(Dia::Core::BitArray8) override;  // now on IInputSource
    void Poll(Dia::Input::EventData& outStream) override;
protected:
    virtual void OnRawSDLEvent(const SDL_Event& event) {}       // mirrors OnRawSFMLEvent
private:
    static Dia::Input::EKey SDLKeycodeToEKey(SDL_Keycode key);  // lookup table
};
```

### DiaSDL::Window pattern
```cpp
class Window : public Dia::Window::IWindow, public InputSource {
    // mirrors DiaSFML::Window exactly; SDL_Window* mWindowContext
    // GetSystemHandle(): uses SDL_GetPointerProperty with platform-specific key
    //   WIN32:   SDL_PROP_WINDOW_WIN32_HWND_POINTER
    //   Linux:   SDL_PROP_WINDOW_X11_WINDOW_NUMBER or SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER
    //   Android: SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER
    //   iOS:     SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER
};
```

### DiaSDL::WindowFactory pattern
```cpp
IWindow* WindowFactory::Create(const IWindow::Settings& settings) {
    SDL_Init(SDL_INIT_VIDEO);          // idempotent on repeated calls
    Window* w = DIA_NEW(Window(settings));
    w->Initialize(settings);
    return w;
}
void WindowFactory::Destroy(IWindow* window) {
    DIA_DELETE(window);
    SDL_Quit();
}
```

### KernelModule swap (T-08)
- Replace all `DiaSFML` includes with `DiaSDL` equivalents
- `Dia::SFML::WindowFactory` → `Dia::SDL::WindowFactory`
- Remove `Dia::SFML::Window* sfmlWindow = static_cast<...>(mWindow)` downcast
- Use `mWindow` (IWindow*) for window ops; cast to `DiaSDL::InputSource*` for `ListenForInputSources` or use the new IInputSource method
- `Dia::SFML::InputSource::ESources::*` → `Dia::SDL::InputSource::ESources::*`
- Win32WndProcChain include path already updated by T-01

### DiaSFML deletion (T-09)
- Delete `Dia/DiaSFML/` folder
- Remove DiaSFML from `Cluiche.sln`
- Remove DiaSFML ProjectReference from any vcxproj that has it
- Patch `render-backend.md`: update non-responsibilities section
- Patch `dia.md`: update Platform Dependencies + Out of Scope
- Patch `dia.sfml.architecture.module.md` — file is deleted with the folder

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| T-00 | Add SDL3 git submodule under `External/SDL3/`; add `dia env setup` SDL3 build step; update DiaEnv env manifest | `dia env verify` reports SDL3 present | Done | sonnet | SDL3 submodule at External/SDL3/; cmake_build entry in deps.json; cmake_build restore support in deps_restore.py |
| T-01 | PowerShell script: move `Win32WndProcChain.h/.cpp` → `Dia/DiaBgfx/Imgui/`; update DiaSFML.vcxproj, DiaBgfx.vcxproj, DiaBgfx.vcxproj.filters; rename namespace to `Dia::Bgfx`; update BgfxImGuiBackend.cpp include | Build passes, ImGui works in debug | Done | sonnet | Win32WndProcChain moved to DiaBgfx/Imgui/; namespace Dia::Bgfx; DiaSFML.vcxproj cleaned; DiaBgfx.vcxproj updated |
| T-02 | Remove `SetActiveContext(bool)` from ICanvas; remove from Bgfx::Canvas; remove all caller sites in KernelModule and RenderModule; update FakeCanvas test fixture | Build passes | Done | haiku | SetActiveContext removed from ICanvas interface, Bgfx::Canvas impl, KernelModule, RenderModule, FakeCanvas; Canvas dtor message updated |
| T-03 | Grep dead EType entries; remove `kMouseWheelMoved`/`kMouseEntered`/`kMouseLeft` from Event.h if no consumers; add `ListenForInputSources(BitArray8)` to `IInputSource` | Build passes | Done | haiku | AC-05; kMouseWheelMoved/kMouseEntered/kMouseLeft all have active consumers (InputState, LegacyEventConverter, tests) so entries retained; ListenForInputSources(BitArray8) added to IInputSource interface with default no-op impl; DiaSFML::InputSource updated with override keyword |
| T-04 | Add Linux (`void*`) and iOS (`void*`) typedefs to `SystemHandle.h` with comments | Build passes | Done | haiku | Linux (void*) and iOS (void*) SystemHandle typedefs added with comments |
| T-05 | Create `Dia/DiaSDL/` folder; scaffold `DiaSDL.vcxproj`, `DiaSDL.vcxproj.filters`, `dia.sdl.architecture.module.md`; add to `Cluiche.sln` | Project loads in VS | Done | haiku | Dia/DiaSDL/ created with vcxproj, filters, module doc; added to Cluiche.sln under Library folder with project ref deps (DiaCore, DiaWindow, DiaInput); project configs for Debug/Release/Debug-Asan/Debug-Ubsan |
| T-06 | Implement `DiaSDL::InputSource` — SDL_PollEvent loop, all event translations, EKey lookup table; implement `DiaSDL::Window` + `DiaSDL::WindowFactory` — SDL_Window creation, GetSystemHandle() per-platform extraction, SDL_Init/Quit in factory | Window opens on Windows, events flow | Done | sonnet | AC-01/AC-02/AC-07/AC-08/AC-09; InputSource (SDLKeycodeToEKey + TranslateSDLEvent public statics + Poll), Window, WindowFactory all implemented; vcxproj+filters updated |
| T-06b | Write `TestDiaSDL.cpp` — EKey lookup table round-trips + all AC-09 event type translations using synthetic SDL_Events; `DiaSDL/Testing/` following module test pattern | N tests GREEN `dia run googletest --filter="DiaSDL*"` | Done | sonnet | TestDiaSDL.cpp written with 18 RED tests covering EKey mapping (alpha/numeric/special/function/unknown) and all AC-09 event translations; will go GREEN when T-06 implements InputSource |
| T-07 | Implement `DiaSDL::Window` — all IWindow methods, GetSystemHandle() platform-conditional extraction | Window opens, resizes, closes correctly | Done | sonnet | AC-01/AC-02; part of T-06 deliverable; InputSource (SDLKeycodeToEKey + TranslateSDLEvent public statics + Poll), Window, WindowFactory all implemented; vcxproj+filters updated |
| T-08 | Update `KernelModule.h/.cpp`: replace all DiaSFML includes/types with DiaSDL equivalents; remove SFML downcast; use IInputSource::ListenForInputSources | CluicheTest runs end-to-end on Windows | Done | sonnet | AC-07/AC-08; KernelModule swapped to DiaSDL::WindowFactory; SFML downcast removed; IInputSource::ListenForInputSources used via ESourceIndex::SetBit; GetSystemHandle via IWindow*; DiaSDL ProjectReference added to CluicheGameBaseline.vcxproj |
| T-09 | Delete `Dia/DiaSFML/`; remove from Cluiche.sln; remove ProjectReferences; patch render-backend.md + dia.md | Full solution builds | Done | sonnet | AC-10; DiaSFML/ deleted; removed from sln + 4 vcxprojs; stale DiaBgfx comments patched; render-backend.md updated |
| T-10 | Run `dia run googletest` + `dia run cluichetest`; verify no regressions; update `dia.sdl.architecture.module.md` final content | All tests green, visual check passes | Todo | haiku | AC-13/AC-14 |
| T-10b | Observation scan — add `DIA_LOG_INFO` to `WindowFactory::Create`/`Destroy`; `DIA_LOG_DEBUG` for unrecognised SDL keycode fallthrough in `InputSource`; `DIA_TRACE_ZONE` on `InputSource::Poll` | Build passes | Todo | haiku | Feeds future DiaSDL domain instrumentation |
