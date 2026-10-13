# Feature Spec: Bus Traffic Recorder

**Parent:** @docs/specs/applications/dia/systems/diamessagebus/diamessagebus.md
**Status:** Done
**Origin:** CoW F-1 Harness Test 6 ("Recorded Traffic Survives the Run"); `docs/specs/applications/cow/dia-needs.md` #1.

## Problem Statement

Bus traffic can only be inspected in memory while the process runs. The last-tick ledger holds counts only (no payloads) and is gone when the process exits, so there is no way to inspect a run afterwards or to build canned message data from one. This feature adds a Debug-only recorder hook to `Bus` and a default file recorder that writes every dispatched message to a versioned binary file. The application owns, creates and injects the recorder; DiaMessageBus knows nothing about paths or lifecycle policy.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | With a recorder set, every dispatched message (Primary and Reaction pass) produces exactly one record, in dispatch order | GoogleTest: post mixed Primary/Reaction traffic, read file back, compare order and count |
| AC-2 | Each record carries tick, type id, router id, pass and the raw payload bytes | GoogleTest: round-trip a known payload, assert every field |
| AC-3 | The file begins with a header (magic + format version); a reader rejects an unknown version | GoogleTest: corrupt the version, assert the reader reports failure |
| AC-4 | A file written during a run is complete and readable after the recorder is closed and the process has ended | GoogleTest: close recorder, reopen file from a fresh reader; plus CoW F-1 Test 6 |
| AC-5 | With no recorder set the bus behaves identically: no allocation, no file I/O | GoogleTest: existing bus tests pass unchanged; code review of the null-check path |
| AC-6 | With `DIA_DEBUG` off (Release) the hook and the recorder compile out entirely | Release build of GoogleTests + code review of `#ifdef` coverage |
| AC-7 | A message with no subscribers is still recorded | GoogleTest: post a type with zero subscribers, assert one record |
| AC-8 | A failed open or write logs once, disables the recorder, and never crashes or stalls the bus | GoogleTest: open an unwritable path, post traffic, assert bus delivery is unaffected and exactly one warning is logged |
| AC-9 | A 10,000-message burst round-trips with count, order and payloads matching | GoogleTest |
| AC-10 | With a recorder set, dispatching a message type that is not trivially copyable trips a `DIA_ASSERT`; with no recorder set nothing is checked and every existing message type still compiles | GoogleTest/code review. *Amended: was a compile-time `DIA_ASSERT_STATIC`, which broke existing types containing `StringCRC` (user-provided copy ctor)* |
| AC-11 | The recorded tick comes from the injected tick source, not the bus; `Open` fails (and logs) if no tick source is supplied | GoogleTest: supply a fake tick source, assert recorded ticks match it |
| AC-12 | `bufferSizeBytes` in `Config` controls the write buffer; a small buffer still produces a complete, ordered file | GoogleTest: 64-byte buffer round-trip |

## Design

### IBusRecorder

A sink interface in the style of `IFlushAdapter`. The bus holds a non-owning pointer; the caller keeps the recorder alive and clears it (`SetRecorder(nullptr)`) before destroying it.

```cpp
// DiaMessageBus/IBusRecorder.h
namespace Dia::MessageBus {

    class IBusRecorder {
    public:
        virtual ~IBusRecorder() = default;

        // Called once per dispatched message, before subscribers run.
        // The recorder obtains the tick itself from its injected tick source;
        // the bus has no opinion on what a "tick" means to the application.
        virtual void OnDispatch(Dia::Core::StringCRC typeId,
                                Dia::Core::StringCRC routerId,
                                Pass pass,
                                const void* payload,
                                uint32_t payloadSize) = 0;
    };

}
```

### Bus hook

```cpp
#ifdef DIA_DEBUG
    void SetRecorder(IBusRecorder* recorder);   // non-owning; nullptr clears
#endif
```

In `Bus::DispatchOne<T>`, before the ledger update and the subscriber loop (so unsubscribed messages are still recorded):

```cpp
#ifdef DIA_DEBUG
    if (mRecorder) {
        DIA_ASSERT(std::is_trivially_copyable_v<T>, "Bus recorder requires trivially copyable message types");
        mRecorder->OnDispatch(T::kTypeId, addr.routerId, currentPass, &msg, sizeof(T));
    }
#endif
```

`mRecorder` is a `#ifdef DIA_DEBUG` member initialised to `nullptr`.

### FileBusRecorder

Default `IBusRecorder` implementation in `DiaMessageBus/FileBusRecorder.h/.cpp`, Debug-only.

