# Research: Evaluate — Engine Data Architecture Foundation

**Input:** docs/research/engine_data_arch/ideate.md

## Scoring Criteria

- **Engine Value (0.25):** Improves Dia module reusability or capability — does this make the engine better for any game?
- **Game Value (0.20):** Unblocks real game content for a small AI-developed game?
- **Implementation Cost (0.25):** Inverse of effort — 5 = very cheap, 1 = very expensive
- **Risk (0.15):** Inverse of uncertainty — 5 = well-understood, 1 = highly uncertain
- **Cluiche Fit (0.15):** Aligns with module structure and PD-001 through PD-009

## Scores

| # | Candidate | Layer | Engine (0.25) | Game (0.20) | Cost (0.25) | Risk (0.15) | Fit (0.15) | Total |
|---|-----------|-------|---------------|-------------|-------------|-------------|------------|-------|
| 8 | JSON Definition Loader | Utility | 3 | 4 | 5 | 5 | 4 | 4.10 |
| 2 | Asset Type Framework | Backbone | 4 | 3 | 4 | 4 | 5 | 3.95 |
| 1 | Identity & Relationship Backbone | Backbone | 5 | 3 | 3 | 4 | 5 | 3.90 |
| 6 | Validation Engine | Build+Auth | 4 | 4 | 4 | 4 | 4 | 4.00 |
| 4 | Build Pipeline Core | Build | 5 | 4 | 2 | 3 | 5 | 3.70 |
| 5 | Stage & Bundle Runtime Loader | Runtime | 4 | 5 | 3 | 3 | 5 | 3.90 |
| 3 | Editor Asset Explorer | Authoring | 3 | 4 | 3 | 3 | 4 | 3.35 |
| 7 | Build Debugger & Log Viewer | Build tooling | 2 | 3 | 5 | 5 | 4 | 3.60 |

## The Real Question

These candidates form a dependency chain, not a competition. The question is: **where do we draw the line for the first deliverable?**

Three natural cut points:

### Cut A: "Foundation Only" — Candidates 8 + 2 + 1
**Combined size:** M (2-4 weeks)
**What you get:** Every asset has identity, a type framework, bidirectional relationships, and a JSON loading mechanism. No pipeline, no runtime loader, no editor view.
**What you can do with it:** Register and query assets programmatically. Write tests. Build the next layers on solid ground.
**What you can't do:** Build assets, load stages at runtime, or browse in the editor.
**Risk:** Low. All three build on proven Dia infrastructure (TypeSystem, StringCRC, HashTableC).
**Weighted score (avg of components):** 3.98

### Cut B: "Foundation + Build" — Candidates 8 + 2 + 1 + 6 + 4
**Combined size:** L (4-7 weeks)
**What you get:** Everything in Cut A, plus validation and a full discover→validate→transform→deploy pipeline with debuggable logging.
**What you can do with it:** Author assets as JSON, run `data build`, get validated built output with clear error messages. AI-authored content is gated by validation.
**What you can't do:** Load stages at runtime (still manual), browse in the editor.
**Risk:** Medium. Pipeline design involves decisions about per-type transforms and output format.
**Weighted score (avg of components):** 3.93

### Cut C: "End-to-End" — All 8 candidates
**Combined size:** XL (8-14 weeks)
**What you get:** The full three-layer system: author in editor, build via pipeline, load at runtime. Complete loop.
**What you can do with it:** Everything. Browse assets in the editor, build with `data build`, load stages in-game.
**What you can't do:** Nothing is deferred (except incremental build optimization and streaming).
**Risk:** Higher. Editor panel (CEF integration), runtime loader (thread safety, memory ownership), and build pipeline all in one pass.
**Weighted score (avg of components):** 3.81

## Top 3 Cut Points

### Rank 1: Cut B — Foundation + Build (score: 3.93)

**Why:** This is the sweet spot for "small AI-developed game, room to grow." The foundation (identity, types, relationships) provides the backbone, and the pipeline (validation + build) provides the critical gate between AI-authored content and runtime. After Cut B, you can:
- Define asset types and register them
- Author entities, configs, stages, bundles as JSON
- Run `data build` and get validated, deployed output
- Read the build log to debug any failures
- Query the registry for what exists and how it connects

The runtime loader (Cut C) can load built output with a simple hardcoded path initially — the Stage & Bundle Loader formalizes this later but isn't strictly blocking for a small first game.

**Watch out for:** The pipeline's per-type transform steps need design decisions (binary format? validated JSON copy? something else?). These decisions should be made during spec, not discovered during implementation.

### Rank 2: Cut A — Foundation Only (score: 3.98)

**Why:** Highest average score because it's the cheapest and lowest risk. Gets the hardest architectural decisions locked in (identity model, relationship model, type framework) without committing to pipeline specifics. Good if you want to start building game content quickly with manual/ad-hoc loading and add the pipeline later.

**Watch out for:** Without validation, AI-authored content can have broken references and schema violations that only surface at runtime. Without the pipeline, there's no raw/built separation — the game loads raw files directly, which works but creates rework when you add the pipeline later.

### Rank 3: Cut C — End-to-End (score: 3.81)

**Why:** The only cut that delivers the complete vision from your three-layer description. No deferred work, no "we'll add the editor later." If the goal is to have the full authoring→build→runtime loop before starting game content, this is it.

**Watch out for:** 8-14 weeks is significant for a small game project. The editor panel (CEF/JS) and runtime loader involve different skill domains. Risk of scope creep as each layer reveals design questions in adjacent layers. The lower score reflects the higher cost and risk, not lower value.

## Recommendation

**Cut B (Foundation + Build)** is the recommended first deliverable.

It scores highest when you weight the constraint that matters most: **AI-authored content must be validated before it reaches runtime.** Cut A skips validation; Cut C spends 2x the time for the editor and runtime loader, which provide polish but not safety.

Cut B delivers the backbone (PD-001 StringCRC identity, PD-004 DiaCore containers, PD-003 component-aware type framework) plus the critical quality gate (validation + pipeline). The runtime loader and editor explorer are natural follow-ups — and they're easier to build well once the foundation and pipeline have stabilized.

The build order within Cut B would be: 8 → 2 → 1 → 6 → 4, matching the dependency chain from ideation.

After Cut B ships, Cut C becomes two independent workstreams that can happen in either order:
- **Runtime path:** Candidate 5 (Stage & Bundle Loader)
- **Tooling path:** Candidates 3 + 7 (Editor Explorer + Build Debugger)
