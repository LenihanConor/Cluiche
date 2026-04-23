# Feature Spec: Splash Screen

## Traceability

| Level | Name | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | CluicheEditor | @docs/specs/applications/cluicheeditor.md |
| System | *(Host application — no system layer)* | N/A |
| Feature | **Splash Screen** | (this document) |

## Problem Statement

CluicheEditor's React shell has a multi-step async initialization sequence (panel RPC, layout load) during which the window is visible but content is absent. Before this feature the only feedback was a plain `"Loading panels…` text string, giving no branding and no clear sense of progress. Additionally, there is a gap between the native Win32 window appearing and CEF/React being ready (~300ms) where the user sees a blank dark frame. The splash screen eliminates both problems: a borderless native Win32 splash window displays the logo immediately at launch, then hands off to a React overlay which fades out once the docking layout is ready.

## Acceptance Criteria

- [ ] A borderless native splash window appears immediately when the editor process starts, before CEF initialises
- [ ] The native splash displays `splash-logo.bmp` centered and scaled to fit the window
- [ ] The native splash is destroyed and the editor window is shown only after React signals it is ready (`shell_ready`)
- [ ] A full-viewport React splash overlay appears in the editor window as the first rendered frame
- [ ] The React splash displays a PNG logo (`splash-logo.png`) centered on screen
- [ ] The React splash displays an animated CSS spinner beneath the logo
- [ ] The React overlay fades out (CSS opacity transition, 400 ms) once `DockingManager` finishes initialising (panels fetched + layout loaded or error recovered)
- [ ] After the fade completes the overlay is removed from the DOM (no layout impact)
- [ ] The splash does not block keyboard shortcuts (Ctrl+Z / Ctrl+Y / Ctrl+Shift+P) registered in `App`
- [ ] The logo PNG canonical source is `Cluiche/Assets/CluicheEditor/splash-logo.png`; a BMP copy `splash-logo.bmp` lives alongside it for Win32 GDI; a PNG copy lives at `Cluiche/CluicheEditor/UI/src/assets/splash-logo.png` for Vite import

## Design

### Boot Sequence

```
T=0ms    Borderless splash window created → logo BMP painted → VISIBLE
T=50ms   Editor Win32 window created (SW_HIDE — hidden)
T=60ms   CEF initialises inside hidden editor window
T=150ms  HTML parses, JS bundle starts executing
T=300ms  React mounts, SplashScreen component rendered
         → JS fires "shell_ready" event via EditorBridge
         → C++: DestroyWindow(mSplashHwnd), ShowWindow(editor, SW_SHOW)
         → Editor window appears already showing React splash
T=500ms  Panels fetched + layout loaded → onReady() → React splash fades (400ms)
T=900ms  React splash gone, editor fully interactive
```

### Native Splash Window (C++)

A borderless `WS_POPUP | WS_VISIBLE | WS_EX_TOPMOST` Win32 window is created in `EditorViewModule::DoStart()` before any other work. Its `WndProc` handles `WM_PAINT` by loading `Assets\CluicheEditor\splash-logo.bmp` via `LoadImageW` and painting it scaled to the window via `StretchBlt`. If the BMP is not found it fills with `RGB(30,30,30)` (editor background colour) so there is never a white flash.

The editor window is created immediately after but kept hidden (`ShowWindow(SW_HIDE)`). A `"shell_ready"` event handler is registered on `WebUIBridge`. When JS fires this event, C++ destroys the splash window and calls `ShowWindow(SW_SHOW)` on the editor window.

### React Splash Component

`SplashScreen.tsx` is a full-viewport fixed overlay (`position: fixed; inset: 0; z-index: 9999`). It:
1. Renders with `opacity: 1` while the `visible` prop is `true`
2. Transitions to `opacity: 0` over 400 ms when `visible` becomes `false`
3. Delays unmounting by 400 ms to let the fade complete before removing from DOM

```
┌─────────────────────────────────────┐
│  background: #1e1e1e                │
│                                     │
│          [splash-logo.png]          │
│             128 × 128 px            │
│                                     │
│               ◎  (spinner)          │
│                                     │
└─────────────────────────────────────┘
```

Spinner: a 36 × 36 px `<div>` with a CSS border trick — three sides `#3a3a3a`, top border `#a6e22e` (Monokai green), animated by `@keyframes dia-spin` (`rotate(360deg)`, `0.8s linear infinite`). No external dependency.

### Asset layout

```
Cluiche/Assets/CluicheEditor/
    splash-logo.png          ← canonical master PNG (replace with final art)
    splash-logo.bmp          ← 24-bit BMP for Win32 GDI (converted from PNG)

Cluiche/CluicheEditor/UI/src/assets/
    splash-logo.png          ← copy for Vite import (keep in sync with master)
```

The BMP is copied to `$(TargetDir)Assets\CluicheEditor\` by a post-build `xcopy` step in `CluicheEditor.vcxproj`. When replacing the logo with final art, update both `splash-logo.png` locations and re-convert to BMP.

### Data flow

```
EditorViewModule (C++)
  ├─ Creates native splash window (T=0ms)
  ├─ Creates editor window hidden
  └─ Registers "shell_ready" handler
         │
         ▼
App (main.tsx)
  splashVisible: boolean = true
        │
        ├─ useEffect([]) → EditorBridge.shellReady()
        │    → C++: destroy native splash, ShowWindow(editor)
        │
        ├─► <SplashScreen visible={splashVisible} />
        │
        └─► <DockingManager onReady={() => setSplashVisible(false)} />
                   │
                   └── fires onReady() in .finally() of panel+layout fetch