- `bool Open(const char* path, const Config& config)`, `void Flush()`, `void Close()`; destructor closes.
- Code-configured via a plain struct (no config file for now):

```cpp
struct FileBusRecorder::Config {
    using TickFn = uint64_t (*)(void* context);   // plain fn pointer, no std::function (PD-004)
    TickFn   tickSource  = nullptr;               // required; Open fails if null
    void*    tickContext = nullptr;               // passed to tickSource
    uint32_t bufferSizeBytes = 64 * 1024;         // write buffer; flushes to disk when full
};
```

- The tick source is injected by the application (CoW passes its own world-tick counter; CluicheTest passes whatever it has). The bus does not supply a tick.
- Buffered writes. Data is guaranteed on disk after `Flush()` or `Close()` (the destructor calls `Close()`). Not crash-safe by design.
- On open or write failure: log one `DIA_LOG_WARNING`, set a `mDisabled` flag, drop all further records.
- No STL in the public header (PD-004); the file handle is held privately.

### File format (version 1, little-endian)

```
Header : magic "DBTR" (4 bytes) | version u32 (=1)
Record : tick u64 | typeId u32 | routerId u32 | pass u8 | pad u8[3] | payloadSize u32 | payload bytes[payloadSize]
```

Type and router ids are `StringCRC` values. A reader needs no schema to skip records, since every record carries its payload size. The reader itself is out of scope.

### Ownership split

| Concern | Owner |
|---|---|
| `IBusRecorder`, `Bus::SetRecorder`, dispatch hook, `FileBusRecorder`, file format | DiaMessageBus (Dia) |
| Whether to record, output path, creating the recorder, `SetRecorder`, flush/close on stage exit | Application (CoW, CluicheTest) |
| Replay reader | Out of scope (deferred to CoW V-3) |

## Files Touched

- `Dia/DiaMessageBus/Bus.h`, `Bus.cpp`
- New `Dia/DiaMessageBus/IBusRecorder.h`, `FileBusRecorder.h`, `FileBusRecorder.cpp` (add via `dia docs vcxproj-add`)
- `Dia/DiaMessageBus/dia.messagebus.architecture.module.md`
- New `Cluiche/Tests/GoogleTests/DiaMessageBus/TestBusTrafficRecorder.cpp`. A small in-memory recorder and file-reader test helper ships in `DiaMessageBus/Testing/`, per the test-utilities convention.

## Binding Decisions

| ID | Decision | How this feature complies |
|---|---|---|
| SD-MBX2-008 | Bus is single-threaded (sim thread) | The recorder is called synchronously from `DispatchOne` on the sim thread; no locking. |
| SD-MBX2-012 | Routing is by `TypeKey<T>()`; `kTypeId` is display metadata | The record stores `T::kTypeId` as display identity only; routing is untouched. |
| PD-004 / AD-002 | No STL in public APIs | `IBusRecorder` and `FileBusRecorder` public headers use only primitives and DiaCore types. |
| PD-008 | Directory.Build.props owns OutDir etc. | New files are added to the existing vcxproj without overriding build properties. |
| Engine/app boundary | Generic capability in Dia, wiring in the app | Mechanism lives in DiaMessageBus; ownership and injection stay in the application. |

## Open Design Questions

1. **Trivially-copyable check (resolved).** The original `DIA_ASSERT_STATIC` failed to compile for existing types containing `StringCRC`, so it became a runtime `DIA_ASSERT` active only when a recorder is set. Original note: **`DIA_ASSERT_STATIC` blast radius.** AC-10 makes any non-trivially-copyable message type a Debug build error. A scan of existing `*_messages.h` structs found no STL or container members, but the full solution should be rebuilt to confirm before merge. If one fails, it should become POD rather than weaken the check. Note `DIA_ASSERT_STATIC` is gated on `DEBUG` while the hook is gated on `DIA_DEBUG`; confirm the two are defined together in Debug.
2. **Tick source shape.** A function pointer plus context avoids `std::function` in the public header and allows applications to inject any tick source they need. DiaSimTime exists (with `SimTimeContext.tick`), but applications have varying tick requirements: CoW may want world domain clock ticks (not root tick), CluicheTest stages may not have DiaSimTime set up, test harnesses may inject simple counters. The function pointer design is correct as-is - flexible, Debug-only, and application-controlled per the ownership split.

## Status

Approved.

**Plan:** [bus-traffic-recorder.plan.md](bus-traffic-recorder.plan.md) — In Progress (no tasks started)
