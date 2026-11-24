# Feature Spec: DiaBgfx3D Post-Process Stack (Feature D)

## Parent System
@docs/specs/applications/dia/systems/diabgfx3d/diabgfx3d.md (Amendment 2026-11-20)

**Gameplay Domains:** rendering
**Depends on:** Render Targets and Dynamic Textures (A)
**Needed by:** DraoiHexWorldRender3D Map Assembly; CoW Strategic Map Presentation Presets
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/alive.js` (`PostFX`, `POST_FS`)

## Status
`Approved` (2026-11-24, user, after spec review: bgfx depth range, `wobbleHz`, tweakable params with sidecar). Drafted 2026-11-20 from the approved prototype. **Needs AD-001 sign-off before build.** Revises the DiaBgfx3D module rule "post-processing: out of scope (RB-001)" (see below).

## Summary

One full-screen composite over the scene target's colour and depth, between the 3D scene and the UI. It provides the presentation effects the research approved:

- **Tilt-shift:** a sharp focus band with blur above and below, for a miniature diorama look.
- **Aerial haze:** distant geometry fades into a haze colour.
- **Sky:** pixels with no geometry get a painted gradient and soft cloud bands.
- **Depth silhouettes:** ink where a nearer surface overlaps a farther one (ridgelines, hill fronts).
- **Vignette.**

It is one shader with per-effect strengths, not four passes, so the cost is one full-screen draw plus the blur taps. A game can append its own pass after it.

## Problem

- Painted-map presentation needs the scene's depth and colour after rendering. There is no post-process capability (the module doc says so).
- The first silhouette test, a depth difference between neighbouring pixels, drew false black lines across flat water at oblique angles, because depth changes steadily across a flat surface. The fix is a test a flat surface can never trigger.

## Goals

1. `PostProcessStack`: runs after the scene views (into the target from Feature A) and before the UI views, drawing the composite to the backbuffer within a viewport rectangle (split-screen safe).
2. **Composite** with independent controls for tilt-shift, haze, sky, silhouettes and vignette; each off by default, and with everything off the output equals the input.
3. **Flat-surface-safe silhouettes** based on the second difference of `1/z`.
4. **Pluggable extra passes**: `IPostPass` appended after the composite, reading the same inputs.
5. Correct for perspective and orthographic cameras (sky, haze and silhouettes are skipped for orthographic unless asked).

## Non-Goals

- Bloom, colour grading LUTs, SSAO, motion blur, AA resolve. Out of scope (RB-001 stays for PBR, IBL and the heavy post effects).
- HDR tone mapping.
- Per-view different settings in one frame (one composite config per viewport rect).

## Design

```cpp
namespace Dia::Bgfx3D {
    struct TiltShiftParams { float strength = 0; float focusY = 0.42f; float bandHalfWidth = 0.12f; float maxBlurPx = 7; };
    struct HazeParams      { float strength = 0; float startDistance = 0; float scaleDistance = 1; uint32_t colour = 0; float maxBlend = 0.85f; };
    struct SkyParams       { bool enabled = false; uint32_t horizon = 0, zenith = 0, cloud = 0; float cloudAmount = 0.5f;
                             float cloudScale = 3.0f, cloudDriftSpeed = 0.02f, cloudEdgeInk = 0.5f; float depthThreshold = 0.99999f; };   // time comes from Draw
    struct SilhouetteParams{ bool enabled = false; uint32_t inkColour = 0xFF20242A; float threshold = 0.025f; float softness = 0.035f;
                             float wobblePx = 0.8f; float wobbleHz = 12.0f; };   // wobbleHz 0 = no wobble steps; frame = floor(timeSeconds * wobbleHz)
    struct CompositeParams { TiltShiftParams tilt; HazeParams haze; SkyParams sky; SilhouetteParams silhouette; float vignette = 0; };

