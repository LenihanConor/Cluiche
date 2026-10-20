# Application Spec: CoW (Crucible of Wings)

## Parent Platform
@docs/specs/platform/Cluiche.md

## Purpose
Crucible of Wings (CoW) is a living dragon terrarium and stewardship simulation: the player is an Archdragon presiding over a clan of autonomous dragons in a human-free, ecologically living world, guiding a lineage across centuries toward Apotheosis (a one-per-world transformation that reshapes the world). It is a Civ-style strategic board with local nest and dragon drill-down, and it is made for kids in a way that respects their intelligence. It is not a combat-first game or a scripted narrative.

CoW is built on top of Dia and CluicheGameBaseline, developed as its own application sibling to CluicheTest and CluicheEditor. Its tech/art phases are built as 12 vertical-slice **testbeds** that accumulate into the final game. The first build boots to either the real game UI or a debug menu that launches any testbed standalone.

**Backlog:** [BACKLOG.md](../../../BACKLOG.md) (Project = CoW). **Cross-boundary needs:** [dia-needs.md](dia-needs.md).

**Design source of truth:** the GDD repo (`C:\GitHub\DesignDocs\COW`, `Output Docs/GDD/`). Macro vision: `01_MacroVision/HighConceptPitch.md`. Testbed roster: `10_PrototypeStrategy/TestbedRoster.md` (DECISION-120). The GDD owns what each testbed must prove and its pass/fail metrics; this spec tree owns how it is implemented (see AD-003).

## Systems
One stage per testbed (AD-005).

