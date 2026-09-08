# Research Summary — Editor Workspace / Persona

**Session folder:** docs/research/editor_workspac/
**Date:** 2026-06-03

## One-Line Answer

"Editor Memory" — the editor invisibly remembers layout, loaded plugins, and per-plugin state (project-scoped), restoring everything on open with no UI or named profiles.

## Journey

1. **Explored:** Surveyed workspace/persona patterns across VS Code, JetBrains, Unity, Unreal. Identified 6 design axes (scope, multiplicity, storage, granularity, switch mechanism, plugin participation). Noted SED-021 already anticipated persona extensibility.
2. **Ideated:** 8 candidates generated from S to L scope — auto-save, named profiles, presets, manifest overlays, plugin groups, full persona system.
3. **Evaluated:** Auto-Save Session (3.85) and Named Workspace Profiles (3.80) scored highest. Auto-save won on cost and risk.
4. **Chose:** "Editor Memory" — Candidate 1 refined. Framework remembers layout + plugins (editor-global); per-plugin state is project-scoped via existing SED-021 `.context.json`. No named profiles needed.

## Chosen Work Item

**Name:** Editor Memory
**Home module:** DiaEditor (extends DockingLayout persistence + SED-021 `.context.json`)
**Suggested spec type:** Feature
**Estimated size:** S (≤1 week)

## Key Insights from Exploration

- SED-021's `.context.json` already handles per-plugin state with archive/session semantics — just needs a project key
- Layout persistence (`editor-layout.json`) already exists in `DockingLayout`; this extends it to also cover loaded plugin list
- Per-plugin state is naturally project-scoped (pipeline runs, open files, connection targets) — the plugin owns this decision, not the framework
- Layout and loaded plugins are editor-global (you want the same arrangement regardless of which game you're editing)
- CEF iframe state loss (SED-018) means we should save state *before* any unload — but since memory just restores on open (not switch), this is not a concern for v1
- Named profiles (Candidate 2) are a clean future extension if needed — memory becomes the "save" half of any switch operation

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| 2 — Named Workspace Profiles | Profile management overhead not justified; auto-restore covers the need |
| 3 — Auto-Save + User-Global Presets | Two concepts; cross-project portability not needed |
| 4 — Manifest Overlays | Overloads manifest purpose; rare plugin set changes don't need switch UI |
| 5 — Plugin Pinning + Layout Snapshots | Two partial solutions; memory is simpler and more complete |
| 6 — Plugin Groups | Additive group model more complex than "remember what was open" |
| 7 — Full Persona System | Massively overscoped |
| 8 — .cluicheproj State Extension | Couples user-local state to VCS; out/ storage is cleaner |

## References

- docs/research/editor_workspac/explore.md
- docs/research/editor_workspac/ideate.md
- docs/research/editor_workspac/evaluate.md
- docs/research/editor_workspac/choose.md
