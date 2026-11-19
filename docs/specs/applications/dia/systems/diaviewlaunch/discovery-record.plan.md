# Plan: Game Discovery Record (DiaViewLaunch Feature 1)

**Spec:** @docs/specs/applications/dia/systems/diaviewlaunch/discovery-record.md
**Status:** Not Started
**Order:** this plan first, then `connection-token.plan.md` (Feature 2), then `debug-client-and-query.plan.md` (Feature 3). Those plans are written when each starts.

## Sign-offs needed before Task 1 (AD-001)

| Change | Where | Status |
|---|---|---|
| New `Discovery/` folder (`DiscoveryRecord`, read/write, liveness, scan) | DiaDebugProtocol | Ask |
| `Server::GetBoundPort()`, port 0 support | DiaWebSocket | Ask |
| `DebugServer::GetBoundPort()`, `"port": "auto"` config | DiaDebugServer | Ask |
| `ObservationSessionService` ServiceStream writer + `ObservationSessionInfo` type | DiaApplicationFlow | Ask |

Ask once for all four before dispatch. `DebugServerHostModule` (CluicheGameBaseline), the two `.diaapp` files and DiaCLI need no sign-off.

## Implementation Patterns

### Record type and I/O (Tasks 2–3)
- Namespace `Dia::DebugProtocol`. Files `Dia/DiaDebugProtocol/Discovery/DiscoveryRecord.h/.cpp`, added with `dia docs vcxproj-add DiaDebugProtocol "Discovery\\DiscoveryRecord.h" --filter Discovery` (from PowerShell or with literal paths; the bash-loop bug is a known Loose End).
- Fields use fixed storage: `Dia::Core::Containers::String64/String128/String512` set through a bounded copy helper (never `str = cstr`, which formats: the `String<N>` footgun Loose End). Integers: `uint32_t pid`, `uint64_t processStart` (FILETIME), `uint16_t port`; `bool stopped` + `String32 stoppedUtc`.
- JSON through the Dia Json wrapper (`DiaCore/Json/`), never raw jsoncpp in the header.
- `enum class DiscoveryReadResult { Ok, Missing, Malformed, WrongFormat, UnsupportedVersion }`.
- Atomic write: write `discovery.json.tmp`, then `MoveFileExW(tmp, final, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`.
- Liveness: `OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION)` + `GetProcessTimes` creation FILETIME compared with `processStart`. A pid that can't be opened is dead. Wrapped behind a small `IProcessProbe` (pid → optional creation time) so tests inject dead/reused pids; the default probe is Win32.
- Scan: `FindFirstFileW` over `<outRoot>\*\sessions\*\discovery.json`. Result is `DiscoveryRecordList` = `DynamicArrayC<DiscoveryRecord, 32>`, sorted newest `startedUtc` first; overflow count returned in the result struct. `DiscoveryFilter` holds optional game / session / port (empty = any).
- Test helpers ship with the library (testing-utilities rule): `Dia/DiaDebugProtocol/Testing/DiscoveryFixture.h` builds temp trees and a fake `IProcessProbe`.

### Bound port (Task 4)
- `Dia::WebSocket::Server::GetBoundPort()` reads `mServer.get_local_endpoint()` after `listen` (0 before listening or on failure). Port 0 passes straight through to `listen("127.0.0.1", "0")`.
- `DebugServer`: `SetPort` stays `uint16_t`; `"auto"` is mapped to 0 in `DebugServerHostModule::OnConfigure`. `StartServer`'s log line prints `GetBoundPort()`.

### Session service (Task 5)
- Follow `JobSystemModule`: `Dia::ApplicationFlow::ServiceStreamWriter<ObservationSessionInfo> mSessionService{this, "ObservationSessionService"};` published after `SessionManager::Start` succeeds. `ObservationSessionInfo` is a plain struct (`String64 sessionId`, `String512 sessionDir`) owned by the module.
- If Task 1 shows an undeclared writer warns or fails, guard the publish on the stream being declared (resolved decision 1).

### Writing the record (Task 6)
- `DebugServerHostModule` gets `ServiceStreamReader<ObservationSessionInfo>`, connected in `OnConnectStreams` like `mDebugPushReader`.
- A small pure helper `DiscoveryRecordBuilder` (in CluicheGameBaseline) assembles the record from server and session state, so it can be tested without an Application.
- `bool mRecordWritten` latch; checked in `Update` (`IsListening()` and not written → write). Rewritten with `stopped_utc` in `DoStop`.
- Session id passed to `StartObservationBridge(sessionId, …)` instead of `""`.

### Python reader (Task 8)
- `dia_cli/utils/discovery.py`: `DiscoveryRecord` dataclass, `read_record(path) -> (result, record)`, `is_live(record, probe=...)`, `find_live(out_root=None, game=None, session=None, port=None)`. Liveness via `ctypes.windll.kernel32` `OpenProcess`/`GetProcessTimes`; injectable probe for tests. Standard library only (no psutil).
- `dia_cli/cli/query.py`: a Click command `query` with `NAME` argument; in this feature only `games` is handled; any other name prints `{ok:false, error:{code:"not_implemented"}}` with exit 1 until Feature 3.
- Fixture trees shared with the C++ tests where practical: `Dia/DiaDebugProtocol/Testing/fixtures/discovery/` (JSON files), copied into temp dirs by both test suites.

