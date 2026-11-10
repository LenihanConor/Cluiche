# Feature Spec: DebugServer Connection Token

**Parent:** @docs/specs/applications/dia/systems/diaviewlaunch/diaviewlaunch.md (Feature 2)
**Status:** Approved (2026-11-10)
**Depends on:** @docs/specs/applications/dia/systems/diaviewlaunch/discovery-record.md (the record carries the token)

## Summary

Today any local process, and any web page served from localhost, can connect to a running game's DebugServer and run commands. This feature adds a per-launch secret token. The game generates it before it listens and publishes it only in its discovery record. The WebSocket server refuses a connection with a missing or wrong token at the handshake (HTTP 403), before any message is read. The token travels in the URL (`ws://127.0.0.1:<port>/?token=<hex>`), so no protocol message changes. Every in-repo client adopts it in this feature: CluicheEditor, the E2E harness and the GoogleTests loopback helpers. The `dia query` client in Feature 3 adopts it too.

## Goals

- A connection without the current launch's token never reaches `HandleMessage`.
- Nothing in the repo breaks: the editor, E2E and tests connect as before, with no extra steps for the user.
- The token never appears in a log line, a metric, `dia query games` output or a crash report.

## Acceptance Criteria

### Token generation and check (DiaDebugServer, DiaWebSocket)

1. `DebugServer::Start` generates 16 random bytes (`BCryptGenRandom`), encoded as 32 lowercase hex characters, before the server listens. `DebugServer::GetToken()` returns it. A new token is generated on every launch.
2. `Dia::WebSocket::Server::SetRequiredToken(const char*)` is set before `Start`. An empty value means no check. The value is immutable while listening, so the validate handler reads it on the WebSocket thread with no cross-PU state.
3. The validate handler runs the existing Origin check first, then reads the `token` query parameter from the request resource. It accepts only an exact match, compared in constant time.
4. On a missing or wrong token the handshake gets HTTP 403. The server logs one Warning, `Server: Rejected connection: missing token` or `invalid token`, with the Origin. It never logs the token or the resource URL. `GetAuthRejectedCount()` (atomic) is incremented.
5. websocketpp access and error logging stays off for the resource line, so the URL with its token is never written by the library.
6. Module config `"auth": "token" | "none"`, default `token`. An unknown value logs a Warning and uses `token`. With `none`, `SetRequiredToken("")` is called and the record's `token` is empty.
7. The discovery record (Feature 1) carries the token. `dia query games` and every log omit it.

### Metrics

8. `DebugServerHostModule` publishes `dia.debugserver.auth_rejected` (counter) from `GetAuthRejectedCount()`.

### CluicheEditor and DiaEditor (`GameConnectionManager`)

9. Before every connection attempt, including auto-reconnect, `GameConnectionManager` resolves the token by calling `FindLiveDiscoveryRecords` with a port filter. Each new launch has a new token, so it is never cached across attempts. `outRoot` is derived from the exe path as `<exe>/../../../../out`, matching `ObservationModule`. Then it connects to `ws://127.0.0.1:<port>/?token=<hex>`.
10. If no live record exists for the port, no socket is opened. The state becomes Error with the message `No running game on port <N>`, shown in the Game Connection panel through the existing error path. Auto-reconnect keeps retrying on its normal schedule.
11. If several live records share the port (only possible after a crash plus pid reuse, which liveness rules out), the newest wins and a Warning is logged.
12. A 403 from the server leads to the Error state `Connection refused: token rejected (game restarted?)`, and the token is resolved again on the next attempt.
13. The saved URL (`editor-connection.json`, `ConnectionButton.tsx` default) and the panel UI are unchanged. Only the host part is normalised to `127.0.0.1` when the connection is made.

### E2E harness

14. `plugin.py` resolves the token from the launched game's discovery record. It already finds that game's session folder. It polls for the record (up to the existing 60 s connect timeout) before connecting, and passes `token=` to `DiaClient`. `DiaClient` adds it to the URL and connects to `127.0.0.1`.
15. `dia test e2e --port <n>` without a launched process (attach mode) resolves the token by port from the live records.
16. The full E2E suite passes with `auth: token`, with the same pass and fail set as the baseline on the same tree.

