# Research Summary — Render Backend Swap (replace SFML)

**Session folder:** docs/research/render_backend_swap/
**Date:** 2026-05-17

## One-Line Answer

Adopt **bgfx** as a new `Dia/DiaBgfx/` render module replacing the SFML render path, delivered in two phases (parity, then light 3D) with all phase specs written up-front and Phase 1 implemented first.

## Journey

1. **Explored:** Surveyed the render-backend space (high-level engines like Filament/Magnum, mid-level GPU layers like bgfx/Diligent/SDL_GPU, thin layers like Sokol, native D3D11/D3D12) against the seams in DiaGraphics (ICanvas, FrameData, SpriteDrawCommand, DebugPrimitive, IShader, ITexture) and the binding platform decisions (PD-001 StringCRC, PD-004 no STL in public APIs, PD-005 x64 Windows, PD-007 C++20).
2. **Ideated:** Generated 10 candidates spanning thin (Sokol) → mid (bgfx, Diligent, SDL_GPU) → high (Filament, Magnum) → native (D3D11, D3D12) → do-nothing (SFML 3.0 upgrade), plus a phased bgfx variant (Candidate 10).
3. **Evaluated:** Scored candidates on Engine Value / Game Value / Cost / Risk / Cluiche Fit. **bgfx phased = 4.35** ranked first; Filament finished last (2.40) — its full PBR pipeline is overkill for the stated "light 3D, no photorealism" target.
4. **Chose:** bgfx phased confirmed by user, with explicit architectural commitments to *not* mirror DiaSFML's structural mistakes.

## Chosen Work Item

**Name:** Render Backend Swap — bgfx phased
**Home module:** new `Dia/DiaBgfx/` (Phase 1); Phase 2 adds `DiaMesh3D`, `DiaRig3D`, `DiaAnimation3D`, `DiaSkinning3D`, `DiaScene3D`
**Suggested spec type:** **System** (one parent system spec) with **13 child feature specs** (6 Phase 1 + 7 Phase 2)
**Estimated size:**
- Phase 1: **L** (1–2 months) — implement now
- Phase 2: **L** (1–2 months) — spec now, implement later
- Phase 3 (DiaTerrain): deferred, no specs yet

## Key Insights from Exploration

- **bgfx is a GPU abstraction, Filament is a renderer.** The gap between them is roughly 3–5 weeks of renderer engineering (sprite batch, debug batch, mesh, skinning, forward shader, shadow map, tone mapping, material params, shader cook). For "light 3D, no photorealism," that work is the *right* size and gives you control of art direction; Filament's free features are mostly the ones the brief doesn't need.
- **DiaSFML conflates renderer + window + input** because SFML does. DiaBgfx must not — it is `ICanvas` only. `IWindow` and `IInputSource` stay in DiaSFML for now (a future SDL migration is captured as separate research).
- **The visitor pattern in DiaSFML's render path adds indirection without payoff.** DiaBgfx consumes `FrameData` directly via internal sub-renderers (Sprite/Debug/UIOverlay).
- **Resource handles need a StringCRC upgrade.** DiaSFML's `unsigned int` texture IDs are a PD-001 regression worth fixing in this swap.
- **UI overlay path needs extraction.** Today the UI shader lives in `DiaSFML::RenderWindow`. Phase 1 extracts a renderer-agnostic UI overlay surface in `DiaUI`, with `DiaBgfx` as the first implementer — future-proofs DiaUICEF / DiaUIUltralight.
- **Async texture loading sequencing matters.** Finish async loading on top of `ITexture` *first*, then DiaBgfx implements `ITexture` and async lights up on bgfx for free. Inverting the order forces the async work to handle both backends mid-flight.
- **DiaCLI env must learn bgfx.** `dia env setup` and `dia env verify` need to acquire and validate prebuilt bgfx libs — its own small feature spec.
- **Phase 1 has a hard ship gate:** SFML render path deletable, all visual debuggers + tests green on bgfx, before Phase 2 starts.

## Discarded Candidates

| Candidate | Why discarded |
|-----------|---------------|
| Filament | Full PBR + IBL + post chain are exactly the features the brief does **not** need; XL integration cost |
| Diligent Engine | Modern API design appealing but smaller community than bgfx, no clear advantage for "light 3D" |
| Sokol | No D3D12/Vulkan, smaller ecosystem, every feature hand-written without bgfx's draw-bucket scaffolding |
| D3D11 native | No multi-backend insurance; viable Phase 1 fallback if bgfx integration disappoints |
| D3D12 native | XL bring-up cost on a brief that doesn't need explicit-API perf ceiling |
| SDL_GPU | Tightly coupled to a future SDL window/input migration that is out of scope |
| Magnum | Smaller community, higher Risk for an unfamiliar engine paradigm |
| SFML 3.0 + custom 3D | Doesn't solve the stated goal; postpones it and accumulates GL2-era technical debt |
| bgfx single milestone | Same backend, but bundles parity + 3D into one push; loses the clean SFML-removal ship gate |

## Spec Map (deliverable from this research)

**One system spec:** `docs/specs/systems/render_backend.md`

**Phase 1 feature specs (implement now, 6 total):**
1. `bgfx-env-setup` — DiaCLI env install + verify for bgfx
2. `diaui-render-overlay-surface` — extract renderer-agnostic UI overlay from DiaSFML into DiaUI
3. `diabgfx-canvas-parity` — `DiaBgfx::Canvas` implements ICanvas with sprite + debug + UI parity
4. `diabgfx-imgui-backend` — IImGuiBackend impl on bgfx
5. `diabgfx-texture-pipeline` — ITexture refactor to StringCRC handles + bgfx impl (coordinates with async loader)
6. `diasfml-render-removal` — delete render path from DiaSFML, keep window+input

**Phase 2 feature specs (spec now, implement later, 7 total):**
1. `diagraphics-3d-types` — Camera3D, Light, Mesh3DDrawCommand, Mesh3DFrameData added to DiaGraphics
2. `diamesh3d` — new module, glTF 2.0 static mesh loading
3. `diarig3d` — new module, skeleton/joint hierarchy
4. `diaanimation3d` — new module, glTF animation curves + sampling
5. `diaskinning3d` — new module, vertex skinning + GPU skinning data
6. `diascene3d` — new module, scene-graph (Camera + Lights + Renderables)
7. `diabgfx-3d-renderers` — mesh, skinned mesh, simple forward + shadow on bgfx

**Phase 3 (deferred):** `DiaTerrain` — heightmap terrain, specced when Phase 2 is closing.

## References

- docs/research/render_backend_swap/explore.md
- docs/research/render_backend_swap/ideate.md
- docs/research/render_backend_swap/evaluate.md
- docs/research/render_backend_swap/choose.md
