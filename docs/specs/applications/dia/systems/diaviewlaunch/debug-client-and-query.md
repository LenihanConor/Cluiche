# Feature Spec: Shared Debug Client and `dia query`

**Parent:** @docs/specs/applications/dia/systems/diaviewlaunch/diaviewlaunch.md (Feature 3)
**Status:** Approved (2026-11-10)
**Depends on:** @docs/specs/applications/dia/systems/diaviewlaunch/discovery-record.md, @docs/specs/applications/dia/systems/diaviewlaunch/connection-token.md

## Summary

A script or AI agent can ask a running game any DebugServer query or command from the shell and get JSON back, with no editor and no window: `dia query cells.history.view --game cow --args '{"layer": "weather.precipitation"}'`. The Python DebugServer client that lives only in the E2E tests today moves into DiaCLI, so `dia query`, the E2E harness and later `dia view` share one client. Replies get an optional `request_id`, so concurrent requests can't be confused.

## Goals

- One shell command reaches any query or command the game exposes. That covers the QueryRegistry, sim-hosted names and DiaAPI commands.
- Results are JSON on stdout, with exit codes a script can branch on.
- One Python client, with request ids, used by both DiaCLI and E2E.

## Acceptance Criteria

### Request ids (DiaDebugProtocol, DiaDebugServer)

1. `CommandRequest` gains `string request_id = 3;` and `CommandResponse` gains `string request_id = 5;` in `debug_protocol.proto`. Generated code is regenerated.
2. The DebugServer copies the request's `request_id` into the response on every routing path: QueryRegistry, sim-hosted (the id is held with the Main-side pending entry across the SimPU round trip, including timeouts) and DiaAPI. It also does this for error replies and the unknown-command reply.
3. A request without `request_id` gets a response without one, exactly as today. The editor's C++ client and existing tests are unchanged.

### Shared client (`dia_cli/utils/debug_client.py`)

