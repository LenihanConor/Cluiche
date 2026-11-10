# Research: Choice — Editor Deep-Link Launch

**Date:** 2026-11-09
**Chosen candidate:** Foundation slice (typed view request + `dia view`, game discovery, headless data queries, light view-state contract), followed by the headless plugin host, gated by a spike. Editor focus mode is the pre-agreed fallback host.

## Rationale

The user's goal is to open as little as possible to see one specific thing (not a full editor), launched from a command line, script, AI call or button press, already connected to the right game and in the right state. The foundation is cheap, uncontested and required by every host option. The headless plugin host (a small native process that loads an unchanged plugin and serves its existing UI over a localhost WebSocket) is the way to cover all plugins, thin and thick, without porting any plugin code, and was code-checked as feasible (see ideate.md "Feasibility evidence"). The user confirmed the evaluation's recommendation as written. Top evaluation factors: low cost and risk of the foundation (scores 4.55 / 4.40 / 4.15 for headless queries, discovery, `dia view`), and the headless host's reach and light weight over editor focus mode and a native CEF viewer.

## What Was Ruled Out

| Candidate | Reason not chosen |
|-----------|------------------|
| 7. Native CEF viewer exe | Duplicates the editor bootstrap; superseded by the headless host |
| 8. `editor.*` DiaAPI commands on a running editor | Not needed when each view is its own small process; deferred until scripted control of the full editor is wanted |
| Browser talking directly to DebugServer (web-only variant) | Only works for plugins whose C++ half is a pure forwarder (about 5 of 7 Inspectors); would need ports for Entity and AssetRuntime |
| 9. In-game "Inspect in Editor" trigger | Follow-up; needs a reliable launch path first |
| 10. `cluiche://` links and presets | Follow-up; thin front ends over the same request |

## Pre-Spec Commitments

- **Scriptable first:** every view and query is reachable from a CLI/API callable from scripts and AI, with structured JSON results and non-zero exit on failure. Callers never need to know which host runs a view (host is a per-plugin manifest setting behind `dia view`).
- **Open as little as possible:** a single view in its own small window (or no window, for headless queries), already connected to the named game.
- **Typed request model first, UI second:** one `ViewRequest` document; CLI, buttons, links and presets are thin front ends (UI/business separation rule). Actions and queries are DiaAPI/CLI commands, not UI-only paths.
- **Host spike gates the headless host.** Before committing: (1) resolve the response-path discrepancy (`DiaApplicationFlowInspector/UI/src/bridge.ts` and `DiaApplicationFlowEditor/UI/src/v2/bridge.ts` match responses on `type === 'DiaEditor_onResponse'`, which the shell's `__diaResponse` envelope does not carry) against a running editor; (2) confirm whether `DiaWebSocket::Server` can serve static files or `dia` serves the UI; (3) size the WebSocket-backed `IJSBridge` implementation. If the spike fails, fall back to editor focus mode (`CluicheEditor --only <plugin>`).
- **Host design constraints:** loads plugins unchanged; registers a `GameConnectionManager` in the `PluginServiceLocator`; binds 127.0.0.1 only; checks Origin and a per-launch token.
- **Perf Inspector must be host-neutral:** its spec should state that the UI speaks only DebugServer-shaped query/push messages and its C++ half stays a pass-through, so it works under the headless host. Update `perf-inspector.md` accordingly when that spec is next touched.
- **Layer placement:** request model, discovery record writer, and the headless host are generic and belong in Dia. Per the escalation rule, confirm placement (new Dia-level project vs. inside DiaEditor) with the user before writing the system spec.
- **Separate finding to track:** `DiaWebSocket::Server` sets no origin check and calls `listen(port)` (likely all interfaces), so any web page can reach a running DebugServer. Add to BACKLOG as its own item; do not fold into this work.
- Platform constraints carried in: PD-010 (targets resolve from `.diagame`), PD-002 (request intake via a Module and stream), PD-004 (Dia containers in public APIs; JSON behind the Dia Json wrapper), PD-007 (C++20).

## Next Step

Run `/spec-system` (suggested name: DiaViewLaunch) covering the foundation as feature specs (view request + `dia view`, game discovery, headless queries, view-state contract) and the headless plugin host as a gated feature with the spike as its Task 1.
Suggested parent system: DiaEditor (host and plugin contract), with DiaCLI (`dia view`, `dia query`) and DiaDebugServer (discovery record) as touched modules; parent application: Dia.