### GoogleTests

17. Loopback test helpers that connect to a real `DebugServer` (the `TestDebugServer*`, `SLOW_DebugServer*` and WebSocket integration tests) pass the server's `GetToken()` in the URL. No test switches to `auth: none` just to get through. Raw `Dia::WebSocket::Server` tests that don't set a token are unchanged.
18. New `SLOW_WebSocketToken` raw-handshake tests:
    - no token → 403
    - wrong token → 403
    - right token → 101
    - right token from a non-localhost Origin → 403
    - `auth: none` → 101
    - the rejection counter increments
    - the log line contains neither the token nor the resource.

### Escape hatch and failure mode

19. `dia launch <target> --no-auth` (and `dia run`) passes a dev override that sets the DebugServer `auth` to `none` for that launch only. It never edits the `.diaapp`. The game logs one Warning at start, `DebugServer auth disabled (--no-auth)`.
20. With `auth: token`, if the discovery record can't be written (no session, or a write error), the server still requires the token. `DebugServerHostModule` logs one Error: `Discovery record not written (<reason>): no client can obtain the token; restart, or launch with --no-auth`.

### Docs

21. The DiaDebugServer and DiaWebSocket module docs, and the protocol doc, describe the URL parameter, the 403, and how a client finds the token (discovery record).

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaWebSocket/Server.h/.cpp` | `SetRequiredToken`, token check in the validate handler, rejection counter, access log off |
| `Dia/DiaDebugServer/DebugServer.h/.cpp` | Token generation, `GetToken`, `auth` mode |
| `Cluiche/CluicheGameBaseline/Modules/DebugServerHostModule.cpp` | `auth` config, token into the record, metric |
| `Dia/DiaEditor/LiveConnection/GameConnectionManager.h/.cpp` | Token resolution per attempt, `127.0.0.1`, new error states |
| `Dia/DiaEditor/LiveConnection/GameConnectionController.cpp` | Surface the new error messages |
| `Cluiche/Tests/E2E/plugin.py`, `client.py` | Record polling, `token=` |
| `Cluiche/Tests/GoogleTests/DebugServer/*`, `WebSocket/*` | Helpers pass the token; new `TestWebSocketToken.cpp` |
| `Dia/DiaCLI/dia_cli/cli/launch.py`, `run.py` | `--no-auth` dev override |
| Module docs (DiaWebSocket, DiaDebugServer, DiaDebugProtocol) | Documented |

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| VL-004 | Per-launch token, only in the discovery record, checked in the validate handler | AC 1–7 |
| SED-010 | DiaDebugProtocol for editor-game wire types | No wire message changes; the editor reads the record through the DiaDebugProtocol reader (Feature 1) |
| SED-016 | `Connect()` is explicit; the manager boots with no game | Resolution happens inside an explicit connect attempt; boot is unchanged |
| PU boundaries are streams | The token is set before `listen` and never written again, so the WebSocket-thread read needs no lock or stream |
| DDS-009 | DebugServer uses `DiaWebSocket::Server` | The check lives in the existing validate handler, not a second layer |
| AD-001 | Engine API changes need sign-off | Sign-off needed for DiaWebSocket (`SetRequiredToken`, counter), DebugServer (`GetToken`, `auth`) and DiaEditor (`GameConnectionManager` behaviour) |

## Test Plan

See ACs 16–18, plus a pytest that `--no-auth` passes the override and a GoogleTest for the AC 20 Error line.

- **Manual:**
  1. `dia run cluichetest`, open CluicheEditor, connect with the saved URL: it works.
  2. Restart the game: the editor's auto-reconnect picks up the new token.
  3. Open a browser console on any localhost page and run `new WebSocket('ws://127.0.0.1:9002')`: it is refused.

## Resolved Decisions

1. **Scripts outside the repo use `dia launch --no-auth` or move to `dia query`** (user decision 2026-11-10). See AC 19. Ad-hoc tools that connect to 9002 directly are expected to move to `dia query` or the shared client. `--no-auth` is the escape hatch for the rest.
2. **When the record can't be written, the server stays locked** (user decision 2026-11-10). See AC 20. The server never falls back to `auth: none` on its own.