    class IPostPass { public: virtual ~IPostPass() {}
                      virtual void Draw(const PostInputs&) = 0; };      // inputs: colour, depth, camera, rect
    class PostProcessStack {
    public:
        void SetComposite(const CompositeParams&);
        void AddPass(IPostPass*);                      // after the composite, in order
        void Draw(const Camera3D&, const RenderTarget& scene, const ViewportRect&, float timeSeconds);
    };
}
```

- **Depth linearisation.** The prototype (WebGL) had depth in [−1, 1]; bgfx on D3D11 has [0, 1] (`caps->homogeneousDepth` false), so the shader takes the convention from the caps. D3D11 perspective: `z = n·f / (f − d(f − n))`; orthographic: `z = n + d(f − n)`. The GL-range forms (`z = 2nf / (f + n − (2d − 1)(f − n))`, ortho `z = n + (d·0.5 + 0.5)(f − n)`) are used only when the caps say so.
- **Tweakable params.** `CompositeParams` and its parts are plain structs with a DiaValueSchema sidecar (ranges and defaults) shipped with DiaBgfx3D, bound by the consumer with `ParamBinding` from its own `AttributeSet`; no look number lives in the shader (the haze cap, sky cloud scale, drift speed and edge ink, and the sky depth threshold are fields above). Structural constants stay: the 16 blur taps and the 1/z silhouette formula.
- **Wobble clock.** `wobbleFrame = floor(timeSeconds × wobbleHz)` is derived inside `Draw` from the `timeSeconds` it already takes, so captures are deterministic from time alone (consistent with DraoiHexWorldRender3D's `boilHz`).
- **Tilt-shift.** The blur radius grows with distance of the pixel's vertical position from `focusY`, outside the band half-width, up to `maxBlurPx`. 16 golden-angle disc taps, sampling only inside the viewport rect.
- **Haze.** `t = 1 − exp(−max(0, z − startDistance) / scaleDistance)`; `colour = mix(colour, hazeColour, clamp(t × strength, 0, maxBlend))`. Not applied to sky pixels.
- **Sky.** Pixels with depth ≥ 0.99999 get a gradient from horizon to zenith by screen height, plus fBm cloud bands (value noise) and thin ink edges on the bands. Perspective only.
- **Silhouettes.** For each pixel, with `w = 1/z` of the pixel and `w₁`, `w₂` of the two pixels one step along an axis on either side: `lap = (2w − w₁ − w₂) / w`. A pixel *in front of* its neighbours gives `lap > 0`; the ink amount is `smoothstep(threshold, threshold + softness, lap)`, taken as the maximum over the horizontal and vertical axes. Because `1/z` is linear across any flat surface in screen space, `lap` is zero on water, plains and any plane at any angle. Sky neighbours count as `w = 0` (infinitely far). A per-frame wobble moves the sample offsets by up to `wobblePx` so ink redraws "on twos" (the caller increments `wobbleFrame` at 12 Hz). The ink is reduced where tilt-shift blur is strong.
- **Vignette.** `colour × (1 − strength × |uv − 0.5|²)`.
- **View order.** The composite draws in a new view placed after the scene views and before the UI views (`setViewOrder`), on the backbuffer, so views 4–7 draw over it.

## Acceptance Criteria

1. **Identity.** With every effect off, the composite output equals the scene colour (every pixel within 1/255).
2. **Tilt-shift.** Pixels inside the focus band are unchanged (within 1/255); pixels far from it differ from the scene (blur), and a one-pixel-wide test line fades with distance from the band, up to `maxBlurPx`. Blur never samples outside the viewport rect.
3. **Haze.** For pixels of increasing depth, blend toward the haze colour is monotonic non-decreasing, zero at or before `startDistance`, capped at `maxBlend` (default 0.85). Sky pixels are unaffected.
4. **Sky.** Only pixels with no geometry change, and the horizon-to-zenith gradient is monotonic with screen height. Orthographic cameras skip it by default.
5. **Flat surfaces get no ink.** A large flat plane viewed at 30°, 55° and 80° from the horizontal produces zero silhouette pixels (regression for the false water lines). Same for a gently sloped heightfield mesh.
6. **Real edges get ink.** A cube in front of a plane, and a hill ridge in front of far terrain, produce a continuous silhouette on the near object's outline, with ink only on the near side.
7. **Sky edge.** A hill in front of sky is outlined; the sky side is not.
8. **Wobble.** The silhouette moves by at most `wobblePx` between wobble frames and by zero when `wobblePx = 0` or `wobbleHz = 0`; two draws at times within the same `1/wobbleHz` step give identical output, and the same time always gives the same result.
9. **Vignette.** With strength > 0 corners are darker than the centre; with 0 nothing changes.
10. **Viewport rect.** Two composites in adjacent half-width rects (split screen) do not bleed into each other.
11. **UI untouched.** Views 4–7 (entity, debug 2D, UI, ImGui) draw unmodified on top of the composite.
12. **Custom pass.** An `IPostPass` added after the composite receives the composite's output and the same depth, runs after it, and can be removed.
13. **Cost (Debug, ballpark).** The composite with tilt-shift and silhouettes on at 1080p is recorded in the plan notes; if it is a large share of a frame, a half-resolution blur option is queued as a perf task (not a gate).
14. **Observability.** Log channel `BGFX`; profile scope `Post.Composite`; metric `bgfx.post_ms` (CPU submit).
15. **Depth convention.** A test depth buffer with known values (near plane 0, far plane 1, a mid value) linearises to the expected `z` within 1e-3 for perspective and orthographic cameras on the D3D11 path; the shader's convention switch follows `caps->homogeneousDepth`.
16. **Tweakable params.** Every `CompositeParams` field has a sidecar range and default; out-of-range values are clamped with one warning; changing `haze.maxBlend` or `sky.cloudScale` changes the next frame with no rebuild.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| RB-001 | bgfx abstraction; PBR, IBL, post-processing out of scope | **Revised by amendment (needs sign-off):** a single scene-composite pass (tilt-shift, haze, sky, silhouettes, vignette) is in scope. PBR, IBL, bloom, SSAO and colour grading stay out of scope |
| RB-006 | No backend types in DiaGraphics public surface | Stack and passes live in DiaBgfx3D |
| TMAP-005 | Per-cell channels, no global state | Not involved; this is presentation only |
| AD-001 | Engine API changes and module doc revision need sign-off | **Sign-off required**, including the `dia.bgfx3d.architecture.module.md` line edit |
| PD-004 | No STL in public APIs | Structs and raw pointers |

## Open Design Questions

1. **Half-resolution blur.** The 16-tap blur at full resolution may be the most expensive part at high resolutions. A half-resolution path trades a little crispness for cost. Decide from AC 13.
2. **Sky and the paper tone.** The sky colours are parameters, so the map can match the paper tone. Should the sky be fully skipped when the camera looks nearly top-down (no horizon visible)? v1: it simply never draws when no sky pixels exist.
3. **MSAA interaction** with Feature A's open question: silhouettes read depth, so multisampled depth needs a resolve first.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Composite shader skeleton, view order, identity, viewport rect | AC 1, 10, 11 | Not Started | sonnet | needs Feature A |
| 2 | Tilt-shift | AC 2 | Not Started | sonnet | |
| 3 | Haze and sky | AC 3, 4 | Not Started | sonnet | |
| 4 | Silhouettes (1/z second difference) and wobble | AC 5, 6, 7, 8 | Not Started | sonnet | the false-line regression test is mandatory |
| 5 | Vignette, `IPostPass`, ordering | AC 9, 12 | Not Started | sonnet | |
| 6 | Module doc revision, metrics, cost note | AC 13, 14 | Not Started | haiku | |
| 7 | Depth convention from caps, `CompositeParams` sidecar and clamping | AC 15, 16 | Not Started | sonnet | do with task 1 |
