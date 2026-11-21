# Feature Spec: Game Discovery Record

**Parent:** @docs/specs/applications/dia/systems/diaviewlaunch/diaviewlaunch.md (Feature 1)
**Status:** Approved (2026-11-10)
**Plan:** @docs/specs/applications/dia/systems/diaviewlaunch/discovery-record.plan.md

## Summary

Every running game that hosts a DebugServer writes a small `discovery.json` into its DiaObservation session folder. The record holds the pid, the bound port, the game identity, the `.diagame`, the session id and (from Feature 2) the connection token. Tools (`dia query`, `dia view`, the editor, the E2E harness) resolve "the CoW game" from these records instead of a hard-coded port, and they tell a live game from a crashed or stopped one without deleting anything. `"port": "auto"` lets two games run at once.

## Goals

- `dia query games` lists the games running now, with no port typed.
- A record left by a crashed game, or by a pid that Windows reused, never reads as live.
- CluicheTest and CoW can run at the same time (`"port": "auto"`).
- One record format and one reader in C++ (game and editor) and one in Python (DiaCLI).

## Record format

`Cluiche/out/<App>/sessions/<session-id>/discovery.json`:

```json
{
  "format": "dia.discovery",
  "version": 1,
  "pid": 41236,
  "process_start": 133761234567890123,
  "port": 9002,
  "game": { "name": "CoW", "build": "dev" },
  "diagame": "C:/GitHub/Cluiche/Cluiche/Assets/CoW/cow.diagame",
  "session_id": "20261110-213937-6cd23ed3",
  "token": "",
  "protocol_version": "2.0.0",
  "started_utc": "2026-11-10T21:39:38Z",
  "stopped_utc": null
}
```

- `process_start` is the process creation time as a Windows FILETIME (100 ns ticks since 1601, UTC). It defeats pid reuse.
- `token` is empty until Feature 2 fills it.
- `stopped_utc` is set on clean stop and `null` while running.

## Acceptance Criteria

### Record type and I/O (DiaDebugProtocol, `Dia/DiaDebugProtocol/Discovery/`)

1. `Dia::DebugProtocol::DiscoveryRecord` holds every field above in fixed-size storage (Dia strings and integers, PD-004).
2. `WriteDiscoveryRecord(const char* sessionDir, const DiscoveryRecord&)` writes the record atomically: it writes `discovery.json.tmp`, then renames it over `discovery.json`. A reader never sees a half-written file.
3. `ReadDiscoveryRecord(const char* path, DiscoveryRecord& out)` returns a `DiscoveryReadResult`: `Ok`, `Missing`, `Malformed`, `WrongFormat` (`format` ≠ `dia.discovery`) or `UnsupportedVersion` (`version` > 1). Unknown extra keys are ignored.
4. `IsDiscoveryRecordLive(const DiscoveryRecord&)` is true only when all of these hold:
   - `stopped_utc` is null
   - the pid exists
   - the process's creation time equals `process_start`.
5. `FindLiveDiscoveryRecords(const char* outRoot, const DiscoveryFilter&, DiscoveryRecordList& out)` scans `<outRoot>/*/sessions/*/discovery.json`. `outRoot` is `Cluiche/out`. It returns live records only, newest `started_utc` first. The list is capped at 32 entries; any beyond that are counted and logged. The filter is optional and can match on game name (case-insensitive, or the `.diagame` file stem), session id or port.

### Session info service (DiaApplicationFlow)

6. `ObservationModule` publishes a read-only `ObservationSessionService` ServiceStream (`ObservationSessionInfo`: session id and session directory) once `SessionManager::Start` succeeds. If the session failed to start, it publishes nothing and logs once.
7. No singleton or static is added (no-singletons rule).

### Writing the record (CluicheGameBaseline `DebugServerHostModule`)

8. `DebugServerHostModule` consumes `ObservationSessionService` through a `ServiceStreamReader`. It passes the session id to `StartObservationBridge`, replacing today's `""`.
9. Within one frame of the DebugServer starting to listen, the module writes the record into the session directory with the **bound** port. This covers `auto_start: false`, where the server starts later.
10. If the server fails to listen, no record is written.
11. On clean stop (`DoStop`), the module rewrites the record with `stopped_utc` set.
12. If the session service is missing, the module logs one Warning ("no observation session: discovery record not written"), writes no record, and the server runs as before.

### Bound port and `"port": "auto"`

13. `Dia::WebSocket::Server::GetBoundPort()` returns the port actually listened on (websocketpp local endpoint). `DebugServer::GetBoundPort()` forwards it.
14. DebugServer module config accepts `"port": "auto"`, which binds port 0, so the OS picks a free port. Integer ports work as today. The `StartServer` log line reports the bound port.
15. `cluiche_main.diaapp` and `cow_main.diaapp` keep `9002`, so the editor's saved URL still works. Switching either to `auto` is a later choice.

### Manifests

16. `cluiche_main.diaapp` and `cow_main.diaapp` declare `ObservationSessionService`, with `ObservationModule` as provider and `DebugServerHostModule` as consumer. `dia validate manifest` passes for both.
17. Apps without a `DebugServerHostModule` (for example `main.diaapp.j2`) do not declare the stream, and `ObservationModule` still starts cleanly there (see Open Design Question 1).

