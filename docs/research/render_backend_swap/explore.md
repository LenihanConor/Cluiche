# Research: Explore — Render Backend Swap (replace SFML)

**Session date:** 2026-05-17
**Folder:** docs/research/render_backend_swap/
**Constraint focus:** Prioritize 3D-capable backends (future-proof for 3D)

## Problem Space Overview

Cluiche currently uses **SFML** as both its windowing/input layer and its 2D draw backend. SFML wraps OpenGL 2.x by default, exposes a fixed-function-style draw API, and bundles window, image, audio, and input under one umbrella. Inside the engine, `Dia::Graphics::ICanvas` is the abstraction that game code talks to; `DiaSFML::RenderWindow` is the concrete `ICanvas` (and also `IWindow` and `IInputSource`). Frame data is accumulated each tick into `FrameData` (sprites, debug primitives, UI) and replayed by an SFML visitor.

The goal is to replace the SFML render backend — and the question is whether to swap in a 3D-capable engine like **Google Filament** (a full PBR forward+ renderer) or a thinner GPU abstraction like **bgfx**, **Diligent Engine**, **Sokol**, or a hand-written D3D12/Vulkan layer. The choice determines (a) whether Cluiche is committing to a deferred/forward HDR pipeline now, or (b) keeping rendering as a thin GPU portability layer and letting higher-level systems compose pipelines.

This decision is load-bearing because the current 2D-primitive use cases (sprite batching, debug overlays, UI compositing, ImGui) must keep working, while the *new* axis — being able to add 3D scenes, PBR materials, post-processing, and shadows later — is what motivates the swap in the first place.

## Existing Approaches

