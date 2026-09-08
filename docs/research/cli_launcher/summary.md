# Research Summary — CLI Launcher / Dia Console

**Session folder:** docs/research/cli_launcher/
**Date:** 2026-09-04

## One-Line Answer

**Dia Console** — a native-window web UI over DiaCLI's existing Click command tree and NDJSON event stream, with typed structured results, project/target context, and named presets, delivered as one coherent architecture rather than a cut-down MVP.

## Journey

1. **Explored:** Started from a user-supplied v0.1 architecture doc ("Dia Console") that assumed a greenfield build. Checking the real `Dia/DiaCLI` codebase found two foundations already exist: Click-based commands (reflectable metadata) and a schema-versioned NDJSON execution-event stream (`OutputContext`/`dia.output.v1`). This meant the doc's proposed "command descriptor" and "execution event" layers could largely be *derived*, not hand-built.
2. **Ideated:** Generated 10 candidates spanning descriptor source (reflection vs. hand-authored), UI technology (Textual TUI vs. local web vs. editor plugin), and scope (narrow exe-launcher slice vs. full seed-doc catalog). Two (fuzzy palette, CluicheEditor plugin) were dropped in discussion — the plugin option has a bootstrapping problem, since a tool that builds CluicheEditor can't live inside CluicheEditor.
3. **Evaluated:** Weighted scoring initially favored the cheapest options (exe-only launcher panel, reflected Textual launcher, NDJSON viewer) — a "ship the MVP first" recommendation.
4. **Chose:** The user rejected the cheap-first framing after seeing what it deferred — they want typed/structured results, richer semantic input types, a strictly separated command/execution model (stated as a standing architectural rule, not just for this tool), named presets as core, and genuine visual richness (ruling out a terminal UI). A key unlock during discussion: CoW and future projects will live as sibling folders in this same repo, so DiaCLI's existing tree-walk command discovery already reaches them — no cross-repo plugin/registry work is needed, only an in-console project/target context.

## Chosen Work Item

**Name:** Dia Console (Web Edition)
**Home module:** New system inside `Dia/DiaCLI` — Python tooling, no engine/C++ implications
**Suggested spec type:** System (multiple features: command/execution model, registry, typed results, presets, project/target context, native web UI shell)
**Estimated size:** L — multi-week, built as one coherent unit (not phased)

## Key Insights from Exploration

- DiaCLI already carries most of the "command descriptor" data needed via Click's own command/option objects — avoid a parallel hand-authored YAML descriptor per command; reflect first, layer a thin metadata overlay only for what Click can't express (richer semantic types, presentation grouping).
- DiaCLI already emits structured, schema-versioned NDJSON execution events (`OutputContext`, `dia.output.v1`) — the execution/event layer should wrap and tail this, not reinvent it.
- CoW and future sibling projects are reachable today via DiaCLI's existing project-root tree-walk discovery (`cli_main.py`) — "share one launcher across projects" is a project/target *context* feature, not a cross-repo plugin/registry problem.
- Webix (used elsewhere in this codebase, CEF-side only) is explicitly excluded from this tool due to licensing-tier uncertainty; use a free-only front-end stack.
- Native-window feel for a web UI is achievable via `pywebview` on WebView2 (built into Windows 10/11) — a chromeless custom titlebar, no browser chrome, optionally packaged as a one-file `.exe`.
- A visual mockup (`mockups/console.html`) was built and approved as the acceptance-gate reference before spec/implementation, per this project's existing editor-feature-mockup convention.
- Standing rule recorded for future work generally: always separate UI from command/execution/business logic (saved to memory as `feedback_ui_business_separation`).

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| Exe-Only Launcher Panel | Solves only "launch," not the fuller structured-results/presets/visual-richness scope the user actually wants; its capability is subsumed by the chosen candidate's generated forms. |
| Trogon-Style Reflected Launcher (Textual) | Textual caps out on visual richness (no inline images, character-grid layout) — conflicts with "real visual richness, that's the point." |
| Curated Vertical-Slice Console (Textual) | Same Textual visual ceiling; also a deliberately narrower command set than the chosen full-registry approach. |
| Live NDJSON Dashboard (viewer only) | Observation only, no action; its function (tailing the NDJSON stream) is absorbed into the chosen execution model. |
| Fuzzy Command Palette | No persistent visual surface; added little beyond what generated forms already cover. |
| CluicheEditor Console Plugin | Bootstrapping problem — can't build CluicheEditor with a tool that lives inside CluicheEditor. |
| Full cross-repo `ProjectRegistry` | Unnecessary once sibling-repo placement was confirmed — needed project/target context, not cross-repo discovery/merging. |

## References
- docs/research/cli_launcher/explore.md
- docs/research/cli_launcher/ideate.md
- docs/research/cli_launcher/evaluate.md
- docs/research/cli_launcher/choose.md
- docs/research/cli_launcher/mockups/console.html
