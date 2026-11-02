**Spec:** @docs/specs/applications/dia/systems/diadebugserver/sim-hosted-announcement-resync.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `SimRegistrationMessage.h`: `seq`, digest, resync_begin, replay build/parse; `SimKind::Resync`; name-set hash | RED-GREEN: pure-function unit tests (ACs 1, 4, 11) | Done | sonnet | |
| 2 | `SimAnnounceTracker` pure state machine | RED-GREEN: tracker unit tests (ACs 2, 3, 5, 6, 7, 11) | Done | sonnet | |
| 3 | `DebugServer::ReplaceSimHosted` + `GetSimHostedDigest`; `DebugServerHostModule` wiring (tracker, resync request, atomic install) | Unit tests for swap/digest; compile + scoped DebugServer/SimHosted run | Pending | sonnet | |
| 4 | `SimQueryHost` resync handler; `SimQueryHostModule`: seq counter, digest timer, paced replay queue, overflow counter + Warning | Scoped SimHosted tests (ACs 4, 5, 10) | Pending | sonnet | |
| 5 | Enable and extend churn integration test; add tail-loss, lost-replay, lost-unregister, no-hole tests | Integration tests (ACs 6-9) | Pending | sonnet | |
| 6 | Metrics, logs, second-host guard, module docs, registry, Command Help spec note, close backlog row | Scoped run + `dia check deps` (ACs 12, 13) | Pending | sonnet | Second-host guard is behaviour, so sonnet not haiku |
| 7 | Exhaustive test pass (gap audit, edge tests, mutation check, sanitizer) | `dia check sanitizer`, full run with `--shards 2` | Pending | sonnet | |