### Python reader (DiaCLI)

18. `dia_cli/utils/discovery.py` reads the same format, with the same result cases and the same liveness rule. It uses only the standard library: `ctypes` with `OpenProcess`/`GetProcessTimes`, because psutil is not installed. It exposes `find_live(game=None, session=None, port=None)`.
19. `dia query games` prints `{ok, games: [{game, build, diagame, port, pid, session_id, started_utc}]}`. It never prints the token, and exits 0 even when no game is running (empty list).

### Compatibility

20. The E2E harness's session lookup (`plugin.py`, newest folder under `sessions/`) and its per-test log-error check still run. Writing a file inside the session folder does not change which folder is picked.

### Observation

21. Logs (channel `DebugServer`):
    - Info: record written (path, port)
    - Info: record marked stopped
    - Warning: write failure (path, error)
    - Warning: more than 32 records found (count).

## Files Touched

| File | Change |
|---|---|
| `Dia/DiaDebugProtocol/Discovery/DiscoveryRecord.h/.cpp` (new) | Record type, read/write, liveness, scan |
| `Dia/DiaDebugProtocol/DiaDebugProtocol.vcxproj(.filters)`, module doc | New files; `dia docs vcxproj-add` |
| `Dia/DiaWebSocket/Server.h/.cpp` | `GetBoundPort()`; port 0 |
| `Dia/DiaDebugServer/DebugServer.h/.cpp` | `GetBoundPort()`, `auto` port |
| `Dia/DiaApplicationFlow/Observation/ObservationModule.h/.cpp` | `ObservationSessionService` writer |
| `Cluiche/CluicheGameBaseline/Modules/DebugServerHostModule.h/.cpp` | Session reader, record write and stop, session id to the bridge |
| `Cluiche/Assets/CluicheTest/.../cluiche_main.diaapp`, `Cluiche/Assets/CoW/cow_main.diaapp` | Stream declaration |
| `Dia/DiaCLI/dia_cli/utils/discovery.py` (new), `dia_cli/cli/query.py` (new; `games` only in this feature) | Python reader, `dia query games` |
| `Cluiche/Tests/GoogleTests/DebugProtocol/TestDiscoveryRecord.cpp` (new) | Unit tests |
| `Dia/DiaCLI/tests/test_discovery.py` (new) | pytest |

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| VL-003 | Record lives in the DiaObservation session folder | AC 8–11: written into the session directory from `ObservationSessionService` |
| SED-010 | DiaDebugProtocol is the one source of editor-game types | The record type lives in DiaDebugProtocol (user decision 2026-11-10), shared by the game, the editor (Feature 2) and its tests |
| PD-004 | No STL in public APIs | Fixed-size fields; `DiscoveryRecordList` is a `DynamicArrayC` capped at 32 |
| PD-009 | Output under `Cluiche/out/<App>/` | Records live in the existing session folders |
| PD-002 / no-singletons / PU boundaries are streams | Session info crosses modules through a ServiceStream (user decision 2026-11-10); the record is written by the MainPU module that owns the server |
| AD-001 | Engine API changes need sign-off | Sign-off needed for DiaDebugProtocol (new folder), DiaWebSocket and DebugServer (`GetBoundPort`, `auto`), and DiaApplicationFlow (`ObservationSessionService`) |

## Test Plan

- **GoogleTests:**
  - write then read round trip
  - `.tmp` left behind with no final file reads as `Missing`
  - `Malformed` / `WrongFormat` / `UnsupportedVersion`
  - live for the current process
  - dead for a pid that doesn't exist
  - dead for the current pid with a different `process_start`
  - dead when `stopped_utc` is set
  - scan over a temp tree: two apps, a stopped record, a malformed file and a non-session folder; checks ordering, filter by game, session and port, and the cap of 32
  - `Server` on port 0 reports a non-zero bound port that accepts a connection
- **pytest:**
  - the Python reader against the same fixture tree, including the liveness cases
  - `dia query games` JSON shape and an empty list
- **E2E:**
  - run CluicheTest: its session folder has a live record with the configured port
  - stop it cleanly: the record reads as stopped
  - the existing E2E suite still passes (log-error check active)
- **Manual:** run CoW with `"port": "auto"` and CluicheTest at the same time; `dia query games` lists both with different ports.

## Resolved Decisions

1. **An undeclared stream is checked first** (user decision 2026-11-10). Plan Task 1 checks what `ServiceStreamWriter` does when `ObservationSessionService` is not declared in the manifest, before the module changes. If it fails or warns, `ObservationModule` publishes only when the stream is declared, so apps without a DebugServer (for example `main.diaapp.j2`) start cleanly (AC 17).
3. **AD-001 sign-off given 2026-11-16** for all three engine edits: DiaDebugProtocol `Discovery/`, DiaWebSocket/DiaDebugServer `GetBoundPort` + `"port": "auto"`, and DiaApplicationFlow `ObservationSessionService`.
2. **Clients wait for the record, not the port** (user decision 2026-11-10). The record is written by `DebugServerHostModule` within one frame of listening (AC 9). Clients that need the token (the E2E harness, `dia query`) poll for a live record rather than for the port answering. File I/O stays out of DiaDebugServer.