| System | Track | Description | GDD | Spec |
|--------|-------|-------------|-----|------|
| GameSpeedController | Foundation | EU4/Civ-style game speed — Pause + 4 config-driven presets via DiaSimTime time scale, daily ticks with band (DAY/WEEK/SEASON/YEAR/DECADE/CENTURY), modal-safe pause. Only sim time pauses/scales; UI uses wall-clock time | `08_SystemsDeepDive/A2_WorldClock/` | @docs/specs/applications/cow/systems/gamespeedcontroller/gamespeedcontroller.md |
| F-1 Harness | Foundation | Boot flow, debug menu, event bus, inspector/debug-command framework, event record/replay | `10_PrototypeStrategy/F1_Harness.md` | @docs/specs/applications/cow/systems/f1-harness/f1-harness.md |
| F-2 Core Runtime | Foundation | Testbed proving GameSpeedController, RNG determinism, save/load round-trip, band resolver (DiaAttribute Feature 7). One scenario (save, reload, replay) with 6 checks: stream independence, determinism round-trip, RNG save/load, clock proof, E1 proof, band resolver timing. GDD Locked 2026-10-09, Spec Approved 2026-10-11 | `10_PrototypeStrategy/F2_CoreRuntime.md` | @docs/specs/applications/cow/systems/f2-core-runtime/f2-core-runtime.md |
| F-3 World Assets | Foundation | Single contract for world-sim assets (layout under `Global/<System>/`, `format`+`version` envelope, load-and-warn staleness) and in-testbed authoring: override-only weather/ecology packs, baked worlds, sparse ecology start states, `.cowworld` bundles; `WorldAssetsModule` loads the `cow.diagame` `default_world` bundle; DiaAPI `save_as` writes source assets in dev launches. Spec Approved 2026-10-19 (6 features); dia-needs #16, #17 Approved, not built | — (product review 2026-10-19) | @docs/specs/applications/cow/systems/f3-worldassets/f3-worldassets.md |
| S-1 World Generation | Simulation | **Live in-game terrain editor** (amended 2026-10-15): rule-based world per GDD `GeographyAndRegions.md` shaped live by sliders and presets, then biomes classified from terrain, then regions and sites, guarantees, retry, JSON save/load. Small map only. Every editor action is a DiaAPI command. No A1/A3 runtime. GDD Locked 2026-10-11 (decision 1 needs updating), Spec Approved 2026-10-11, 9 features (Feature 1 re-spec pending; 8 and 9 to spec) | `10_PrototypeStrategy/S1_WorldGeneration.md` | @docs/specs/applications/cow/systems/s1-worldgeneration/worldgenerationpipeline.md |
| A4 Weather | Simulation | Weekly hex cellular-automaton weather (moisture, temperature, pressure, wind, precipitation, snow). Owns the A1↔A4 contract: `WeatherUpdated{week}` + read-only `WeatherSnapshot`, `IBiomeSeasonalBaseline` (A1 implements). Debug domain, overlays, presets and regression tests; the stage is S-2.5. GDD Locked 2026-10-11, Spec Approved 2026-10-14 (6 features). Features 3–6 blocked on the S-1 hex + terrain amendment; Feature 4 also on dia-needs #11 | `08_SystemsDeepDive/A4_Weather/` | @docs/specs/applications/cow/systems/a4-weather/a4-weather.md |
| A1 Ecology | Simulation | Layer 1 analytical cell ecology on the hex grid: data-driven species (juvenile/adult), producers, pressure fields, band-gated tick pipeline (no delta_scale), hysteretic ecology state, succession rule table feeding A3, aquatic + coastal feeding, edge/refugia lifeboats. Implements A4's `IBiomeSeasonalBaseline`. Dragon step 9, tokens and local instantiation deferred. GDD Locked 2026-10-12, Spec Approved 2026-10-18 (9 features; 14 GDD conflicts resolved) | `08_SystemsDeepDive/A1_Ecology/` | @docs/specs/applications/cow/systems/a1-ecology/a1-ecology.md |
| A3 Biome Succession | Simulation | Biome identity changes: staging table, synchronous resolver inside A1's step 10, authors the succession rule table A1 evaluates (forward + recovery), one-shot legacy + timed marker, visual states, recovery stages, dragon acceleration interface. GDD Locked, Spec Approved 2026-10-18 (5 features) | `08_SystemsDeepDive/A3_BiomeSuccession/` | @docs/specs/applications/cow/systems/a3-biomesuccession/a3-biomesuccession.md |
| S-2.5 Weather Patterns | Simulation | The weather layer's tool: A4 alone over curated S-1 worlds. Nine numeric checks (GDD Tests 1–9) run headless for the verdict and live as check lines; hosts A4's overlays, inspector, presets and step-back; CoW-Shared scenario/walkthrough format; time-series graphs. S-2.5 should pass before S-3 Living-mode results are trusted (amended 2026-10-19, product review). GDD Locked 2026-10-18, Spec Approved 2026-10-18 (5 features) | `10_PrototypeStrategy/S2.5_WeatherPatterns.md` | @docs/specs/applications/cow/systems/s25-weatherpatterns/s25-weatherpatterns.md |
| T-1 World Sim Inspector | Tools | CoW's first game-scoped CluicheEditor plugin (only with `cow.diagame`): reads DiaCellHistory live or from `dia.cell_history` report files; time series + events, region/biome breakdowns, distributions, hex mini-map, ledger "why" panel, correlation, run-vs-run compare; navigation commands back to the game. Spec Approved 2026-10-19 (6 features) | — (product review 2026-10-19) | @docs/specs/applications/cow/systems/t1-worldsiminspector/t1-worldsiminspector.md |
| T-2 World Data Editor | Tools | Game-scoped CluicheEditor plugin (only with `cow.diagame`), opened via Asset Catalogue `open_asset`: schema-driven table/form editors (species, biomes, rules, defaults), pack diff/compare, bundle composer, scenario editor, cell editor for baked worlds + start states; one validator with `dia validate schema`; reload in game. Spec Approved 2026-10-19 (6 features) | — (product review 2026-10-19) | @docs/specs/applications/cow/systems/t2-worlddataeditor/t2-worlddataeditor.md |
| ~~S-2 Single Cell Ecology~~ | Simulation | **Merged into S-3 Ecology 2026-10-18** (Controlled mode). Single-cell equation proof moved to A1's GoogleTests and `isolate_cell` | `10_PrototypeStrategy/S2_SingleCellEcology.md` | @docs/specs/applications/cow/systems/s3-ecology/s3-ecology.md |
| S-3 Ecology | Simulation | The ecology layer's tool (S-2 + S-3 merged 2026-10-18): loads an S-1 world and hosts A1's tools (inspector, change ledger, tuning with write-back, painting, graphs). **Controlled** mode: pinned climate, guided S-2 scenarios. **Living** mode: A4 live, A3 when built, guided S-3 scenarios + headless long-run report. Spec Approved 2026-10-18 (5 features) | `10_PrototypeStrategy/S2_SingleCellEcology.md`, `S3_LivingEcology.md` | @docs/specs/applications/cow/systems/s3-ecology/s3-ecology.md |
| S-3.5 World Sim | Simulation | Every world layer together on one world (S-1 + A4 + A1 + A3 via F-3 bundles). **Author** mode: shape → bake → weather/ecology packs → start state → bundle → set `default_world`. **Run** mode: all layers live with layer viewer, selection, timeline, snapshots; T-1 connects. **Regression** mode: R1–R6 whole-world checks (reproducible, save/load, snapshot, authoring round trip, perf at Large, coverage) across Small/Standard/Large. Launch-outs to S-2.5/S-3 with the same bundle. Spec Approved 2026-10-19 (4 features); GDD Locked 2026-10-20 (CH-S35-01…05) | `S3.5_WorldSim.md` Locked 2026-10-20 (DECISION-128, CHANGE-045) | @docs/specs/applications/cow/systems/s35-worldsim/s35-worldsim.md |
| S-4 Dragon | Simulation | Isolated single-dragon sandbox and live-in-world modes; pathfinding | `10_PrototypeStrategy/TestbedRoster.md` | TBD |
| S-5 Lineage & Hearth | Simulation | Single-hearth isolated mode, live hearth economy/lifecycle, breeding/drift generations preset | `10_PrototypeStrategy/TestbedRoster.md` | TBD |
| S-6 Dynasty | Simulation | Command chain (loyalty/succession/Archdragon powers), clans and politics, Apotheosis late-game preset | `10_PrototypeStrategy/TestbedRoster.md` | TBD |
| S-7 Endurance | Simulation | Stress load (swarm/tick budget) and long-run history/save paging | `10_PrototypeStrategy/TestbedRoster.md` | TBD |
| V-1 Map Visuals | Visual | Tile rendering, overlays, fog of war, camera, on static data | `10_PrototypeStrategy/TestbedRoster.md` (summary; testbed spec not written) | TBD |
| V-2 Dragon & Nest Visuals | Visual | Genome-to-visual rendering, lifecycle/weathering stages, nest diorama scene | `10_PrototypeStrategy/TestbedRoster.md` (summary; testbed spec not written) | TBD |
| V-3 UI & Interaction | Visual | Generic UI kit, input/interaction, lineage view, event readability (canned then live) | `10_PrototypeStrategy/TestbedRoster.md` (summary; testbed spec not written) | TBD |
| A-1 Audio | Audio | Ambient soundscape, notification sounds, time-speed-aware audio. Dia has no audio module today, so this is a from-scratch gap (Dia need, see AD-001/AD-002) | `10_PrototypeStrategy/TestbedRoster.md` (summary; testbed spec not written) | TBD |
| G-1 First Playable | Integration | Real front end plus every testbed's visuals toggled on; the actual boot-to-game path | `10_PrototypeStrategy/TestbedRoster.md` (summary; testbed spec not written) | TBD |

