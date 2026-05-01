# System Spec: DiaUIUltralight

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaUIUltralight is a lightweight HTML/CSS/JS UI renderer for Cluiche applications, implementing the `IUISystem` interface using the [Ultralight SDK](https://ultralig.ht). Ultralight is a WebKit-based CPU renderer designed specifically for embedding in games and native applications — significantly smaller than CEF (~15 MB vs ~300 MB) while retaining full HTML5/CSS3/JavaScript support.

DiaUIUltralight implements the `IUISystem` contract, meaning consuming code (`MainUIModule`, `LaunchUIPage`, `DummyUIPage`) requires no changes to integrate.

## Responsibilities

- **IUISystem Implementation** — Implement `Initialize`, `LoadPage`, `UnloadPage`, `Update`, `FetchUIDataBuffer`, and all five mouse input methods
- **Platform Initialisation** — Set up Ultralight `Platform` singleton with `FileSystem`, `FontLoader`, `Logger`, and `Config` before creating the `Renderer`
- **Page Loading** — Convert `Dia::Core::FilePath` to a `file://` URL and load it into the Ultralight `View`; block until the page finishes loading
- **JS Binding** — Register all `BoundMethod` entries from a `Page` as callbacks on the JavaScript `app` global object during `OnDOMReady`
- **Pixel Buffer Readback** — Lock the `BitmapSurface`, copy pixels into a staging buffer, and expose via `UIDataBuffer`
- **Input Forwarding** — Map `Dia::Input::EMouseButton` to Ultralight `MouseEvent` and `ScrollEvent`
- **DLL Deployment** — Pre-build xcopy of `AppCore.dll`, `Ultralight.dll`, `UltralightCore.dll`, `WebCore.dll` to the executable directory
- **Logging** — Forward Ultralight log messages and console output to `Dia::Core::Log`

## Public Interfaces

### Core Class

```cpp
namespace Dia::UI::Ultralight {

    class UISystem : public IUISystem {
    public:
        UISystem(const Window::IWindow* windowContext);
        virtual ~UISystem();

        virtual void Initialize() override;

        virtual void LoadPage(Page& newPage) override;
        virtual void UnloadPage() override;
        virtual bool IsPageLoaded() const override;

        virtual void Update() override;
        virtual void FetchUIDataBuffer(UIDataBuffer& outBuffer) const override;

        virtual void InjectMouseMove(int x, int y) override;
        virtual void InjectMouseDown(EMouseButton button, int x, int y) override;
        virtual void InjectMouseUp(EMouseButton button, int x, int y) override;
        virtual void InjectMouseClick(EMouseButton button, int x, int y) override;
        virtual void InjectMouseWheel(int scroll_vert, int scroll_horz) override;
    };

}
```

### Internal Implementation (pimpl — not part of public API)

`UISystemImpl` owns the Ultralight objects and implements `LoadListener` / `ViewListener`:

| Class | Purpose |
|---|---|
| `UISystemImpl` | Owns `Renderer`, `View`; implements `LoadListener`, `ViewListener` |
| `DiaFileSystem` | Ultralight `FileSystem` — maps paths to disk via `fopen_s` |
| `DiaLogger` | Ultralight `Logger` — forwards to `Dia::Core::Log` |

### JavaScript API (injected at DOM-ready time)

```javascript
// 'app' global object created per-page; methods are bound to C++ BoundMethod entries.
// Example for LaunchUIPage:
app.Application_LaunchLevel("DummyLevel");

// Example for DummyUIPage:
app.Application_ExitLevel();
```

## Dependencies

### Required (Dia modules)
- **DiaUI** — `IUISystem`, `Page`, `BoundMethod`, `BoundMethodArgs`, `UIDataBuffer`
- **DiaCore** — `Assert`, `Memory`, `Log`, `FilePath`, `String64`
- **DiaWindow** — `IWindow` (for initial viewport dimensions)
- **DiaInput** — `EMouseButton`

### External
- **Ultralight SDK** (`External/Ultralight/`) — headers in `include/`, import libs in `lib/`, runtime DLLs in `bin/`
  - `Ultralight.lib` / `Ultralight.dll`
  - `UltralightCore.lib` / `UltralightCore.dll`
  - `WebCore.lib` / `WebCore.dll`
  - `AppCore.lib` / `AppCore.dll` (provides `GetPlatformFontLoader`, `JSHelpers`)
  - License: free for projects under $100K/year revenue

### Build
- Visual Studio 2022 (v143 toolset), C++17, x64, StaticLibrary
- Include path: `$(SolutionDir)../External/Ultralight/include/`

## Non-Responsibilities

- **HTML/CSS/JS content** — owned by the consuming application
- **GPU-accelerated rendering** — no `GPUDriver` implementation; CPU renderer only
- **Keyboard input** — not in the current `IUISystem` interface
- **Multi-view / multi-window** — single `View` per `UISystem` instance
- **Clipboard** — not required by `IUISystem`
- **Window management** — owned by `DiaWindow`

## Related Systems

| System | Relationship |
|---|---|
| DiaUI | Interface provider (`IUISystem`, `Page`, `BoundMethod`) |
| DiaUICEF | Alternative implementation (heavier, editor-focused) |
| DiaUICEF | Alternative future implementation (heavier, editor-focused) |
| CluicheTest | Primary consumer (`MainUIModule`) |

## Inherited Binding Decisions

| Source | ID | Decision | Impact on DiaUIUltralight |
|---|---|---|---|
| Platform | PD-001 | C++ as primary language | Implementation in C++17; JS accessed only through Ultralight API |
| Platform | PD-002 | Windows as primary platform | x64 build; Ultralight x64 SDK used |
| Platform | PD-003 | Visual Studio + MSBuild | Built as `.vcxproj` in Cluiche solution |
| Platform | PD-004 | Spec-driven development | This spec documents completed implementation |
| Dia | AD-001 | Module YAML frontmatter | `dia.uiultralight.architecture.module.md` created |
| Dia | AD-002 | No STL in public APIs | `UISystem` public API uses only Dia types; STL confined to `UISystemImpl` internals |
| Dia | AD-003 | Namespace `Dia::<Module>::` | All classes in `Dia::UI::Ultralight::` |

## System-Specific Decisions

| ID | Decision | Rationale | Status | Binding |
|---|---|---|---|---|
| UL-001 | CPU renderer only (`is_accelerated = false`) | No `GPUDriver` needed; pixel buffer maps directly to existing `UIDataBuffer` pattern | Accepted | Yes |
| UL-002 | Single `View` per `UISystem` | `MainUIModule` only ever uses one page at a time | Accepted | Yes |
| UL-003 | `OnDOMReady` for JS binding | Bindings registered after DOM is ready | Accepted | Yes |
| UL-004 | `file://` URL scheme for local assets | `DiaFileSystem` maps paths to disk | Accepted | Yes |
| UL-005 | 16 MB staging buffer for pixel readback | Sufficient for all current window sizes | Accepted | Yes |
| UL-006 | `GetPlatformFontLoader()` (AppCore) | Avoids implementing a custom font loader; relies on Ultralight's Windows system font integration | Accepted | Yes |
| UL-007 | Transparent background by default (`is_transparent = true`) | UI composited over game scene; matches existing IUISystem compositing contract | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced on all features · `No` = guidance only

## Features

| Feature | Description | Spec | Status |
|---|---|---|---|
| Platform Initialisation | Set up Ultralight Platform singleton (FileSystem, FontLoader, Logger, Config, Renderer) | [platform-initialisation.md](../../features/dia/diauiultralight/platform-initialisation.md) | Approved |
| Page Loading | Load HTML pages from FilePath, block until loaded, clear on unload | [page-loading.md](../../features/dia/diauiultralight/page-loading.md) | Approved |
| JavaScript Binding | Bind BoundMethod entries to `app` JS object via OnDOMReady; convert args bidirectionally | [javascript-binding.md](../../features/dia/diauiultralight/javascript-binding.md) | Approved |
| Pixel Buffer Readback | Lock BitmapSurface, copy to staging buffer, expose via UIDataBuffer each frame | [pixel-buffer-readback.md](../../features/dia/diauiultralight/pixel-buffer-readback.md) | Approved |
| Input Injection | Map EMouseButton + coordinates to Ultralight MouseEvent and ScrollEvent | [input-injection.md](../../features/dia/diauiultralight/input-injection.md) | Approved |
| Game UI Framework Convention | Tiered JS framework convention (Alpine + React) for game panels; removes Webix; bridge contract tests | [game-ui-framework-convention.md](../../features/dia/diauiultralight/game-ui-framework-convention.md) | Approved |

## AI Review Questions

| # | Section | Question | Answer |
|---|---|---|---|
| 1 | Replacement | Why Ultralight over CEF? | Ultralight is ~15 MB vs CEF ~300 MB; free under $100K revenue; simpler init (no subprocess); sufficient for in-game UI |
| 2 | Rendering | Why CPU renderer? | Matches existing UIDataBuffer pixel copy pattern; no GPUDriver complexity; GPU mode would require DiaGraphics integration |
| 3 | JS Binding | Why `OnDOMReady` vs `OnWindowObjectReady`? | DOM must be ready for `app` object assignment; `OnWindowObjectReady` fires before DOM elements exist |
| 4 | Threading | Is UISystem thread-safe? | Yes — outer `UISystem` uses `std::mutex` on all public methods |
| 5 | File Loading | How does `DiaFileSystem` resolve paths? | Calls `FilePath::Resolve()` to get absolute path, converts `\` to `/` for URL, then `fopen_s` for file reads |
| 6 | Font Loading | Why `GetPlatformFontLoader()`? | Windows has system fonts; avoids embedding font files; AppCore provides this for free |
| 7 | Staging buffer | Why a fixed 16 MB staging buffer? | Avoids heap allocation per frame; 16 MB covers 2048×2048 BGRA; consistent with the UIDataBuffer pixel copy pattern |
| 8 | DLL deployment | Why xcopy in pre-build? | Ensures DLLs are next to the exe before the linker runs |
| 9 | Migration | What changes to CluicheTest for Step 2? | Only `MainUIModule.h`: change include and instantiation to `Ultralight::UISystem` |
| 10 | Backwards compat | Can multiple UI backends coexist? | Yes — separate namespaces and projects; CluicheTest can switch at any time |

## Status

`Done` — Implementation complete. Step 2 (CluicheTest migration) is a separate task.
