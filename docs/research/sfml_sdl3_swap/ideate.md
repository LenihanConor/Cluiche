# Research: Ideate — SFML → SDL3 Swap (Window + Input)

**Input:** docs/research/sfml_sdl3_swap/explore.md

## Candidates

### Candidate 1: DiaSdl Minimal — Window + Input, 4 Platforms
**Home module/system:** New `Dia/DiaSdl/` module (sibling to DiaSFML)
**Size:** M

Replace DiaSFML with a new DiaSdl module that implements the same `IWindow` + `IInputSource` contracts using SDL3. Single module, all four target platforms (Windows/Linux/Android/iOS) gated with `#ifdef` inside one set of source files — no per-platform subclasses needed since SDL3 already abstracts the OS. Remove `IWindow::SetActive(bool)` from the interface. Port `Win32WndProcChain` to use SDL3's `SDL_SetWindowsMessageHook` (Windows-only, DIA_DEBUG guard). Delete DiaSFML once DiaSdl is wired in.

Touch events are **not** added — `SDL_EVENT_FINGER_*` events fall through silently (logged as unhandled). This keeps scope tight and leaves touch for a follow-on feature. `SystemHandle.h` gets the missing Linux (`Window*` / `xcb_window_t`) and iOS (`UIView*`) typedefs.

**Primary value:** CluicheTest and future game applications compile and run on Android, iOS, Linux, and Windows from a single code path.

---

### Candidate 2: DiaSdl + Touch Input
**Home module/system:** New `Dia/DiaSdl/` + changes to `Dia/DiaInput/`
**Size:** M

Everything in Candidate 1, plus: add `kTouchBegan`, `kTouchMoved`, `kTouchEnded` to `Dia::Input::Event::EType`, add a `TouchEvent` struct to `Dia::Input::Event`, and translate `SDL_EVENT_FINGER_*` events in DiaSdl's InputSource. No consumer changes required — existing game code simply ignores the new event types.

Adding touch now costs ~1 extra day and avoids a future DiaInput API change after mobile targets are live. The risk is scope creep: touch input has subtleties (pressure, multi-finger, coordinate normalisation) that may pull in more work than expected.

**Primary value:** Mobile targets ship with usable touch input on day one, not as a retrofit.

---

### Candidate 3: DiaSdl + SDL_main Platform Bootstrap
**Home module/system:** New `Dia/DiaSdl/` + new `Dia/DiaSdl/Platform/` sub-folder
**Size:** M

Everything in Candidate 1, plus a thin platform entry-point adapter so that CluicheTest's `main()` does not need to be `#ifdef`-wrapped per platform. On Android, SDL3 requires the native activity to call into SDL's JNI shim; on iOS it requires `SDL_UIKitRunApp`. DiaSdl provides a `DiaSdl::Platform::Bootstrap()` helper and the required `SDL_main`/`SDL_SetMainReady` wiring so application `main.cpp` stays unchanged.

Without this, every application's entry point needs platform-specific scaffolding. With it, adding a third or fourth application later (a new game) is a copy-paste of the existing `main.cpp`.

**Primary value:** Application entry points remain platform-neutral; porting a new game to mobile requires no entry-point surgery.

---

### Candidate 4: DiaSdl Full (C1 + C2 + C3)
**Home module/system:** New `Dia/DiaSdl/` + `Dia/DiaInput/` + entry-point bootstrap
**Size:** L

Combines Candidates 1, 2, and 3 into a single deliverable: new DiaSdl module with window, input, touch, and platform bootstrap. This is the "ship everything at once" option. The upside is a single spec and a single PR. The downside is that touch and bootstrap are speculative until someone actually runs on Android — discovering a design problem mid-way blocks the entire deliverable.

**Primary value:** One spec, one PR, full mobile readiness from day one.

---

### Candidate 5: In-Place Rename — DiaSFML → DiaSdl
**Home module/system:** Existing `Dia/DiaSFML/` renamed to `Dia/DiaSdl/`
**Size:** M

Same implementation work as Candidate 1 but delivered by modifying the existing DiaSFML project files rather than creating a new module alongside. Rename the folder, rename the `.vcxproj`, update all `#include` paths. Simpler from a project-management perspective (no parallel module wiring), but git history shows a rename rather than a deletion+creation — easier to blame later.

