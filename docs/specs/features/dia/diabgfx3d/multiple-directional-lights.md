# Feature Spec: DiaBgfx3D — Multiple Directional Lights

## Parent System
@docs/specs/applications/dia/systems/diabgfx3d/diabgfx3d.md

**Status:** `Approved`

---

## Summary

The current renderer hard-wires a single directional light: `Canvas3D` reads only `dirLights[0]` from `Mesh3DFrameData` and uploads two flat `vec4` uniforms to the shader. All remaining slots in the 32-entry `mDirectionalLights` array are silently ignored. This feature expands the shader and renderer to support up to 8 simultaneous directional lights (sun, key, fill, rim, and environment lights), using a static loop over array uniforms so the GPU cost scales with authored lights, not the array size.

---

## Goals

1. Up to 8 directional lights registered via `Mesh3DFrameData::AddDirectionalLight()` all contribute Lambert diffuse in `fs_mesh.sc`.
2. A named constant `kMaxDirLights = 8` is the single source of truth, shared between `MeshPassLighting.h` and the shader (`BGFX_CONFIG_MAX_DIR_LIGHTS` define injected at cook time, or a `bgfx_shader.sh` include constant).
3. Unused slots are zero-padded by `Canvas3D` before upload so the static shader loop produces zero contribution for inactive lights.
4. Shadows remain tied to `dirLights[0]` only — additional lights contribute diffuse only.
5. Existing single-light scenes are unaffected: a scene with one registered light produces identical visual output to the current renderer.
6. `MeshPassLighting` intermediate struct carries the expanded array data without exposing bgfx types (BG3-005).

---

## Constant Definition

`kMaxDirLights = 8` is defined in `MeshPassLighting.h` and referenced by:
- `Canvas3D.cpp` (loop bound when packing lights into `MeshPassLighting`)
- `MeshRenderer.cpp` (`bgfx::createUniform` `num` argument)
- `fs_mesh.sc` (array size and loop bound — injected as a `#define` or literal)

---

## Uniform Layout

Replace the two flat uniforms with array uniforms of length 8:

| Old uniform | New uniform | Type |
|---|---|---|
| `u_directionalLightDir` (vec4 × 1) | `u_dirLightDir` (vec4 × 8) | xyz = direction toward light, w = 0 |
| `u_directionalLightColour` (vec4 × 1) | `u_dirLightColour` (vec4 × 8) | rgb = colour, a = intensity |

`u_ambient` is unchanged — ambient is a single global term, not per-light.

---

## MeshPassLighting Changes

```cpp
// Before
struct MeshPassLighting {
    float dirLightDir[4];
    float dirLightColour[4];
    ...
};

// After
static constexpr int kMaxDirLights = 8;
struct MeshPassLighting {
    float dirLightDir[4 * kMaxDirLights];    // 8 × vec4, zero-padded
    float dirLightColour[4 * kMaxDirLights]; // 8 × vec4, zero-padded
    ...
};
```

---

## Shader Loop

```glsl
// fs_mesh.sc — replace the single-light diffuse block
vec3 diffuse = vec3_splat(0.0);
for (int i = 0; i < 8; i++) {
    vec3  dir      = -normalize(u_dirLightDir[i].xyz);
    float NdotL    = max(dot(normal, dir), 0.0);
    diffuse       += u_dirLightColour[i].rgb * u_dirLightColour[i].a * NdotL;
}
```

The loop is statically bounded. Unused slots have `dir = (0,0,0)` and `colour.a = 0`, so they contribute nothing. The driver unrolls the loop at compile time.

---

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Add `kMaxDirLights = 8` to `MeshPassLighting.h`; expand `dirLightDir` and `dirLightColour` to `float[4 * kMaxDirLights]` | Zero-initialise the arrays in the struct (= {} default). |
| 2 | Update `Canvas3D.cpp`: replace the `dirLights[0]` single-light read with a loop over all active lights up to `kMaxDirLights`; zero-pad remaining slots | Guard against `Mesh3DFrameData::GetDirectionalLights().Size() > kMaxDirLights` — log a warning and clamp. |
| 3 | Update `MeshRenderer::InitUniforms()`: replace `createUniform("u_directionalLightDir", Vec4, 1)` and `createUniform("u_directionalLightColour", Vec4, 1)` with array variants (num = `kMaxDirLights`) | Rename handles to `mUDirLightDir` / `mUDirLightColour` to match new uniform names. |
| 4 | Update `MeshRenderer::DrawCommand()`: upload both array uniforms with `bgfx::setUniform(h, data, kMaxDirLights)` | All 8 slots uploaded per draw; zero-padded slots cost nothing on GPU. |
| 5 | Rewrite the directional light section of `fs_mesh.sc`: declare `uniform vec4 u_dirLightDir[8]` and `u_dirLightColour[8]`; replace single-light block with static loop | Remove old `u_directionalLightDir` / `u_directionalLightColour` declarations. |
| 6 | Add a second directional light (a cool-toned fill light) to `Mesh3DTestStage` to visually exercise the feature | Low intensity blue-grey fill from the opposite hemisphere to the main light. |
| 7 | Cook shaders (`dia pipeline --target cluichetest`), run `dia run cluichetest`, verify both lights contribute visibly to the mesh | Existing single-light box/avocado scenes must produce identical output to pre-change. |

---

## Binding Decisions

| Decision | Implication |
|----------|-------------|
| BG3-005 — No bgfx types in public headers above DiaBgfx3D | `MeshPassLighting` stores `float[]` arrays (bgfx handle indices never appear). `kMaxDirLights` constant lives in `MeshPassLighting.h`, which is DiaBgfx3D-internal. |
| BG3-004 — No visitor pattern in the render path | `Canvas3D` reads `Mesh3DFrameData` directly in a plain loop; no dispatch mechanism added. |
| PD-004 — No STL in public APIs | No new public APIs. `Canvas3D` packing loop uses plain indexed access on `DynamicArrayC`. |
| RB-001 — bgfx is the GPU abstraction | Array uniforms created via `bgfx::createUniform` with `num` parameter; no secondary abstraction. |

---

## Open Design Questions

1. **Uniform name rename**: The old flat uniforms are named `u_directionalLightDir` / `u_directionalLightColour`; the new array uniforms use `u_dirLightDir` / `u_dirLightColour`. bgfx uniform names are matched by string at bind time. Any external tooling or test that references the old names will break silently. Confirm the rename is acceptable (no other shaders or tools reference these names) before task 3.

2. **`kMaxDirLights` vs `Mesh3DFrameData::kMaxLights`**: `Mesh3DFrameData` already defines `kMaxLights = 32`. The shader/renderer cap of 8 is a separate ceiling. Should `kMaxDirLights` live in `MeshPassLighting.h` (renderer-local) or be promoted to `DiaGraphics3D` as a shared engine constant? Promoting it keeps the data layer and render layer in sync but crosses the module boundary for a renderer-specific limit.

---

## Acceptance Criteria

- Up to 8 directional lights registered via `Mesh3DFrameData::AddDirectionalLight()` all produce visible Lambert diffuse contributions.
- A scene with exactly one registered light produces visually identical output to the pre-change renderer.
- Registering 0 lights produces a diffuse-black surface (ambient still applied) — no crash, no undefined behaviour.
- Registering more than 8 lights logs a warning and uses the first 8; no buffer overrun.
- Shadows remain tied to `dirLights[0]`; additional lights cast no shadows.
- No bgfx types appear in `MeshPassLighting.h` public fields.
- `Mesh3DTestStage` has a visible second fill light demonstrating the feature.
