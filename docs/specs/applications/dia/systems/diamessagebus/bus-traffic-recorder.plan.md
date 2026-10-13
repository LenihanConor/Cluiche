**Spec:** @docs/specs/applications/dia/systems/diamessagebus/bus-traffic-recorder.md
**Status:** In Progress

> Feature plan for one feature spec. Nothing is implemented yet. Tasks follow TDD (RED, GREEN, REFACTOR); prove RED with quoted output before writing the implementation. All Dia changes need the user's sign-off per change (CoW AD-001), which the approved spec covers.

## Implementation Patterns

Patterns to validate against the real code before a task is marked Done.

| Task | Pattern / convention intended |
|------|-------------------------------|
| 1 | `IBusRecorder` mirrors `IFlushAdapter`: header-only pure interface, virtual dtor, non-owning pointer held by `Bus`. The interface enables multiple implementations (FileBusRecorder for local disk, plus future NetworkBusRecorder for debug servers, in-memory ring buffers, multi-recorders, etc.) — applications inject any implementation via `SetRecorder()`. `mRecorder` and `SetRecorder` are both inside `#ifdef DIA_DEBUG`. Hook sits at the top of `Bus::DispatchOne<T>`, before the ledger update and the subscriber loop, so unsubscribed messages are recorded. `DIA_ASSERT_STATIC(std::is_trivially_copyable_v<T>, BusRecorderRequiresTriviallyCopyableMessage)` inside the same `#ifdef`. Members `m`-prefixed, constants `k`-prefixed, namespace `Dia::MessageBus`. |
| 2 | `FileBusRecorder` public header uses only primitives and DiaCore types (PD-004, no STL); the file handle is private (pimpl-free `FILE*` or platform handle in the `.cpp`). `Config` is a plain struct with a function-pointer `TickFn` plus `void* tickContext`, no `std::function`. Failure path: one `DIA_LOG_WARNING("DiaMessageBus", ...)`, set `mDisabled`, drop all later records. Whole class compiled only under `DIA_DEBUG`. |
| 3 | Test helpers ship with the library in `Dia/DiaMessageBus/Testing/` (not in GoogleTests): an in-memory `IBusRecorder` and a `BusTrafficFileReader` that validates the header and iterates records. GoogleTest file `TestBusTrafficRecorder.cpp` follows `TestDiaMessageBusCore.cpp` conventions. |
| 4 | Module doc updated through `dia.messagebus.architecture.module.md` frontmatter (public API list). New files registered with `dia docs vcxproj-add DiaMessageBus ... --filter ...`; no OutDir/IntDir overrides (PD-008). |

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `IBusRecorder.h`, `Bus::SetRecorder`, dispatch hook with `DIA_ASSERT_STATIC` (AC-1, 2, 5, 6, 7, 10). Add an in-memory test recorder to `DiaMessageBus/Testing/` | `TestBusTrafficRecorder` hook tests: one record per dispatched message in Primary and Reaction order, fields correct, no-subscriber message recorded, no recorder = unchanged bus behaviour | Done | sonnet | TDD, prove RED first. Rebuild the whole solution to confirm every registered message type satisfies the static assert (spec open question 1). Confirm `DEBUG` and `DIA_DEBUG` are both defined in Debug; RED: C1083 missing IBusRecorder.h; GREEN 48/48. AC-10 relaxed to runtime DIA_ASSERT (spec amended) |
| 2 | File format v1 and `FileBusRecorder`: `Config`, `Open`, buffered writes, `Flush`, `Close`, header (magic `DBTR`, version 1), injected tick source (AC-3, 4, 8, 11, 12) | Writer tests: header present, version check rejects unknown version, `Open` fails when no tick source, small-buffer (64 byte) round-trip complete and ordered, unwritable path logs exactly one warning and the bus keeps delivering | Done | sonnet | Prereq: 1. Needs `BusTrafficFileReader` in `Testing/` to verify (write it in this task, test-first); RED: C1083 missing FileBusRecorder.cpp; GREEN 55/55. Warning count via ISink capture |
| 3 | 10,000-message burst round-trip, count, order and payload match (AC-9). Release-config build check that hook and recorder compile out (AC-6) | Burst GoogleTest; Release|x64 build of DiaMessageBus and GoogleTests | Not Started | sonnet | Prereq: 2. Run `dia run googletest --filter="*MessageBus*"` plus a full run for regression |
| 4 | Housekeeping: `dia docs vcxproj-add` for the new files, module doc public API, spec Status to In Progress or Done, DiaMessageBus system plan gets a one-line pointer here | `dia check deps`, `dia docs precommit` | Not Started | haiku | Prereq: 3. Update spec status via `dia docs spec-done` when all ACs pass. Observation scan: note any missed instrumentation (e.g. a `dia.msgbus.recorded` counter) as suggestions only |

## Out of Scope

CoW wiring (output path, tick source, flush and close on stage exit) is a separate CoW task. The replay reader is deferred to CoW V-3.
