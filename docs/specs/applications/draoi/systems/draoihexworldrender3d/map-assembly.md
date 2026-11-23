# Feature Spec: DraoiHexWorldRender3D Map Assembly

## Parent System
@docs/specs/applications/draoi/systems/draoihexworldrender3d/draoihexworldrender3d.md

**Gameplay Domains:** rendering
**Depends on:** Features 1–3; DiaBgfx3D Features A (render targets) and D (post-process stack)
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/app.js` (frame loop, draw order, state defaults)

## Status
`Draft` (2026-11-23). **Was DiaTerrainMap3D Feature 4 Module, Presentation and Tooling (Draft 2026-11-20, Open Design Questions 4–6 added 2026-11-22).** Draoi packages are plain libraries (TMAP-011), so it is split:
- **Kept here (library):** the `HexWorldMap` facade, versioned uploads, draw order, layer toggles, data-view switch, post settings hand-off, picking helpers, bake-state query, logs, profile scopes and metrics.
- **Moved to CoW Strategic Map Feature 4 (@docs/specs/applications/cow/systems/strategicmap/module-and-camera.md):** the module, the stream contract and snapshot types, bake jobs and commit timing, all DiaAPI commands, presets, the capture command, the map camera, the health reporter, selection routing, markers and regions, and the S-1 Data | Map view switch (the old Open Design Questions 1, 3, 4, 5 and 6).

## Summary

`HexWorldMap` is the single object a game's render-side module owns to get a hex map on screen. It holds Features 1–3 and hands DiaBgfx3D its work in the right order each frame. It never reads sim state, registers commands or touches streams: the game calls it with plain data.

## Goals

1. **Facade.** `HexWorldMap` owns `HexTerrainSurface`, the hex terrain material, `PropScatter`, the dynamic sprite layers and the particle density fields. Its references to the DiaBgfx3D sprite renderer, particle system and post stack come from the caller.
2. **World lifecycle.** `SetWorld` builds the heightfield, textures and scatter from a committed bake. `ApplyRebake(changedRect, changedCells)` uploads the rectangle and rescatters those cells. A new world releases the old resources.
3. **Versioned channel uploads.** `UpdateChannel` and `UpdateCustomSlots` take a version, and an unchanged version is a no-op (counted). A Condition change rescatters affected cells. Weather and Events changes refresh the particle density.
4. **Draw order.** Skirt → terrain → props (cutout) → particles → translucent sprites → post (DiaBgfx3D D, settings from the style) → caller's UI.
5. **Toggles and data views** as style fields (`layers.*`, `dataView`). Switching never rebuilds the world.
6. **Picking helpers.** `ScreenToWorld` (ray against the heightfield through `HeightAt`) and `WorldToCell` (through `OwnerAt`).
7. **Bake state query.** `BakeState { Idle, Baking(done, total), Ready }`, set by the caller's progress callback and readable by its status command.
8. **Observability.** Logs on world set, rebake applied and rejected inputs; profile scopes; metrics (below). No health reporter here: the module that owns the map registers one.

## Non-Goals

- Modules, streams, snapshots, commands, presets, capture commands, camera, input, selection, markers (CoW Strategic Map Feature 4).
- Running the bake. The caller runs DiaCellFieldBake and DraoiHexWorldRelief on a worker and calls `SetWorld` or `ApplyRebake` after `Commit`.

## Design

```cpp
namespace Draoi::HexWorldRender3D {
    struct MapRenderers { Dia::Bgfx3D::SpriteInstanceRenderer* sprites; Dia::Bgfx3D::ParticleSystem* particles;
                          Dia::Bgfx3D::PostProcessStack* post; };
    enum class BakeStage : uint8_t { Idle, Baking, Ready };
    struct BakeState { BakeStage stage; uint32_t done, total; };

