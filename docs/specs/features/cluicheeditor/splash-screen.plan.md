# Plan: Splash Screen

**Spec:** @docs/specs/features/cluicheeditor/splash-screen.md  
**Status:** In Progress  
**Started:** 2026-04-23  
**Last Updated:** 2026-04-23

## Tasks

| # | Task | Status | Notes |
|---|------|--------|-------|
| 1 | Native splash window + BMP paint (C++) | Done | `SplashScreenModule` created; `EditorViewModule` wired |
| 2 | Fix BMP rendering quality and artifacts | Done | See rendering fix notes below |
| 3 | React `SplashScreen.tsx` overlay + spinner | Not Started | |
| 4 | `DockingManager` `onReady` wiring | Not Started | |
| 5 | `main.tsx` `splashVisible` state + `shellReady` signal | Not Started | |

## Rendering Fix (Task 2) — Validation Notes

### What changed (`SplashScreenModule.cpp`)

```cpp
// Fix 1 — preserve 32-bit pixel data as DIB (prevents alpha byte corruption)
HBITMAP hbmp = (HBITMAP)LoadImageW(nullptr,
    L"Assets\\CluicheEditor\\splash-logo.bmp",
    IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

// Fix 2 — HALFTONE mode averages surrounding pixels when downscaling
SetStretchBltMode(hdc, HALFTONE);
SetBrushOrgEx(hdc, 0, 0, nullptr);   // required by GDI after HALFTONE
StretchBlt(...);
```

### Why each fix is needed

| Symptom | Root cause | Fix |
|---------|-----------|-----|
| Weird colour artifacts / washed-out hues | BMP is 32-bit; without `LR_CREATEDIBSECTION`, `LoadImageW` converts to a DDB matched to the screen DC, which can corrupt the 4th byte (alpha) into colour channels | `LR_CREATEDIBSECTION` keeps raw pixel data intact |
| Low fidelity / jagged edges | Default `StretchBlt` mode is `COLORONCOLOR` — drops pixels with no averaging when scaling 1112×641 → 560×323 | `HALFTONE` mode averages a neighbourhood of pixels, producing smooth downscale |

### How to validate

1. **Build** `CluicheEditor` (`Debug|x64`)
2. **Run** the editor — the native splash window should appear for ~300 ms before the React shell is ready
3. **Check against the asset** — open `Cluiche/Assets/CluicheEditor/splash-logo.bmp` in an image viewer alongside the running splash window
4. **Pass criteria:**
   - Colours match the source BMP (no green tint, no washed-out palette, no banding)
   - Edges and gradients appear smooth at 560×323 (no block/jagged artefacts from pixel-drop scaling)
5. **Regression:** dark-fill fallback still works — temporarily rename the BMP and confirm the splash shows a plain dark background with no crash

## Session Notes

### 2026-04-23
- Identified two root causes for distorted splash: 32-bit BMP alpha corruption via DDB conversion, and COLORONCOLOR pixel-drop downscale
- Applied `LR_CREATEDIBSECTION` and `HALFTONE` fixes to `SplashScreenModule.cpp`
- Plan file created; Tasks 3–5 (React side) not yet started
