**Spec:** @docs/specs/applications/dia/systems/diadebugserver/sim-hosted-command-help.md
**Status:** In Progress

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `SimHelpInfo` + limits + `SimQueryHost` overloads (old ones removed); handler-side tests | RED-GREEN: AC 1, 2, 8 | Done | sonnet | RED not captured (impl written before tests; new tests would have been compile errors). Removing the old overloads breaks every caller: update them with a short placeholder description so the build stays green; Task 5 writes the real help |
| 2 | Register-message `info` encode/decode in `SimRegistrationMessage.h`; `SimQueryHostModule` wrappers and announce path; Sim-side `mAnnounced` stores the info; `BuildSimReplayReply` carries `info` | RED-GREEN: AC 3, 8; 24 names fit the 16 KB event | Done | sonnet | Fixed Task 1 bug: DynamicArrayC::RemoveAt left a destroyed shared_ptr, so SimQueryHost.mHandlers is now std::vector (private member). Includes Open Question 3 (a) and (b) from the Announcement Resync interaction |
| 3 | Main catalogue in `DebugServer` (`RegisterSimHosted(name, kind, info)`, unregister, list accessor); `DebugServerHostModule` wiring; tracker staging and `SimHostedName` carry info so `ReplaceSimHosted` installs the catalogue atomically | AC 4, 9, 12; resync tests still pass | Pending | sonnet | Open Question 3 (c). Digest stays name+kind only (d): help changes only via unregister/re-register, which bumps seq |
| 4 | Consumers: DebugServer command list, palette index, schema collector read the merged list | AC 5, 6, 7 | Pending | sonnet | Run `dia check deps` first; resolve Open Question 1 (describe barrier) and report. Size L: split if it grows |
| 5 | Describe the DiaCellLayers names (24) and `sim.echo`; description-required unit test over the registrar | AC 10 | Pending | sonnet | |
| 6 | Wire E2E for help; module docs; close backlog row | AC 11 | Pending | sonnet | |
| 7 | Exhaustive test pass (gap audit, edge tests, mutation check, sanitizer) | Whole feature | Pending | sonnet | Explicit task per the exhaustive-test-pass rule |