4. `DiaClient` moves from `Cluiche/Tests/E2E/client.py` to `dia_cli/utils/debug_client.py`, with its public methods unchanged (`connect`, `send_command`, `subscribe_topic`, `wait_for_topic`, `report`, `navigate_to`, `quit`). `Cluiche/Tests/E2E/client.py` becomes a one-line re-export, so no scenario changes.
5. The client connects to `ws://127.0.0.1:<port>/?token=<hex>` (Feature 2). It drains the unsolicited `HANDSHAKE_RESPONSE` and `GAME_INFO` and exposes `game_info` (name, build).
6. Every `send_command` sends a fresh `request_id` (uuid4 hex) and matches the reply by it. A reply with no `request_id` (an older game build) falls back to matching by command name, as today. Two threads sending the same command at once each get their own reply.
7. Construction helper: `DiaClient.from_discovery(game=None, session=None, port=None)` resolves a live record (Feature 1's `find_live`) and returns a client configured with its port and token.

### `dia query` (`dia_cli/cli/query.py`)

8. Usage: `dia query <name> [--args <json>] [--game <name> | --session <id> | --port <n>] [--timeout <seconds, default 10>]`.
   - It prints exactly one JSON object to stdout: `{ok, game, session_id, port, name, result}` on success, or `{ok: false, …, error: {code, message}}` on failure.
   - Diagnostics go to stderr.
9. **Game resolution:**
   - With no flag, it uses the only live game.
   - `--game` matches the record's game name (case-insensitive) or the `.diagame` file stem.
   - `--session` matches exactly.
   - `--port` uses the live record on that port, for the token.
   - When nothing matches, exit 2 (`no_game`). When more than one matches, exit 3 (`ambiguous`), and the error lists the candidates.
10. **Identity check:** if `GAME_INFO.name` differs from the record's game, exit 2 (`stale_record`).
11. **Exit codes:**
    - 0 ok
    - 2 no matching live game (or a stale record)
    - 3 ambiguous
    - 4 connection or token refused
    - 5 command error reply
    - 6 timeout
    - 1 bad CLI usage (invalid `--args` JSON).
12. **Error replies:** `success: false` is exit 5, with the message. A sim-hosted error that arrives as `success: true` with a payload whose only key is `error` is also exit 5 (the known sim-hosted quirk; see Open Design Question 1).
13. `dia query --list [--filter <glob>]` prints the merged command catalogue from `list_commands` (names, descriptions, categories, params, sim-hosted included).
14. `dia query games` stays as defined in Feature 1. `games` is a reserved name. A game command literally named `games` is reachable as `dia query --name games`.
15. `dia query` appears in DiaConsole automatically, because DiaConsole reflects the Click tree. A pytest asserts it is registered.
16. The token never appears in stdout, stderr or the error object.

### Large replies

17. A `command_response` whose payload exceeds the server's cap (measured in Task 1) is replaced by `success: false` with the message `reply_too_large (<bytes> > <cap>)`, and keeps its `request_id`. `dia query` exits 5 with `error.code = "reply_too_large"`. A GoogleTest sends a payload just over the cap and asserts the error. If Task 1 finds no truncation path, this AC is recorded as not applicable, with the measurement.

### Docs

18. `dia query --help` gives examples for a query, a sim-hosted command and `--list`. The DiaCLI command reference lists the command and its exit codes.

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaDebugProtocol/proto/debug_protocol.proto` + generated code | `request_id` fields |
| `Dia/DiaDebugServer/DebugServer.cpp` (+ the sim-hosted pending entries) | Echo `request_id` on every path |
| `Dia/DiaCLI/dia_cli/utils/debug_client.py` (moved from `Cluiche/Tests/E2E/client.py`) | Shared client, request ids, `from_discovery` |
| `Cluiche/Tests/E2E/client.py` | Re-export |
| `Dia/DiaCLI/dia_cli/cli/query.py` | `dia query <name>`, `--list` (added to Feature 1's `games`) |
| `Cluiche/Tests/GoogleTests/DebugServer/TestDebugServerRequestId.cpp` (new) | Echo tests |
| `Dia/DiaCLI/tests/test_debug_client.py`, `test_query_cmd.py` (new) | pytest |
| `Cluiche/Tests/E2E/scenarios/cluichetest/test_dia_query.py` (new) | E2E |

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| VL-001 | Scriptable first: JSON output and exit codes | AC 8–12 |
| VL-011 | Optional, echoed `request_id`; old clients unaffected | AC 1–3, 6 |
| SED-010 | DiaDebugProtocol for wire types | The fields are added to `debug_protocol.proto` |
| DDS-008 | DebugServer forwards commands to the DiaAPI CommandRegistry | `dia query` uses `command_request`, so it reaches the same routing (QueryRegistry, then sim-hosted, then DiaAPI) as the editor |
| DDS-004 | DebugServer is Debug-only | Against a Release build no record exists, so the result is exit 2 `no_game` |
| AD-001 | Engine API changes need sign-off | Sign-off needed for DiaDebugProtocol (proto fields) and DebugServer (echo) |

## Test Plan

- **GoogleTests:**
  - `request_id` echoed on the QueryRegistry, DiaAPI, sim-hosted, sim-hosted timeout, error and unknown-command replies
  - absent when not sent
- **pytest:**
  - against a fake DebugServer (a `websockets` sync server in the test):
    - every exit code
    - request-id matching with two concurrent same-name requests
    - name fallback when the reply has no id
    - draining handshake and game info
    - the stale-record identity check
    - the sim-hosted `{error}` payload
    - the token absent from all output
  - resolution against fixture discovery trees: none, one, two, by game, session and port
- **E2E:**
  - with CluicheTest running, `dia query <report command> --game cluichetest` exits 0 with the current stage
  - `dia query no.such.command` exits 5
  - `dia query --list` includes a sim-hosted name
  - the existing E2E suite passes on the moved client.

## Resolved Decisions

1. **Sim-hosted errors are detected by shape here; the wire fix is a backlog row** (user decision 2026-11-10). AC 12 treats a `success: true` reply whose payload has only an `error` key as exit 5. Making `SimQueryHost` send `success: false` is tracked in BACKLOG, because it means re-checking every client that reads `{error}` today (the E2E `_vc` shim, the editor).
2. **Large replies fail loudly** (user decision 2026-11-10). Plan Task 1 measures the size cap on the `command_response` path (`data_update` uses a 64 KB stack buffer; sim-hosted replies are capped at about 16 KB). If a reply can be truncated, the server sends an explicit error instead (see AC 17), never truncated JSON.