    class HexWorldMap {
    public:
        bool SetWorld(const Dia::CellFieldBake::BakedWorld&, const HexWorldRelief::ReliefFields&,
                      const HexWorldVisuals&, const MapRenderers&);
        void ApplyRebake(Dia::CellFieldBake::TexelRect changed, const uint16_t* cells, uint32_t count);
        void UpdateChannel(Channel, const float* valuesPerCell, uint32_t version);
        void UpdateCustomSlots(const float* rgbaPerCell, uint32_t version);
        void SetDynamicSprites(SpriteLayer, const DynamicSprite*, uint32_t count);
        void SetStyle(const HexWorldStyle&);                 // includes layers.* toggles and dataView
        void SetSeasonWeights(const float weights[4]);
        void Draw(const Dia::Camera3D&, float heightScale, float timeSeconds, uint32_t boilFrame);
        bool ScreenToWorld(const Dia::Camera3D&, float sx, float sy, float& wx, float& wy) const;
        uint16_t WorldToCell(float wx, float wy) const;      // kNoOwner outside
        void SetBakeState(BakeState); BakeState GetBakeState() const;
        void Release();
    };
}
```

The caller owns time, the boil frame (12 Hz in the prototype) and the height scale (the camera eases it with zoom). The library takes them as arguments, so captures stay deterministic.

## Acceptance Criteria

1. **World lifecycle.** `SetWorld` with a different grid rebuilds the heightfield, textures and scatter. Resource counters return to baseline after `Release` or a second `SetWorld`.
2. **Rebake.** `ApplyRebake` for K cells uploads only the rectangle and rescatters only those cells (counters); the next frame shows the change.
3. **Versioning.** An `UpdateChannel` with an unchanged version causes no upload (counted). A Condition update rescatters only cells whose condition band changed. Weather and Events updates rewrite the particle density fields only when the version changes.
4. **Draw order.** A capture with every layer on matches the documented order (translucent sprites over particles over props; post last). Each toggle removes exactly its layer.
5. **Data views.** Each view renders its raw value; switching view does not call any world rebuild (counter).
6. **Picking.** `ScreenToWorld` then `WorldToCell` round-trips a grid of screen points through the camera and the grid under tilt and relief (error under half a texel). A ray that misses the world returns false.
7. **Determinism.** The same inputs, camera, time and boil frame give byte-identical frames (the caller's capture command relies on this).
8. **Lifetime.** 20 cycles of `SetWorld` and `Release` leak nothing.
9. **Observability.** Log channel `HWR3D` (info on world set and rebake, error on rejected inputs); scopes `HexWorldMap.Draw`, `HexWorldMap.ApplyRebake`; metrics `hwr3d.frame_ms`, `hwr3d.channels_uploaded`, `hwr3d.instances`.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| TMAP-011 | Plain library | No module, stream, command or PU affinity; the game calls it |
| TMAP-005 | Per-cell channels | Channel inputs are per-cell arrays |
| PD-012 | Draoi: no modules, wiring or debug domains | Everything wired lives in CoW Strategic Map Feature 4 |
| CFB-004 | Double-buffered rebake committed at a frame boundary | `ApplyRebake` is called after the caller's `Commit` |

## Open Design Questions

1. **Hex readability** (system ODQ 1). If cell highlighting becomes an outline from the owner texture, it lands here as a style layer that takes a hovered and a selected cell. CoW owns the selection state.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `HexWorldMap` world lifecycle and release | AC 1, 8 | Not Started | sonnet | |
| 2 | `ApplyRebake` and versioned channel updates | AC 2, 3 | Not Started | sonnet | |
| 3 | Draw order, toggles, post hand-off | AC 4, 7 | Not Started | sonnet | needs DiaBgfx3D D |
| 4 | Data views switch | AC 5 | Not Started | haiku | |
| 5 | `ScreenToWorld` / `WorldToCell` | AC 6 | Not Started | sonnet | |
| 6 | Bake state, observability | AC 9 | Not Started | haiku | |