- **High-level renderers (engine-grade):** Google Filament (PBR, forward+, IBL), Ogre3D, Magnum, Open3D
- **Mid-level GPU abstractions:** bgfx (multi-backend, render-bucket model), Diligent Engine (D3D12/Vulkan/Metal/WebGPU, modern API style), The-Forge (AAA-leaning, modern API)
- **Thin GPU portability layers:** Sokol (single-header, GLES3-style), WebGPU/wgpu-native (Rust core, C API, browser-aligned), Dawn (Google's WebGPU impl)
- **Native API direct:** D3D12 (Windows-first, matches PD-005), Vulkan (cross-platform, verbose), D3D11 (mature, simpler)
- **Hybrid 2D+3D libraries:** raylib (raylib's 3D mode), Allegro 5, SDL_GPU (new in SDL3)
- **Stay with extended SFML:** SFML 3.0 has modernized internals; pair with a dedicated 3D lib (e.g. bgfx-on-SFML-window) only for 3D scenes

## Design Axes

| Axis | Options | Notes |
|------|---------|-------|
| **Abstraction level** | High-level renderer / mid-level GPU / thin portability / raw API | Higher = faster to PBR, less control; lower = more code, more flexibility |
| **GPU backend coverage** | D3D11 / D3D12 / Vulkan / OpenGL / Metal / WebGPU | PD-005 makes Windows-only acceptable; D3D12 + Vulkan covers debug tooling like RenderDoc/PIX |
| **Window & input ownership** | Renderer owns window / engine owns window (DiaWindow) | Filament expects a native handle; SFML currently owns. Decoupling is required either way |
| **2D path strategy** | Renderer treats 2D as ortho 3D / dedicated 2D pipeline / two backends side-by-side | Filament has no native 2D primitives — sprites/debug lines become quads/line-list meshes |
| **Threading model** | Single-threaded / explicit render thread / command-buffer recording / fiber | Cluiche already has a Render ProcessingUnit; backend must not fight existing threading |
| **Asset pipeline coupling** | Renderer-owned (gltf, .filamesh) / engine-owned (DiaAssetCatalogue) / hybrid | Filament has its own offline tools (matc, cmgen, filamesh); bgfx ships shaderc; Sokol ships sokol-shdc |
| **Shader language** | HLSL / GLSL / SPIR-V / WGSL / MSL / abstracted (Filament .mat, bgfx varying.def) | Affects pipeline tooling, hot-reload, debugging |
| **ImGui integration** | First-class backend / DIY | All viable backends have community ImGui examples; verify quality |
| **Build & deps weight** | Header-only / static lib / DLL / submodule with own toolchain | Filament builds via CMake + ninja and pulls many deps; Sokol is single-header |
| **License** | MIT / Apache 2.0 / BSD / proprietary | Most candidates are permissive; verify before commitment |

## Known Tradeoffs

- **High-level renderer (Filament)**: fastest path to "looks AAA," but you inherit a full pipeline (deferred/forward+, IBL, bloom, TAA) — hard to opt out of, hard to do unconventional 2D, and hard to bend toward stylized/non-PBR looks
- **Mid-level GPU (bgfx)**: production-proven, broad backend support, strong debug tooling — but "view + draw bucket + uniform" model is its own paradigm to learn, and shader pipeline (shaderc with `varying.def.sc`) is opinionated
- **Thin layer (Sokol/wgpu)**: maximum control, smallest dep, easiest to audit — but you build *every* renderer feature yourself; no PBR, no post stack, no shadow shipping for free
- **Native API direct (D3D12)**: best perf ceiling on Windows, matches PD-005, no third-party rot risk — but verbose, slow to bootstrap, and pinned to one OS
- **2D-on-3D**: a 3D-capable backend forces you to express sprites and debug lines as ortho-camera meshes; this is fine but adds indirection compared to SFML's direct `draw(sf::CircleShape)` model
- **Renderer owns window vs engine owns window**: Filament expects to own the swapchain via a native handle, which means `DiaWindow` would create the OS window and hand the HWND to the renderer — clean, but requires DiaSFML to stop being the IWindow

## Known Pitfalls (C++ / game engine context)

- **API churn**: Vulkan/D3D12 each have multi-year migration costs; if backing a thin abstraction you pay this yourself, if backing a high-level engine you absorb its abstraction churn
- **Shader pipeline fragmentation**: getting one shader to run across D3D12 + Vulkan reliably is a non-trivial offline-tooling problem (DXC, glslang, SPIRV-Cross). Filament hides this; bgfx hides this; thin layers do not
- **Editor + game in same process**: CluicheEditor and CluicheTest both render — backend must support multi-window/multi-swapchain. Filament does, bgfx does (multi-view), but check carefully
- **ImGui regression**: SFML's ImGui backend currently works; the new backend's ImGui story has to ship at the same time or debug overlays die
- **Asset hot-reload regression**: existing texture/sprite hot-reload (if any) tied to SFML loaders must be preserved or replaced
- **Debug overlay perf**: `DebugFrameData` can emit thousands of lines/circles per frame (physics + AI viz). Backend must allow dynamic vertex buffers / immediate-mode-style line drawing without per-line draw calls
- **Build time + dep weight**: Filament's CMake build is heavy; pulling it in via vcxproj will require either a prebuilt redistributable or a parallel CMake build step that DiaCLI orchestrates
- **License/pipeline tools redistribution**: Filament's `matc` is needed at asset cook time; pipeline runners (DiaPipelineEditor) must invoke it
- **`sf::Event`-shaped input**: `DiaSFML::InputSource` translates SFML events into `IInputSource`. If the backend swap also takes the window, input has to be re-sourced — likely from `DiaWindow` directly
- **C++20 + module compatibility**: PD-007 requires `/std:c++20`. Filament builds in its own translation units, so this is fine, but its public headers must compile cleanly in C++20 mode

## Cluiche-Specific Opportunities

### Relevant Existing Modules

| Module | Relevance |
|--------|-----------|
| **DiaGraphics** | Owns `ICanvas`, `FrameData`, `SpriteDrawCommand`, `DebugPrimitive`, `IShader`, `ITexture` — this is the seam the new backend plugs into |
| **DiaSFML** | The current concrete backend — a sibling `DiaFilament` (or `DiaBgfx`, `DiaD3D12`, etc.) replaces it |
| **DiaWindow** | Already has `IWindow` interface; currently implemented by SFML. After swap, a non-SFML `IWindow` (e.g. Win32 directly, GLFW, or DiaWindow native) owns the HWND |
| **DiaInput** | `IInputSource` is already abstract; `DiaSFML::InputSource` is one impl. New backend means new `IInputSource` impl from the new windowing path |
| **DiaImGui** | `IImGuiBackend` exists; new backend must ship a matching ImGui adapter at parity |
| **DiaAssetCatalogue / DiaAssetRuntime** | Texture/shader/material assets currently feed SFML loaders; need new loaders for the chosen backend |
| **DiaApplicationFlow** | Render ProcessingUnit lifecycle calls `ICanvas::StartFrame/ProcessFrame/EndFrame` — should remain unchanged |
| **DiaVisualDebugger family** | Heavy users of `DebugFrameData` — performance regressions here are highly visible |
| **DiaUI / DiaUICEF / DiaUIUltralight** | UI is composited via `UIFrameData` → `RenderWindow`'s UI shader. New backend needs an equivalent UI overlay path |

### Platform Decision Constraints

| Decision | Implication for this topic |
|----------|---------------------------|
| **PD-001 StringCRC** | New backend's resource handles (textures, shaders, materials) should use StringCRC IDs in DiaGraphics-facing layer, even if the backend internally uses pointers/indices |
| **PD-002 ProcessingUnit/Phase/Module** | Render ProcessingUnit owns the backend; backend init/shutdown lives in a Phase; multi-thread record/submit must respect Phase boundaries |
| **PD-003 Component system** | Sprite/Mesh/Light/Camera become IComponents in higher-level systems; the backend itself is below this — but its API must accept frame data assembled from components |
| **PD-004 No STL in public APIs** | Backend's *public* C++ API (the one DiaGraphics talks to) must not leak `std::vector` / `std::string`. Internally, a Filament/bgfx wrapper can use STL freely |
| **PD-005 x64 Windows only** | D3D12 is fully on the table; Metal is not needed; cross-platform is not a tiebreaker |
| **PD-006 VS project files source of truth** | Backend integration must work via `.vcxproj` references (with prebuilt libs from CMake if needed). No CMake-of-CMakes at top level |
| **PD-007 C++20 required** | Backend's headers must compile under `/std:c++20` cleanly — verify on each candidate |

## Open Questions for Ideation

- Is the goal to ship 3D *features* (lit meshes, IBL, shadows) within the next 6–12 months, or only to *not preclude* 3D? This determines whether a high-level renderer (Filament) is justified vs. a thin layer.
- Should the new backend also replace `IWindow` (DiaSFML currently owns it), or should DiaWindow get its own native Win32/GLFW implementation first as a prerequisite step?
- Does CluicheEditor (CEF-based) need the same backend instance as CluicheTest, or are they independent?
- Are there existing 3D assets / target art style commitments (gltf? PBR? stylized 2D-only?) that pre-bias the backend choice?
- How much of the existing SFML feature footprint (audio, font rendering, image loading) needs to be replaced separately vs. dropped? (`sf::Font`, `sf::SoundBuffer`, `sf::Image` are quietly load-bearing)
- Is the team willing to take on shader-pipeline tooling (shaderc / DXC / matc) as a first-class concern in DiaPipelineEditor?
- What is the perf budget impact tolerance for debug primitive drawing? Backends with poor immediate-mode/dynamic-buffer paths will hurt visual debuggers visibly.
