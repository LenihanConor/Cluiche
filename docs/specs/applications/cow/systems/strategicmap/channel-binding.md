# Feature Spec: Strategic Map Channel Binding

> **Strata (2026-11-21, not yet built):** `depthZone` in the S-1 source becomes `stratum` (@docs/specs/features/draoi/generic-strata.md, Approved, not yet built).

## Parent System
@docs/specs/applications/cow/systems/strategicmap/strategicmap.md (Feature 2)

**Gameplay Domains:** rendering, simulation
**Depends on:** Strategic Map Feature 1 (visuals), S-1 (world), GameSpeedController (day), Strategic Map Feature 4 (snapshot types; they moved there from the old DiaTerrainMap3D module spec on 2026-11-23)
**Research:** docs/research/tile_renderin_art_pipeline/summary.md
**Prototype:** `docs/research/tile_renderin_art_pipeline/lookdev/js/alive.js` (`YearClock`), `js/weather.js` (`WeatherSim` output contract), `js/app.js` (`healthOf`, channel textures)

## Status
`Draft` (2026-11-20). Drafted from the approved prototype; awaiting user review.

## Summary

The Strategic Map is a view of the simulation. This feature defines **how each simulation system's state reaches the renderer**:

- **`IMapChannelSource`**: a small interface each source implements to fill one channel. The renderer cannot tell a V-1 stand-in from the real A1, A3, A4 or A5.
- **Snapshot publisher** (SimPU): collects the sources each step and publishes `TerrainWorldSnapshot` and `TerrainChannelSnapshot` over FrameStream (the contract in Strategic Map Feature 4). Standard channels (Condition, Weather, Snow, Water, Events) and the 4 custom slots follow DraoiHexWorldRender3D's layout (TMAP-010).
- **Built sources now**: the S-1 world source and the year clock. A1, A3, A4 and A5 sources are written when those systems are built; their slots and the contract are fixed here.

## Problem

- Four different systems own the facts the map shows. Without one contract, the map module would import each system's headers and break whenever one changes.
- The map must keep working while A1, A4 and A5 are unbuilt or partly built (V-1 stand-ins), and switch to the real systems without any renderer change.
- State may cross PU boundaries only as stream snapshots.

## Goals

1. **`IMapChannelSource`** with one fill method per channel kind, a version counter, and a validity flag.
2. **Channel layout** documented once: per cell, what each component means, range and filtering.
3. **S-1 world source** (built today): class, elevation, temperature, rivers, depth zone, relief class from the S-1 world.
4. **Year clock source**: season weights and living-year drivers derived from the GameSpeedController day.
5. **Slots for A1 (condition, succession), A4 (weather), A5 (water state), A1/A3 (events) and herds**, each marked required or optional, with a "none" behaviour (see below).
6. **Snapshot publisher** module on SimPU that builds snapshots from the registered sources and writes the streams, publishing only changed channels.
7. **Degradation**: a missing source yields the neutral channel (full health, no weather, no events), logged once.

## Non-Goals

- Implementing A1, A3, A4 or A5 sources (done with those systems; each is a small adapter).
- The stand-in drivers (V-1 Feature 2).
- Rendering (DraoiHexWorldRender3D).
- Smoothing A4's two snapshots: the source supplies the smoothed state A4 already computes (CHANGE-048).

## Design

### Channels

| Channel | Layout (per cell) | Range | Source |
|---|---|---|---|
| World class | `primary`, `secondary`, `reliefClass`, `depthZone` (4 × u8) | ids | S-1 |
| World scalars | elevation, temperature, + river inputs (mask, flowDir, flowAccum) | 0..1 etc. | S-1 |
| Condition | `health`, `succession fade` (0 = old class, 1 = new), `successionTarget` (class id) | 0..1 | A1 + A3 |
| Weather | `cloud`, `precip`, `snowFraction`, `gust` | 0..1 | A4 smoothed |
| Water state | `lakeLevel`, `flow state` (0..4), `flood`, `ice` | 0..1 / enum | A5 (year clock until built) |
| Events | `burn`, `scar` | 0..1 | A1 natural fire: `burn` = `ecology.transient.fire_intensity`; `scar` = healing curve of `years_since_fire` (and the A3 `BurnedAshland` class); V-1 stand-in until A1 is built |
| Season | `weights[4]`, `day of year` (global, not per cell) | 0..1 | GameSpeedController + year clock |

### Interface

```cpp
namespace CoW::Map {
    enum class ChannelId : uint8_t { World, Condition, Weather, WaterState, Events, Season };
    class IMapChannelSource {
    public:
        virtual ~IMapChannelSource() {}
        virtual ChannelId Channel() const = 0;
        virtual bool      IsLive() const = 0;                                 // false: renderer uses the neutral channel
        virtual uint64_t  Version() const = 0;                                // changes when content changes
        virtual bool      Fill(float* rgbaPerCell, uint32_t cellCount) = 0;   // SimPU, inside the publisher's step
    };
    class MapSnapshotPublisher : public Dia::ApplicationFlow::Module {
    public:
        bool Register(IMapChannelSource&);                                   // one per channel; replace = hot swap (live switch)
        void Unregister(ChannelId);
        // Per sim step: for each source whose Version changed, Fill and write the channel snapshot.
    };
}
```

### Season and year clock

`YearClock` is pure: day-of-year → weights. Each season holds for 70% of its 91 days, then cross-fades over the last 30% (the prototype). `Drivers(weights)` gives the living-year values (snow pack, lake level, river flow state, fire risk) used until A4/A5 supply theirs. The clock reads `GameSpeedController`'s day counter through its published state (no new dependency on its internals).

