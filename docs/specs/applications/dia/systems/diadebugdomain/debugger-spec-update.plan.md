**Spec:** @docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
**Status:** Done

---

## Context

Full audit of all visual debugger specs against:
1. **`DrawImGui()` retirement** — the old ImGui hook is fully deprecated; all display must flow via `GetJSONState()` → Ultralight/JS panel. Any spec referencing DrawImGui() describes a dead code path.
2. **Panel stats gaps** — domains that work at the draw-layer level but whose panel cards show nothing useful (no stats, no live data).
3. **Missing domain specs** — domains with live IDebugDomain code but no spec.
4. **Spacing compliance (AC-16)** — all domain panel cards must honour the CSS spacing values from `docs/research/visual_debugger_redesign/mockup.html`:

| Element | Required value |
|---------|---------------|
| Group header padding | `5px 10px` |
| Domain header padding | `4px 8px` |
| Group body padding | `4px 8px 6px 8px` |
| Domain card `margin-bottom` | `3px` |
| Drawer checkbox row gap | `5px 10px` (row / column) |
| Domain card body padding | `6px 10px 8px 10px` |
| Accent colour | `var(--accent)` only — no hardcoded hex |

AC-16 is formally defined in `debugger-contract.md` and inherited by all domain specs. These spacing values must be referenced explicitly in each new or updated spec's Panel Card Specification section.

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | **coord3d-debug-overlay.md** — retire AC5 DrawImGui ref; add GetJSONState ACs 11-13; update AC1 console→panel; update Task 6 | dia run cluichetest (panel shows camera pos/dir/FOV) | Done | sonnet | Camera stats were completely absent from panel — code bug |
| 2 | **diamesh3dvisualdebugger.md** — full expansion: JSON schema with all 11 cached MeshStatsDrawer fields; data-flow note; domain ACs D1-D8; AC-16 spacing | dia run cluichetest (panel shows Draws/Dropped/Loaded) | Done | sonnet | mCachedDrawCount etc. exist but GetJSONState() never reads them |
| 3 | **diautilityaivisualdebugger.md** — new system spec: panel-only IDebugDomain; GetJSONState via GetLastFrameScores(); score bars; AC-16 spacing | dia run cluichetest (panel shows per-action scores) | Done | sonnet | UtilityScoreDrawer.Draw() is a no-op; DrawImGui() was never written |
| 4 | **coord2d-debug-overlay.md** — update AC3 console→panel; AC8 add GetJSONState cursor stats; add ACs 11-14 (cursor XY, bounds stat, setScale, AC-16) | dia run cluichetest (panel shows Cursor: (X, Y)) | Done | haiku | — |
| 5 | **dialighting3dvisualdebugger.md** — full expansion: LightRangesDrawer; JSON schema Dir/Point/Spot/ShadowCasting; domain ACs D1-D7; AC-16 spacing | Inspect panel (Lights: N shown) | Done | sonnet | LightRanges is a new drawer |
| 6 | **diarigidbody2dvisualdebugger.md** — enhanced JSON schema (sleeping/static/angular velocity); domain ACs D1-D7 incl. sleep-state colouring; AC-16 spacing | dia run cluichetest (panel shows Bodies: A awake / S sleeping) | Done | haiku | — |
| 7 | **ik2d-visual-debugger-stack.md** — append Domain Panel Specification: identity table, JSON schema (chainCount, per-chain solved/iterations/endEffectorError), panel card, domain ACs, AC-16 ref | Inspect panel (Chains: N, per-chain table) | Done | haiku | IK2DDebugDomain exists in code; spec has no panel section |
| 8 | **rig2d-visual-debugger-stack.md** — append Domain Panel Specification: identity table, JSON schema (boneCount, ikChainCount), panel card, domain ACs, AC-16 ref | Inspect panel (Bones: N) | Done | haiku | Rig2DDebugDomain exists in code; spec has no panel section |
| 9 | **animation2d-visual-debugger-stack.md** — append Domain Panel Specification: identity table, JSON schema (activeClip, normalizedTime, layers[], springs{}), panel card + clip name stat line, domain ACs, AC-16 ref | Inspect panel (Active: run_forward @ T=0.42) | Done | sonnet | Clip name/time are the highest-value missing stats |
| 10 | **softbody2d-visual-debugger-stack.md** — append Domain Panel Specification: identity table, JSON schema (bodyCount, particleCount, constraintCount, anchorCount, sleeping), panel card, domain ACs + constraint-stress colouring, AC-16 ref | Inspect panel (Bodies: N, P particles) | Done | haiku | SoftBody2DDebugDomain exists; spec has no panel section |
| 11 | **geometry2d-visual-debugger-stack.md** — append Domain Panel Specification: identity table, JSON schema (shapesThisFrame, circles/polygons/lines), panel card, domain ACs, AC-16 ref | Inspect panel (Shapes: N) | Done | haiku | Geometry2DDebugDomain exists; spec has no panel section |
| 12 | **diascene2dvisualdebugger.md** — new system spec: Cameras + Lights + LayerBounds drawers; JSON schema (activeCam, cameraCount, lightCount, layerCount, camera pos/zoom/rot); panel card; AC-16 | Inspect panel (Cam: main) | Done | sonnet | — |
| 13 | **diaentityvisualdebugger.md** — new system spec: EntityDebugDomain 6 drawers; JSON schema (alive, total, maxHierarchyDepth, selection{}); panel card; domain ACs; AC-16 | Inspect panel (Entities: A / T) | Done | sonnet | — |
| 14 | **diaassetruntimevisualdebugger.md** — new system spec: GetJSONState wired to loaded/streaming/failed; JSON schema (stats{}, byType[]); panel card; domain ACs; AC-16 | Inspect panel (Assets: L loaded, S streaming) | Done | sonnet | Draw() stub noted; memory stats deferred |