## Application-Specific Architecture

CoW work follows a four-tier dependency model, checked for every capability CoW needs:

1. **Dia** — generic engine capability, any game could use it (rendering, physics, ECS, math).
2. **CluicheGameBaseline** — generic gameplay infrastructure any Cluiche game needs (e.g. `Scene2DModule`), but not engine-level.
3. **CoW-Shared** (`Cluiche/CoW/Shared/`) — specific to CoW's own content/mechanics, reused across more than one CoW system.
4. **Feature-local** — specific to a single CoW feature; stays where it's used.

Tiers 1 and 2 are outside CoW's edit zone (see Decisions below); tiers 3 and 4 are built directly as part of CoW work.

## Platform Dependencies
- [x] Dia (engine)
- [x] CluicheGameBaseline (shared gameplay infra)
- [ ] Other:

## Platform Targets
- **PC first**, tablet later.
- Dia has no touch input today; tablet support will need a Dia input capability (log to `dia-needs.md` per AD-002 when it becomes relevant).

## Out of Scope
- Direct edits to Dia, CluicheGameBaseline, CluicheTest, or CluicheEditor from CoW work — these are proposed, not implemented, from within CoW (see AD-001).

## Key Users / Personas
- **Dev team** (harness and testbeds): launches any testbed from the debug menu, inspects state, runs the pass/fail scenarios. The debug menu is dev-only and never reaches players.
- **Children and families** (the game): the audience the shipped game is built for. It must respect their intelligence, avoid combat-first play, and keep the simulation readable without menus.

