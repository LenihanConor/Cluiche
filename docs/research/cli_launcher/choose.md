# Research: Choice — CLI Launcher / Dia Console

**Date:** 2026-09-04
**Chosen candidate:** Dia Console — Web Edition (synthesis of Candidates 3, 4, 8, and 10)

## Rationale

Discussion after the initial evaluation surfaced that the user wants the full feature set the cheap-first recommendation deliberately deferred: typed/structured results (not raw log text), a clean command/execution model separated from the UI (stated as a standing architectural rule, not just a preference for this tool), richer semantic input types including project/target context, named presets as a core feature, and genuine visual richness — which rules out a terminal UI and points at a native-window web UI instead.

This combines:
- **Candidate 3's** full scope (typed command/project/execution/results model, full registry, no bespoke per-command UI).
- **Candidate 4's** delivery technology (local web UI in a chromeless native window via `pywebview`/WebView2, free-only front-end stack — explicitly not Webix).
- **Candidate 8's** presets, promoted from "nice to have" to a core feature.
- **Candidate 10's** foundation-first architecture (typed model/registry built as its own layer, before/independent of the UI), matching the newly-recorded standing rule to always separate UI from business/execution logic (see memory: `feedback_ui_business_separation`).

A key scoping fact emerged during discussion: CoW (and future projects) will live as **sibling folders inside this same Cluiche repo**, per CLAUDE.md's existing architecture statement. DiaCLI's existing command discovery (`cli_main.py` walking the project root for `cli/` folders) already reaches any sibling project's own commands for free — so "share one launcher across projects" does not require cross-repo plugin/entry-point machinery. It only requires a **project/target context** inside the console (which game/target you're currently pointed at, filtering commands and supplying defaults), not a full multi-repo `ProjectRegistry`. This meaningfully de-risks what looked like the hardest unknown going into this decision.

The user explicitly chose to build this as one coherent unit rather than phase it, prioritizing a design optimized for the end state over incremental shippable milestones.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|-------------------|
| 7. Exe-Only Launcher Panel | Too narrow — solves only "launch," not structured results/presets/visual richness the user actually wants. Its capability is subsumed by the chosen scope's generated forms for `run`/`launch`. |
| 1. Trogon-Style Reflected Launcher (pure reflection, Textual) | Reflection alone can't produce the richer semantic types (project/target-aware pickers) or typed results the user wants; Textual caps out on visual richness (no inline images, character-grid layout) — directly conflicts with "real visual richness, that's the point." |
| 2. Curated Vertical-Slice Console (Textual) | Same visual-richness ceiling as Candidate 1; also a deliberately narrower command set, which the chosen full-registry approach supersedes. |
| 6. Live NDJSON Dashboard (viewer only) | Solves observation only, not action (no launching, no forms) — a useful building block but not a destination on its own. Its function (tailing the existing NDJSON event stream) is absorbed into the chosen execution model rather than treated as a separate deliverable. |
| 5. Fuzzy Command Palette | Dropped earlier in discussion — added little beyond what the chosen approach's generated forms already cover, no persistent visual surface. |
| 9. CluicheEditor Console Plugin | Dropped earlier — unresolved bootstrapping problem (can't build CluicheEditor with a tool that lives inside CluicheEditor). |
| Full multi-project `ProjectRegistry` across separate repos | Considered but ruled unnecessary once the sibling-repo fact was confirmed — the console needs project/target *context*, not cross-repo command discovery/merging. |

## Pre-Spec Commitments

- **UI/business separation is a standing rule**, not scoped to this tool: build a typed command/execution/result model first; the UI (native web window) depends on that model's interfaces only and never constructs or executes business logic directly.
- **Descriptor source:** reflect from existing Click command objects as the baseline, layered with an optional thin metadata overlay for richer semantic types (target/project/config/platform) and presentation grouping — not a fully hand-authored YAML descriptor per command.
- **Execution transport:** route through the existing `OutputContext` NDJSON event stream (`dia.output.v1`) rather than inventing a parallel event system from scratch; wrap it in a typed `ExecutionService`/`ExecutionEvent` layer per the standing separation rule.
- **UI technology:** local web UI (backend in-process with the console, not a separate always-running server) presented in a chromeless native window via `pywebview` on WebView2. Free-only front-end stack — Webix is explicitly excluded due to its licensing tier.
- **Project/target context:** CluicheTest, CoW, and future sibling projects are discovered via the existing repo-tree-walk mechanism; the console needs a "current project/target" selector and per-project defaults, not a cross-repo registry.
- **Presets:** named, saved command+argument combinations are a core v1 feature, not deferred polish.
- **Build approach:** designed and built as one coherent unit, optimized for the end-state architecture rather than staged for early incremental delivery.

## Next Step

Run `/spec-system` (this is system-sized: multiple features — command/execution model, registry, results model, presets, project/target context, native web UI shell) with this candidate and `docs/research/cli_launcher/summary.md` as input.
Suggested parent: new Dia system, e.g. `DiaConsole`, living inside `Dia/DiaCLI` (Python tooling, no engine/C++ module implications).
