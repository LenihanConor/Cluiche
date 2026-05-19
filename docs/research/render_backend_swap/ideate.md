# Research: Ideate — Render Backend Swap (replace SFML)

**Input:** docs/research/render_backend_swap/explore.md

**Bias from Step 2 answers:**
- Light 3D within 6–12 months: terrain, static + skinned models, animation. **Not** photoreal. → favours mid-level GPU + simple custom forward renderer over a full PBR engine.
- **Keep SFML for window/input** for now (SDL migration is a separate research session). → backend must accept a foreign GL context or sit on an HWND that SFML created. No window/input ownership change in this work item.

## Candidates

### Candidate 1: bgfx-backed `DiaBgfx` render module
**Home module/system:** new `Dia/DiaBgfx/` (sibling of DiaSFML), implements `Graphics::ICanvas`
**Size:** L (1–2 months)
**Description:** Adopt **bgfx** as the GPU abstraction. bgfx supports D3D11/D3D12/Vulkan/OpenGL backends, has multi-view rendering, draw-bucket sorting, dynamic vertex buffers (great for `DebugFrameData`), and a mature ImGui integration. SFML keeps owning the window — bgfx attaches to the existing HWND via `bgfx::PlatformData::nwh`. We write a thin `DiaBgfx::Canvas` that consumes `FrameData` and submits to bgfx views, plus a `shaderc`-driven asset cook step for `.sc` shaders feeding into DiaPipelineEditor. Light 3D (mesh + skinned anim + simple forward lighting + shadow maps) is built on top by us — bgfx provides the GPU layer, not the renderer.
**Primary value:** Production-proven multi-backend GPU abstraction with strong debug-draw ergonomics and ImGui parity, leaving rendering features (PBR/non-PBR, shadows) under our control.

### Candidate 2: Diligent Engine-backed `DiaDiligent` render module
**Home module/system:** new `Dia/DiaDiligent/`, implements `Graphics::ICanvas`
**Size:** L (1–2 months)
**Description:** Use **Diligent Engine** — a modern GPU abstraction modelled on D3D12/Vulkan with C++ object API and uniform shader source via Diligent's HLSL→SPIR-V compiler. Cleaner architecture than bgfx for forward-looking 3D, slightly less battle-tested for 2D-heavy games. Coexists with SFML window via `IRenderDevice` + native HWND swapchain. We build the renderer (sprite batch, debug primitive batch, mesh, skinned mesh) on top.
**Primary value:** Modern, explicit-API design that ages well into D3D12/Vulkan-native engines without the legacy GL idioms baked into bgfx.

### Candidate 3: Sokol-backed `DiaSokol` render module
**Home module/system:** new `Dia/DiaSokol/`, implements `Graphics::ICanvas`
**Size:** M (1–3 weeks)
**Description:** Adopt **sokol_gfx** — a single-header GLES3-style portable GPU layer (D3D11/GL/Metal/WebGPU). Smallest possible dep footprint. SFML stays as window+input; Sokol attaches to SFML's GL context. We build sprite + debug + mesh paths ourselves. Shader pipeline via `sokol-shdc` (HLSL/GLSL/MSL/WGSL output). Light 3D is achievable but every renderer feature is hand-written.
**Primary value:** Lowest integration cost, easiest to audit and own — at the price of building all renderer features ourselves with no community shortcut for things like shadow mapping.

### Candidate 4: Filament-backed `DiaFilament` render module
**Home module/system:** new `Dia/DiaFilament/`, implements `Graphics::ICanvas`
**Size:** XL (>2 months)
**Description:** Adopt **Google Filament** — a full PBR forward+ renderer with IBL, bloom, TAA, and offline material/shader tooling (`matc`, `cmgen`). SFML keeps the window; Filament's `Engine` attaches to the SFML-owned HWND via `nativeWindow`. 2D primitives become ortho-camera meshes; debug overlays go through Filament's `DebugRegistry` or a custom unlit material. Asset pipeline gains `matc` and `cmgen` invocations from DiaPipelineEditor.
**Primary value:** AAA-quality lit rendering "for free" — at the cost of accepting Filament's pipeline opinions and absorbing significant integration effort for features the brief explicitly does not need (photorealism, IBL).

### Candidate 5: D3D11 native `DiaD3D11` render module
**Home module/system:** new `Dia/DiaD3D11/`, implements `Graphics::ICanvas`
**Size:** L (1–2 months)
**Description:** Hand-write a D3D11 renderer that owns its own swapchain on the SFML-created HWND. Uses HLSL natively (no transpile step), integrates cleanly with PIX and RenderDoc, and matches PD-005 perfectly. We own the entire stack: sprite batch, debug primitive batch, mesh draw, skinned mesh, simple forward lighting, shadow map. Mature, well-documented Microsoft API; future bump to D3D12 is its own future-research project.
**Primary value:** Single-vendor, single-API simplicity with zero third-party renderer surface — ages predictably and matches Windows-only platform constraint exactly.

