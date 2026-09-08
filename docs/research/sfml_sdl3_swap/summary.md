# Research Summary — SFML → SDL3 Swap (Window + Input)

**Session folder:** docs/research/sfml_sdl3_swap/
**Date:** 2026-05-26

## One-Line Answer

Replace DiaSFML with a new DiaSDL module that implements the same `IWindow` + `IInputSource` contracts using SDL3, enabling CluicheTest and future games to compile and run on Windows, Linux, Android, and iOS.

## Journey

1. **Explored:** DiaSFML uses only 2 source files and SFML's Window subsystem — no audio, no graphics, no network. The only remaining SFML dependency is window creation and input event polling; swapping it is well-bounded. Three separate window paths exist in the codebase (DiaSFML for games, Win32Window for the editor, SplashScreenModule raw Win32) — only DiaSFML needs to change.
2. **Ideated:** 8 candidates generated, ranging from S (Win32WndProcChain cleanup) to XL (collapse all backends into DiaWindow); scope spanned minimal swap, touch input, platform bootstrap, parallel backends, and full architectural unification.
3. **Evaluated:** C1 (DiaSdl Minimal) scored 4.15 — highest on cost and fit; the only candidate that achieves cross-platform capability without untestable speculative work.
4. **Chose:** C1 confirmed as DiaSDL Minimal; touch and bootstrap deferred until mobile build environments exist; Win32WndProcChain relocation bundled as a mandatory prep commit.

## Chosen Work Item

**Name:** DiaSDL Minimal — Replace DiaSFML with SDL3 window + input backend
**Home module:** New `Dia/DiaSDL/` (sibling to and replacement for `Dia/DiaSFML/`)
**Suggested spec type:** Feature (under a new DiaSDL system spec, or directly as a feature under the Dia application)
**Estimated size:** M (1–3 weeks)

## Key Insights from Exploration

- **Nothing is lost.** No audio, no SFML Graphics, no network — DiaSFML was only ever SFML Window. The swap has no functional regressions.
- **C6 is not optional.** `Win32WndProcChain` must move into DiaBgfx before DiaSDL is built — it is semantically a bgfx/ImGui concern, not a window concern. Bundle it as commit 1.
- **SDL3 key codes are not a cast.** `SDL_Keycode` values are UTF-32-like codepoints, not a dense enum. A lookup table is required to translate to `Dia::Input::EKey` — this is the only non-trivial implementation work.
- **Three dead `EType` entries in DiaInput** (`kMouseWheelMoved`, `kMouseEntered`, `kMouseLeft`) were never mapped in DiaSFML. Grep for consumers before deleting — if none, remove them in this spec.
- **`IWindow::SetActive(bool)` is dead API.** Removing it from the interface is in-scope and unblocks a cleaner DiaSDL and Win32Window.
- **`SystemHandle.h` is already multi-platform aware** — it has `WIN32` and `ANDROID_OS` guards. Linux and iOS typedefs are missing and must be added as part of this spec.
- **`SDL_Init`/`SDL_Quit` must live in a Module**, not in Window ctor/dtor — required by PD-002 (ProcessingUnit/Phase/Module lifecycle).

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| C2 DiaSdl + Touch | Touch coordinate space (normalised vs pixel) is a design decision best made against a real mobile target; deferred |
| C3 DiaSdl + Platform Bootstrap | SDL_main / iOS UIKitRunApp can't be verified without mobile toolchains; deferred until mobile builds start |
| C4 DiaSdl Full | C2 + C3 risks combined; both deferred |
| C5 In-Place Rename | Rename ripples across registry, docs, build scripts; harder to review; loses fallback safety net during transition |
| C7 Parallel Backends | Two implementations with no long-term payoff; deleted once DiaSDL verified |
| C8 DiaWindow Unification | XL scope, high design risk, disproportionate to the benefit |

## References

- docs/research/sfml_sdl3_swap/explore.md
- docs/research/sfml_sdl3_swap/ideate.md
- docs/research/sfml_sdl3_swap/evaluate.md
- docs/research/sfml_sdl3_swap/choose.md
