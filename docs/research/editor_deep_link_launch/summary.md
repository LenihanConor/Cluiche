# Research Summary — Editor Deep-Link Launch

**Session folder:** docs/research/editor_deep_link_launch/
**Date:** 2026-11-09

## One-Line Answer

A scriptable `dia view` command (typed request, game discovery, headless data queries, view-state contract) that opens exactly one editor view, already connected to the right game and in the right state, using a small headless plugin host rather than the full CluicheEditor.

## Journey

1. **Explored:** Today the editor only accepts `--project=<.diagame>` at cold start; there is no single-instance handling, no way to address a plugin or view state, and nothing lets the editor find the right running game. DebugServer knows its port, name and `.diagame` but does not publish them.
2. **Ideated:** 10 candidates, reframed mid-session from "steer the full editor" to "open as little as possible to see one thing". Candidates split into a shared foundation, a host choice (headless web host, editor focus mode, native CEF viewer), and follow-up front ends (in-game button, URI links, presets).
3. **Evaluated:** Top scorers were the cheap foundation pieces (headless queries 4.55, discovery 4.40, `dia view` 4.15). The real decision was the host: headless host 3.60 vs editor focus mode 3.55 vs native CEF viewer 2.75.
4. **Chose:** The recommended bundle: foundation first, then the headless plugin host gated by a spike, with editor focus mode as the fallback. User wanted the host to work for all plugins, not just thin ones, which ruled out a browser-only front end.

## Chosen Work Item

**Name:** DiaViewLaunch (foundation + headless plugin host)
**Home module:** DiaEditor (plugin contract and host), with DiaCLI (`dia view`, `dia query`) and DiaDebugServer (discovery record)
**Suggested spec type:** System, with feature specs for each foundation piece and a gated host feature
**Estimated size:** L (foundation S-M; host M-L after spike)

## Key Insights from Exploration

- `WebUIBridge` is already decoupled from CEF through `IJSBridge`, a two-method interface in DiaCore, so a WebSocket-backed implementation lets any plugin's unchanged C++ half run in a headless process.
- Plugin UIs talk to the host through `postMessage` and a small shell relay (about 100 lines), so existing `bridge.ts` files do not need changes if the web shell replays the protocol.
- None of the seven Inspector plugins uses the context's model, view or plugin loader, and `EditorPluginBase` null-guards them; the host only has to supply a bridge and a `GameConnectionManager` service.
- Open discrepancy: the ApplicationFlow Inspector's and ApplicationFlow Editor's `bridge.ts` match responses on a shape the shell never sends; must be checked against a running editor.
- Pure-web (browser straight to DebugServer) covers only the thin plugins (about 5 of 7); the headless host covers all of them.
- Separate finding: `DiaWebSocket::Server` has no origin check and likely listens on all interfaces, so any web page can reach a running DebugServer today.

## Discarded Candidates

| Candidate | Why discarded |
|-----------|--------------|
| Native CEF viewer exe | Duplicates editor bootstrap; superseded by the headless host |
| `editor.*` commands on a running editor | Not needed when each view is its own process; deferred |
| Browser direct to DebugServer | Only covers thin plugins |
| In-game trigger, URI links, presets | Follow-ups once a view launches reliably |

## References
- docs/research/editor_deep_link_launch/explore.md
- docs/research/editor_deep_link_launch/ideate.md
- docs/research/editor_deep_link_launch/evaluate.md
- docs/research/editor_deep_link_launch/choose.md