```

### Implementation files

| File | Change |
|------|--------|
| `Cluiche/Assets/CluicheEditor/splash-logo.bmp` | New — 24-bit BMP converted from PNG |
| `Cluiche/Assets/CluicheEditor/splash-logo.png` | Existing canonical logo asset |
| `Cluiche/CluicheEditor/UI/src/assets/splash-logo.png` | Existing Vite build copy |
| `Cluiche/CluicheEditor/UI/src/components/SplashScreen.tsx` | Existing — React overlay component |
| `Cluiche/CluicheEditor/UI/src/layout/DockingManager.tsx` | Edit — add `onReady` prop; call in init `.finally()` and `.catch()` |
| `Cluiche/CluicheEditor/UI/src/main.tsx` | Edit — add `splashVisible` state, `<SplashScreen>`, `shellReady` signal, `onReady` wiring |
| `Cluiche/CluicheEditor/UI/src/bridge/EditorBridge.ts` | Edit — add `shellReady()` event |
| `Cluiche/CluicheEditor/ApplicationFlow/Modules/EditorViewModule.h` | Edit — add `mSplashHwnd`, `mSplashWidth/Height`, helper methods |
| `Cluiche/CluicheEditor/ApplicationFlow/Modules/EditorViewModule.cpp` | Edit — native splash creation, SW_HIDE editor, shell_ready handler |
| `Cluiche/CluicheEditor/CluicheEditor.vcxproj` | Edit — post-build xcopy for Assets/CluicheEditor/ |

## Binding Decisions Compliance

| Source | ID | Decision Summary | Compliance |
|--------|----|--------------------|------------|
| Platform | PD-001 | Use StringCRC for IDs | **Compliant** — `"shell_ready"` registered via `Dia::Core::StringCRC` |
| Platform | PD-002 | PU/Phase/Module architecture | **Compliant** — splash logic lives inside `EditorViewModule` (a Module) |
| Platform | PD-003 | Component-based entities | **N/A** — no new engine entities |
| Platform | PD-004 | No STL containers in public APIs | **Compliant** — no new public C++ API surface |
| Platform | PD-005 | x64 Windows only | **Compliant** — Win32 GDI is x64 Windows; frontend JS is platform-agnostic |
| Platform | PD-006 | VS project files are source of truth | **Compliant** — `CluicheEditor.vcxproj` updated with post-build xcopy |
| Platform | PD-007 | C++20 required | **Compliant** — no C++ standard changes |
| Platform | PD-008 | Directory.Build.props owns build output | **Compliant** — no MSBuild output path changes |
| CluicheEditor | AED-001 | CluicheEditor owns application flow; DiaEditor is pure library | **Compliant** — native splash is host application code in `EditorViewModule`; DiaEditor library unchanged |
| CluicheEditor | AED-002 | Plugins via .diaapp manifest | **N/A** — splash is not a plugin |
| CluicheEditor | AED-003 | Systems own their editors in `<System>/Editor/` | **N/A** — not a system plugin |
| CluicheEditor | AED-004 | Editor connects via WebSocket | **N/A** — splash has no network behaviour |
| CluicheEditor | AED-005 | React + DiaUICEF + react-mosaic | **Compliant** — React splash is a React component rendered inside the same CEF window |
| CluicheEditor | AED-006 | `.cluicheproj` is top-level project file | **N/A** — splash is independent of project loading |

**All binding decisions: COMPLIANT or N/A**

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | Timing | Should the splash wait for project load (C++ side) in addition to panel/layout fetch? | No — dismiss on panel+layout ready | No — project loading is opaque to the React shell; the splash covers the visually broken period only |
| 2 | Logo format | PNG or SVG for React? BMP for native? | PNG + BMP | PNG for React (Vite import), 24-bit BMP for Win32 GDI (LoadImageW, no alpha needed) |
| 3 | Spinner library | Use an npm package or pure CSS? | Pure CSS | Pure CSS — no dependency, consistent with project's minimal-dependency approach |
| 4 | Fade duration | 400 ms — too long / too short? | 400 ms | 400 ms is a comfortable minimum; feels polished without being slow |
| 5 | Z-index collision | Could `z-index: 9999` conflict with `CommandPalette` or other overlays? | No conflict | CommandPalette only opens after a keypress — the splash will already be gone |
| 6 | Error path | Does the React splash always dismiss if the backend is unreachable? | Yes | Yes — `onReady` is called in both `.finally()` and `.catch()` paths |
| 7 | Keyboard shortcuts | Can the user trigger Ctrl+Shift+P during the splash? | Technically yes | `pointerEvents: none` on faded-out overlay; keyboard handlers in `App` unaffected |
| 8 | Asset sync | What keeps `UI/src/assets/` in sync with `Cluiche/Assets/CluicheEditor/`? | Manual for now | Manual copy on art update; post-build xcopy handles runtime BMP deployment |
| 9 | Native splash dismiss race | What if `shell_ready` fires before the editor window is fully composited? | `ShowWindow` is synchronous | `ShowWindow(SW_SHOW)` is synchronous; CEF child window is already parented and sized inside it |
| 10 | BMP missing at runtime | What happens if the BMP file is not found by `LoadImageW`? | Dark fill fallback | `SplashWndProc` fills with `RGB(30,30,30)` — same as the editor background, no white flash |

## Status

`In Progress`