### Candidate 6: D3D12 native `DiaD3D12` render module
**Home module/system:** new `Dia/DiaD3D12/`, implements `Graphics::ICanvas`
**Size:** XL (>2 months)
**Description:** Same shape as Candidate 5 but on **D3D12**. Modern explicit-API: descriptor heaps, command lists, fences, root signatures. Significantly more bring-up cost than D3D11 (you write the resource binding model yourself), but better matched to current GPU hardware and where Microsoft is investing.
**Primary value:** Best long-term performance ceiling on Windows with no abstraction tax — but the bring-up cost is hard to justify for "light 3D" requirements.

### Candidate 7: SDL_GPU-backed `DiaSDLGPU` render module
**Home module/system:** new `Dia/DiaSDLGPU/`, implements `Graphics::ICanvas`
**Size:** M (1–3 weeks)
**Description:** **SDL_GPU** is SDL3's new portable GPU abstraction (D3D12/Vulkan/Metal). It would naturally pair with the planned SDL window/input migration — but using SDL_GPU *now* while SFML still owns the window is awkward (SDL_GPU expects an SDL window for swapchain creation in the standard path, with limited support for foreign HWND). Listed for completeness; tightly coupled to the parallel SDL research session.
**Primary value:** Aligns with future windowing/input migration to SDL — but the timing constraint (SFML stays for now) makes this a poor fit *today*.

### Candidate 8: SFML 3.0 in-place upgrade + custom 3D layer
**Home module/system:** keep `Dia/DiaSFML/`, add `Dia/DiaSFML3D/` for a sibling 3D path
**Size:** S (≤1 week) for SFML 3 upgrade alone; M total with 3D experimentation layer
**Description:** Stay on SFML — bump to SFML 3.0 for modernized internals — and bolt a thin custom OpenGL renderer onto the same SFML GL context for 3D scenes. Cheapest path; no new dep. But it doesn't actually replace SFML; it just postpones the decision and accumulates technical debt because SFML's GL2-era abstractions limit how modern the 3D layer can be.
**Primary value:** Nearly zero migration risk and fastest start — at the cost of *not* solving the underlying problem (the user's stated goal is to replace SFML's 2D primitive renderer, not extend it).

### Candidate 9: Magnum Engine-backed `DiaMagnum` render module
**Home module/system:** new `Dia/DiaMagnum/`, implements `Graphics::ICanvas`
**Size:** L (1–2 months)
**Description:** **Magnum** is a modular C++ graphics engine with strong scene-graph, mesh, shader, and 2D+3D support. Backends: GL/GLES/WebGL/Vulkan. Its API is C++-idiomatic and modular ("import only what you need"). Coexists with SFML's window via custom `Platform::*Application`-free integration. Less mainstream than Filament/bgfx, smaller community, but interesting fit for "light 3D + 2D" because it explicitly supports both.
**Primary value:** First-class 2D + 3D support in a single, modular C++ engine — less re-implementation needed for sprite/debug paths than bare GPU layers.

### Candidate 10: Two-step bgfx adoption — Phase 1 parity, Phase 2 light 3D
**Home module/system:** new `Dia/DiaBgfx/`, two consecutive feature specs
**Size:** L total (M + M); deliverable in two clean milestones
**Description:** Same backend as Candidate 1, but explicitly framed as two sequential feature specs:
- **Phase 1 (M):** `DiaBgfx` reaches parity with `DiaSFML` for sprites + debug primitives + UI overlay + ImGui. SFML keeps window/input. Existing tests + visual debuggers go green on bgfx. SFML is then deletable from the render path.
- **Phase 2 (M):** add static mesh, skinned mesh + animation playback, simple directional-light forward shader, optional shadow map. Driven by a small `DiaMesh3D` + `DiaSkinning3D` module set.
This is a *delivery shape* variant of Candidate 1, not a different backend.
**Primary value:** Concrete, shippable parity milestone before any 3D risk is taken on; matches the user's "light 3D within 6–12 months" framing without making bring-up + 3D one giant work item.

## Coverage Map

The candidate list spans the design axes from explore.md as follows:

- **Abstraction level:** high-level (Filament-4, Magnum-9), mid-level (bgfx-1/10, Diligent-2, SDL_GPU-7), thin (Sokol-3), native (D3D11-5, D3D12-6), do-nothing (SFML-8)
- **Effort range:** S (SFML upgrade-8), M (Sokol-3, SDL_GPU-7, each phase of bgfx-10), L (bgfx-1, Diligent-2, D3D11-5, Magnum-9), XL (Filament-4, D3D12-6)
- **Risk range:** lowest (SFML-8), low-med (bgfx-1/10, D3D11-5, Sokol-3), med (Diligent-2, Magnum-9), high (Filament-4, D3D12-6, SDL_GPU-7-with-SFML-window)
- **Cluiche-fit:** all candidates respect PD-001/PD-004 at the public API layer; PD-005 means D3D11/D3D12 are perfectly aligned; PD-006 means each candidate must integrate via `.vcxproj` (with prebuilt libs where the source build uses CMake).

The set covers the realistic decision surface: do nothing (8), thin (3), mid (1/2/7/10), high (4/9), native (5/6).