The risk is that DiaSFML is referenced by name in several documentation files, the module registry, and potentially build scripts; a rename touches more files than creating a sibling. Also loses the ability to run DiaSFML alongside DiaSdl during a transition period (if bugs appear, you can't easily fall back).

**Primary value:** Slightly simpler project structure; no orphaned DiaSFML module to clean up.

---

### Candidate 6: Win32WndProcChain → DiaBgfx (Standalone Cleanup)
**Home module/system:** `Dia/DiaBgfx/` (move 2 files from DiaSFML)
**Size:** S

`Win32WndProcChain` exists to let the bgfx ImGui backend intercept Win32 messages. It is semantically a DiaBgfx concern, not a window-backend concern — it was only in DiaSFML because that was the only window implementation at the time. Move `Win32WndProcChain.h/.cpp` into `Dia/DiaBgfx/` (or a `DiaBgfx/Imgui/` sub-folder), remove the DiaSFML dependency on it.

This can be done independently of the SDL3 swap and actually unblocks a cleaner DiaSdl design (DiaSdl never needs Win32WndProcChain at all; DiaBgfx uses `SDL_SetWindowsMessageHook` on its own). It is the lowest-risk candidate and could land first as a preparatory step.

**Primary value:** DiaBgfx owns its own debug hook infrastructure; DiaSdl ships with zero Win32 debt.

---

### Candidate 7: Parallel Backends — Keep DiaSFML + Add DiaSdl
**Home module/system:** Both `Dia/DiaSFML/` and new `Dia/DiaSdl/` coexist
**Size:** L

Add DiaSdl for game targets while keeping DiaSFML alive for any Windows-only path. The application selects a backend at startup via a factory parameter. This is the safest migration path — if DiaSdl has a bug on Windows, the editor or a debug build can fall back to DiaSFML instantly.

The cost is maintaining two implementations forever (or until DiaSFML is deleted in a follow-on PR). In practice, once DiaSdl is working on Windows there is no reason to keep DiaSFML — the "parallel for safety" argument collapses after the first successful release. Likely adds 1–2 weeks of wiring complexity for a benefit that is only realised if DiaSdl ships broken.

**Primary value:** Zero regression risk on Windows during the SDL3 migration window.

---

### Candidate 8: DiaWindow Unification — Collapse DiaSdl into DiaWindow
**Home module/system:** `Dia/DiaWindow/` (major expansion)
**Size:** XL

Rather than maintaining DiaSFML/DiaSdl as a separate adapter layer, bring all concrete window backends (Win32Window, SDL3Window) directly into DiaWindow as platform sub-folders. DiaWindow becomes the single module that owns both the interface (`IWindow`) and all implementations. The separate DiaSFML and DiaSdl modules disappear.

This is architecturally cleaner — there is no longer a question of "which window module do I link?" — but it requires restructuring DiaWindow, updating every downstream `.vcxproj` that links DiaSFML, and deciding how DiaWindow handles the SDL3 init/quit lifecycle (currently a ProcessingUnit-level concern). The Win32Window already lives in DiaWindow, so there is precedent, but extending it to carry SDL3 makes DiaWindow a heavyweight module with a C library dependency.

**Primary value:** Single window module, no adapter indirection, clear ownership of all window backends.

---

## Coverage Map

The candidates span the full design-axis range from the explore phase:

- **Scope range:** S (C6 cleanup) → M (C1/C2/C3/C5 core swap) → L (C4/C7 full or parallel) → XL (C8 unification)
- **Module strategy axis:** New sibling module (C1–C4), in-place rename (C5), both coexist (C7), full merge (C8)
- **Touch input axis:** Excluded (C1/C3/C5/C6/C7/C8), included (C2/C4)
- **Platform bootstrap axis:** Excluded (C1/C2/C5/C6/C7/C8), included (C3/C4)
- **Architectural cleanup axis:** C6 is a standalone prep step that is orthogonal to all others; C8 is the maximalist cleanup
- **Risk axis:** C6 is near-zero risk; C7 is lowest migration risk; C8 is highest design risk