### Verification cadence
- Each task runs scoped tests: `dia run googletest --filter="*Discovery*:*WebSocketBoundPort*:*ObservationSession*:*DebugServerHost*" --shards 2` and/or `dia test cli -k discovery`.
- The full suite, sanitizer and Release build run once, in Task 10.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 0 | Get the four AD-001 sign-offs. Record the baseline: current `GoogleTests` pass count (one scoped run of `*DebugServer*:*WebSocket*:*Observation*`) and whether the E2E log-error check runs | — | Done | haiku | No code. Gate for every later task; AD-001 sign-offs given 2026-11-16 (all four). Baseline run in progress; AD-001 sign-offs given 2026-11-16 (all four). Baseline scoped run (*DebugServer*:*WebSocket*:*Observation*, 2 shards): both shards print 185 tests from 41 suites, PASSED. E2E log-error check not confirmed here; verified in Task 10 |
| 1 | **Spike (resolved decision 1):** what does `ServiceStreamWriter` do when its stream id is not declared in the `.diaapp`? Read `ServiceStreamWriter.h` + `ServiceStreamStore`, then prove with a throwaway GoogleTest. Also confirm DiaDebugProtocol may link DiaCore Json + Win32 (vcxproj deps, module doc `dependent_modules`) | Throwaway test result quoted in notes | Not Started | sonnet | Decides whether Task 5 guards the publish. Delete the throwaway test |
| 2 | RED → GREEN: `DiscoveryRecord`, `WriteDiscoveryRecord` (atomic), `ReadDiscoveryRecord` + all five result cases; `Testing/DiscoveryFixture.h`; module doc updated | AC 1–3: round trip; `.tmp` only → `Missing`; malformed; wrong format; version 2 → `UnsupportedVersion`; unknown keys ignored | Not Started | sonnet | `dia docs vcxproj-add` for new files (DiaDebugProtocol + GoogleTests) |
| 3 | RED → GREEN: `IProcessProbe` + Win32 probe, `IsDiscoveryRecordLive`, `FindLiveDiscoveryRecords` with filter, ordering and cap | AC 4–5: live (self), dead pid, reused pid (same pid, other start), stopped; scan tree with two apps, a stopped record, a malformed file, a non-session folder; filter by game (name and `.diagame` stem, case-insensitive), session, port; 33 records → 32 + overflow 1 | Not Started | sonnet | Mutation check on the liveness rule and filter match only (pure logic) |
| 4 | RED → GREEN: `Server::GetBoundPort()`, port 0; `DebugServer::GetBoundPort()`; `"port": "auto"` in `DebugServerHostModule::OnConfigure`; bound port in the `StartServer` log | AC 13–14: `SLOW_` loopback test: port 0 → non-zero bound port that accepts a connection; integer port unchanged; `auto` maps to 0 | Not Started | sonnet | Use port 0 in the new test (avoids the shared-port SLOW flake) |
| 5 | RED → GREEN: `ObservationSessionInfo` + `ObservationSessionService` writer in `ObservationModule`; no publish and one log on session failure; guard per Task 1 | AC 6–7: provider publishes id + dir after a successful start; nothing on failure | Not Started | sonnet | No statics. Mirror `JobSystemModule` |
| 6 | RED → GREEN: `DebugServerHostModule` reads the session service; session id to `StartObservationBridge`; `DiscoveryRecordBuilder`; write once when listening (covers `auto_start: false`); none on listen failure; `stopped_utc` on `DoStop`; Warning and no record when the session is missing | AC 8–12, 21: builder fields from fake server/session; latch writes once; no write when not listening; stop rewrite; missing-session path logs once | Not Started | sonnet | Token field stays empty (Feature 2) |
| 7 | Manifests: declare `ObservationSessionService` (provider `ObservationModule`, consumer `DebugServerHostModule`) in `cluiche_main.diaapp` and `cow_main.diaapp`; leave `main.diaapp.j2` alone; `dia validate manifest` | AC 15–17: validate passes for both; `dia run cluichetest` writes a live record with port 9002 (quote the file) | Not Started | haiku | Ports stay 9002 |
| 8 | RED → GREEN: `dia_cli/utils/discovery.py` (reader, liveness via ctypes, `find_live`) and `dia query games`; shared JSON fixtures | AC 18–19: pytest over the fixture tree (same cases as Tasks 2–3); `dia query games` JSON shape, empty list exits 0, token never printed | Not Started | sonnet | `dia test cli -k discovery` |
| 9 | Exhaustive test pass: gap audit of every AC against tests, edge cases (long paths near 512, non-ASCII session dir, record for a game in a different `out/<App>`, two records for one port, clock skew in `started_utc` ordering), `/gen-tests` audit on `DiscoveryRecord.h` | Gap list in notes, new tests green | Not Started | sonnet | Mutation checks already done in Task 3 |
| 10 | Final verification: full `dia run googletest --shards 2`; `dia check sanitizer`; Release build of GoogleTests; full E2E on a quiet machine (log-error check still active, AC 20); manual: CoW with `"port": "auto"` + CluicheTest at once, `dia query games` lists both; observation scan (5 pillars) in notes; `dia check deps`; mark spec Done-ready, update system features table + BACKLOG | AC 20 + all; quoted pass/fail | Not Started | sonnet | Kill leftover `CluicheTest.exe` before E2E. Revert any `auto` port edit used for the manual check |