## Decisions
<!-- Decisions specific to this application. Binding decisions cascade to all systems and features within it.
     AI: Always check parent platform decisions (Cluiche.md) first — those take precedence.
     Use AD- prefix for application-level decision IDs. -->

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| AD-001 | CoW develops on a dedicated `cow` branch/worktree carrying a tracked `.claude/settings.json` `ask` permission rule for `Edit`/`Write`/`NotebookEdit` on `Dia/**`, `Cluiche/CluicheGameBaseline/**`, `Cluiche/CluicheTest/**`, `Cluiche/CluicheEditor/**`, their spec trees, and shared build/platform files. | Guarantees no engine or shared-layer change lands without explicit human sign-off, enforced by the permission layer rather than relying on convention alone. | This app | Accepted | Yes |
| AD-002 | Every cross-cutting need CoW hits is classified against the four-tier model (Dia / CluicheGameBaseline / CoW-Shared / Feature-local) before implementation. Blocking needs are flagged immediately for a decision; non-blocking needs are logged to `dia-needs.md` for batch review. | Prevents CoW-only mechanics leaking into shared layers, and prevents genuinely reusable code getting stranded inside CoW; matches how urgent a need actually is instead of stopping on every ask. | This app | Accepted | Yes |
| AD-003 | The GDD repo owns design intent (what each testbed must prove, pass/fail metrics, the roster). This spec tree owns implementation (system specs, `.plan.md` files, AD decisions, code). | Keeps one source of truth per concern; avoids design drifting into implementation specs and vice versa. | This app | Accepted | Yes |
| AD-004 | Promote `BootMenuModule` from CluicheTest into CluicheGameBaseline (proposed under AD-001; requires sign-off). Dev builds boot to the debug menu by default, which has a Play entry for the front end (no `--play` flag; the flag was dropped because DiaCLI has no argument forwarding). Shipping-build gating was dropped by the user for now. Each game supplies its own `.diaapp` with Boot transitions (`GetStageTransitions("Boot")`); no new manifest format. | CoW testbeds need the same stage-launcher pattern CluicheTest already proves; promoting it avoids duplicating it per game. | This app | Accepted | Yes |
| AD-005 | One stage per testbed (CluicheTest-style): each of the 16 testbeds in the GDD roster (including S-2.5, V-4 and S-3.5 World Sim, added as DECISION-128 / CHANGE-045, a recorded exception to the roster's Rule 1) is one `.diastage` (amended 2026-10-20, GDD fold-back) listed as a transition from CoW's `Boot` stage. Modes and presets (e.g. isolated vs live, Apotheosis) are chosen from a panel inside the stage, never as separate stages. | Keeps the roster at 12 stages and reuses the existing `GetStageTransitions("Boot")` pattern with no new manifest format. | This app | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all child systems and features · `No` = guidance only

## AI Review Questions
<!--
  AI: Review this application spec and list any questions that must be answered
  before systems can be designed or features built. Consider:
  - Ambiguous purpose or scope
  - Missing persona detail that would affect system design
  - Unclear platform dependency boundaries
  - Anything that contradicts the platform spec or its Binding decisions
  Format: | # | Section | Question | Answer |
-->

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Systems | What is CoW's first system / vertical slice? | F-1 Harness |
| 2 | Platform Dependencies | Does CoW need new CluicheGameBaseline capability day one, or can it start purely on existing Dia + baseline APIs? | Yes: promoting `BootMenuModule` into the baseline (AD-004). Its `DebugUIModule`, `DebugServerHostModule` and `RenderToSimNavRequest` already live in the baseline; the only CluicheTest-specific coupling is `CluicheTest::TestResultsRegistry` (status display), which must be abstracted. |
| 3 | Personas | Who are the key users? | Dev team (harness and testbeds) and children/families (the game). |

## Status
`Draft`