### Stand-in to live switch

`Register(source)` replaces the previous source for that channel on the next step; the version counter is bumped so the renderer re-uploads. V-1 uses this for its Live switch (Feature 2 there).

## Acceptance Criteria

1. **Channel sizes.** Each channel snapshot is exactly `cellCount × 4` components for the world's grid; a mismatch is rejected with an error and the previous snapshot stays.
2. **S-1 source.** For an S-1 Small world, the world snapshot's class, elevation, temperature, depth zone and river inputs equal the S-1 layers cell for cell (hash equality against the layer dump).
3. **Versioning.** A step where no source's version changed writes no channel snapshot (counter); a version change writes exactly that channel.
4. **Neutral on missing.** With no condition source the renderer receives health 1.0 for every cell (logged once); no weather source gives cloud/precip 0; no events gives 0. Never NaN or uninitialised data.
5. **Hot swap.** Replacing a source at runtime changes the next published channel, bumps the version and logs the swap; the old source is not called again.
6. **Year clock.** Weights always sum to 1 (±1e-6) over every day 0..363; hold plateaus are exactly one season; the cross-fade is C¹ smooth (no derivative jump at the window edges); weights repeat each year.
7. **Living-year drivers.** Snow pack, lake level, flow state and fire risk follow the documented seasonal curves (Dry: low flow and high fire risk; Freeze: frozen, high snow; Bloom: flood).
8. **PU boundary.** The publisher reads source state only on SimPU; no render-side code includes a source header (a dependency check: the map module does not depend on A1/A3/A4/A5 modules).
9. **Cost.** Filling and publishing all channels at Large (6,656 cells) takes under 1 ms on the dev machine (Debug-only, ballpark; recorded, not gated).
10. **Observability.**
    - **Logs:** channel `MAP`, info on source register or swap, warning on missing source or size mismatch.
    - **Metrics:** `map.channels_published`, `map.publish_ms`.
    - **Health:** the publisher reports Degraded when any registered source is not live.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| SM-003 | Weather per cell, never global | The weather channel is per cell; the year clock's global season is the only global |
| SM-004 | Fire and herds are rendered from A1/A3 state | Events channel from the source; the map simulates nothing |
| PD-002 | PU/Phase/Module | Publisher is a SimPU module; renderer reads streams |
| Feedback: PU boundaries are always streams | No mutex-guarded cross-PU reads | FrameStream snapshots only |
| Feedback: no singletons | Use ModuleRef or streams | `Register` is called through `ModuleRef<MapSnapshotPublisher>` |
| PD-001 | StringCRC | Stream ids, channel names |
| AD-002 | Four-tier | Interface and publisher are CoW-Shared (reused by every testbed and the game) |

## Open Design Questions

1. **Succession fade.** A3 changes a cell's biome. The map wants a cross-fade over several weeks, which needs both the old and new class for the fade. The channel carries `successionTarget` and `fade`; the *old* class stays in the world snapshot until the fade ends. A3 must publish the transition events with a duration, or the publisher computes the fade from the change time. Decide when A3's plan is written.
2. **Fire (resolved 2026-11-20; was "no owner").** A1 owns fire: natural ignition and spread at the Season band (A3 amendment 3), the per-cell transient `fire_intensity` (layer `ecology.transient.fire_intensity`), `ash`, and the history field `years_since_fire`; A3 turns repeated fire into `BurnedAshland`. So the Events channel needs **no new A1 field**: `burn` comes from `fire_intensity`, and the `scar` is a healing curve of `years_since_fire` (default exp(−years / 3), a style parameter) plus the `BurnedAshland` class itself once A3 fires. **Caveat:** A1 resolves a whole fire inside one Season step, so no continuous daily front exists in the data. The map therefore plays the front as a **cosmetic playback**: when the Events channel version changes, cells with `burn > 0` ignite in order of distance from the highest-intensity cell over a few render seconds. The V-1 fire CA is the prototype of that playback, not a model of what A1 does. **Optional A1 ask** (small, not blocking): publish the spread order or ignition cell per fire (`ecology.transient.fire_order`) so the playback can follow the real spread; without it the playback uses distance from the intensity peak.
3. **Fauna (resolved 2026-11-20; was "needs a Fauna channel").** v1 needs **no new channel**. A1 already publishes per-cell group and species densities (`ecology.group.<tag>`, `ecology.species.<id>`, e.g. `ecology.group.herbivore`). The map scatters herd sprites from those densities exactly as it scatters props (deterministic per cell, count proportional to density), and lets them wander slowly, steered by `ecology.migration` where present; birds stay ambient and visual-only. When A1 Population Tokens (Layer 2) is built, its herd tokens publish real positions and replace this source through the Live switch (a `Fauna` sprite-list source, specced with the Tokens feature specs). The V-1 herd driver is the prototype of this density-driven scatter.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `IMapChannelSource`, channel layouts, neutral channels | AC 1, 4 | Not Started | sonnet | |
| 2 | `YearClock` and living-year drivers (pure) | AC 6, 7 | Not Started | sonnet | |
| 3 | S-1 world source | AC 2 | Not Started | sonnet | |
| 4 | `MapSnapshotPublisher` module, versioning, hot swap | AC 3, 5, 8 | Not Started | sonnet | needs the snapshot types from Feature 4 |
| 5 | Cost, observability | AC 9, 10 | Not Started | haiku | |
| 6 | A1/A3/A4/A5 source adapters | per system | Deferred | sonnet | done with each system; slots fixed here |
