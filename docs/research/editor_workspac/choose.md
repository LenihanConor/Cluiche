# Research: Choice — Editor Workspace / Persona

**Date:** 2026-06-03
**Chosen candidate:** Editor Memory (based on Candidate 1 — Auto-Save Session)

## Rationale

The user's core need is "reopen what I had" — not switching between named profiles. The concept is called "Editor Memory": the editor remembers what was open, where it was arranged, and any per-plugin state. On open, it restores that memory. No named profiles, no workspace switcher, no presets.

Per-plugin state (e.g. last pipeline run, last open stage) is project-scoped — handled by the existing SED-021 `.context.json` mechanism with a project key. Layout and loaded plugins are editor-global. This gives the user 80% of what named workspaces would without the conceptual overhead.

Key: this is called "memory", not "auto-save" or "workspace."

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| 2 — Named Workspace Profiles | Overhead of profile management UI not needed; auto-restore covers the use case |
| 3 — Auto-Save + User-Global Presets | Two concepts to learn; user-global presets not needed when project scoping solves the data isolation |
| 4 — Manifest Overlays | Overloads manifest purpose; plugin set changes are rare enough not to need a switch UI |
| 5 — Plugin Pinning + Layout Snapshots | Two small features that partially solve the problem; memory is simpler and more complete |
| 6 — Plugin Groups | Additive group model more complex than just "remember what was open" |
| 7 — Full Persona System | Massively overscoped for the actual need |
| 8 — .cluicheproj State Extension | Couples user-local state to version-controlled file; memory in out/ is cleaner |

## Pre-Spec Commitments

- **Name:** "Editor Memory" — the feature is called memory, not auto-save or workspace
- **Layout + loaded plugins:** Editor-global (same arrangement regardless of project)
- **Per-plugin state:** Project-scoped via SED-021 `.context.json` with a project key — plugins own their own memory, framework just provides the project context
- **Storage:** `out/CluicheEditor/` per PD-009 (gitignored, user-local)
- **No new UI:** Invisible — just works. No named profiles, no switcher dropdown
- **Plugin participation:** Plugins opt-in to saving/restoring state via `.context.json`; framework handles layout and plugin list opaquely
- **Project isolation:** When switching projects, plugins that key state by project will show the correct data; plugins that don't key by project show their last global state (acceptable default)

## Next Step

Run /spec-feature with this candidate as input.
Suggested parent system: DiaEditor (extends existing SED-021 `.context.json` and DockingLayout persistence)
