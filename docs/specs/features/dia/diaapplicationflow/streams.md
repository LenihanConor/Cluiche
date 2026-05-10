# Feature Spec: Streams

## Parent System
@docs/specs/systems/dia/diaapplicationflow.md

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationFlow | @docs/specs/systems/dia/diaapplicationflow.md |
| Feature | Streams | (this file) |

## Problem Statement

Modules on different PUs (different threads) need to communicate data safely. Streams are the sole inter-PU communication mechanism — framework-owned, config-declared, typed channels that replace the v1 MessageBus and ad-hoc shared state patterns.

## Acceptance Criteria

1. Two stream flavors: FrameStream (temporal, keeps latest) and EventStream (discrete, buffered)
2. Streams declared in manifest and created by the framework at Application::Start()
3. `StreamWriter<T>` — write handle, one per stream (unless multi_writer declared)
4. `StreamReader<T>` — read handle, any number of consumers
5. `EventStreamWriter<T>` — sends discrete events
6. `EventStreamReader<T>` — consumes buffered events
7. Handles are constructed in module member initializers with owner module + stream ID
8. Framework injects the underlying stream into handles after module creation (based on manifest reads/writes)
9. FrameStream: writer calls `Write(data, timestamp)`, reader calls `FetchLatest()` or `FetchClosestTo(time)`
10. EventStream: writer calls `Send(event)`, reader calls `Consume(outArray)` which drains buffer
11. Thread safety: FrameStream uses double-buffer (lock-free read of latest), EventStream uses mutex-protected queue
12. Streams are framework-owned — modules do not create or destroy them
13. MessageBus is removed — EventStream replaces all inter-module event passing

## Public API

```cpp
namespace Dia::ApplicationFlow {

    // FrameStream — temporal data (keeps latest frame)
    template<typename T>
    class StreamWriter {
    public:
        StreamWriter(Module* owner, const StringCRC& streamId);
        void Write(const T& data, TimeAbsolute timestamp);
        bool IsConnected() const;
    };

    template<typename T>
    class StreamReader {
    public:
        StreamReader(Module* owner, const StringCRC& streamId);
        const T* FetchLatest() const;
        const T* FetchClosestTo(TimeAbsolute time) const;
        bool IsConnected() const;
    };

    // EventStream — discrete events (buffered, consumed once)
    template<typename T>
    class EventStreamWriter {
    public:
        EventStreamWriter(Module* owner, const StringCRC& streamId);
        void Send(const T& event);
        bool IsConnected() const;
    };

    template<typename T>
    class EventStreamReader {
    public:
        EventStreamReader(Module* owner, const StringCRC& streamId);

        template<unsigned int N>
        void Consume(DynamicArrayC<T, N>& outEvents);

        bool HasPending() const;
        bool IsConnected() const;
    };
}
```

## Internal Architecture

```
Application owns:
  FrameStreamStore<T> — double-buffered slot per stream ID
  EventStreamStore<T> — mutex-protected queue per stream ID

StreamWriter/Reader hold:
  - Pointer to owning module (for debug/validation)
  - StringCRC streamId
  - Pointer to underlying store slot (injected by framework after creation)
```

## Thread Safety Model

**FrameStream (double-buffer):**
- Writer writes to back buffer, then atomically swaps front/back pointer
- Reader always reads front buffer — no lock needed
- Multiple readers safe (all read same front buffer)
- Single writer assumed (multi_writer requires external synchronization or framework lock)

**EventStream (mutex queue):**
- Writer locks mutex, pushes to queue, unlocks
- Reader locks mutex, drains queue into outArray, unlocks
- Multiple readers each get disjoint events? No — all readers see all events (fan-out)
- Consume drains only for the calling reader's buffer (per-reader copy)

## Per-Reader Buffering for EventStream

Each EventStreamReader gets its own buffer. When a writer sends an event:
1. Framework copies the event into each registered reader's buffer
2. Consume() drains that reader's buffer only
3. Readers can consume at different rates without affecting each other

## Connection Lifecycle

1. Application::Start() creates stream stores from manifest declarations
2. Modules are created — StreamWriter/Reader constructed with stream ID
3. Framework matches module's declared reads/writes (from manifest) to stream stores
4. Injects store pointer into each handle (SetStream internal method)
5. If manifest doesn't list a stream in module's reads/writes, handle remains disconnected (IsConnected() = false)
6. Streams persist for application lifetime — not affected by stage transitions

## Files Touched

| File | Action |
|------|--------|
| `Dia/DiaApplicationFlow/Streams/StreamWriter.h` | New |
| `Dia/DiaApplicationFlow/Streams/StreamReader.h` | New |
| `Dia/DiaApplicationFlow/Streams/EventStreamWriter.h` | New |
| `Dia/DiaApplicationFlow/Streams/EventStreamReader.h` | New |
| `Dia/DiaApplicationFlow/Streams/FrameStreamStore.h` | New — double-buffer implementation |
| `Dia/DiaApplicationFlow/Streams/EventStreamStore.h` | New — per-reader queue |
| `Dia/DiaApplicationFlow/MessageBus.h` | Delete (v1) |
| `Dia/DiaApplicationFlow/MessageBus.cpp` | Delete (v1) |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj` | Update |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj.filters` | Update |

## Dependencies

- **Config Format v2** (this system) — stream declarations in manifest
- **Module Lifecycle** (this system) — Module base class (owner pointer)
- **DiaCore/CRC/StringCRC** — stream IDs
- **DiaCore/Time/TimeAbsolute** — timestamps for FrameStream
- **DiaCore/Containers/DynamicArrayC** — event buffers
- **DiaCore/Threading** — std::atomic for double-buffer swap, std::mutex for event queues

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all IDs | Compliant — stream IDs are StringCRC |
| PD-004 | Platform | No STL in public APIs | Compliant — DynamicArrayC for Consume output. Internal use of std::atomic and std::mutex is implementation detail. |
| PD-007 | Platform | C++20 | Compliant |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | Compliant — Dia::ApplicationFlow:: |
| SD-001 | DiaAppFlow | Config is sole source of truth | Compliant — streams declared in manifest |
| SD-006 | DiaAppFlow | Streams in config, framework-owned | Compliant — this feature implements that decision |
| SD-007 | DiaAppFlow | MessageBus removed, EventStream replaces | Compliant — MessageBus deleted, EventStream is the replacement |
| SD-015 | DiaAppFlow | No shared modules across PUs | Compliant — streams are the cross-PU mechanism instead |
| SD-017 | DiaAppFlow | Clean break | Compliant — MessageBus removed |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | EventStream | Fan-out (all readers see all events) vs. competing consumers (each event goes to one reader)? | Fan-out. Each reader gets a copy. This matches the use case: InputToSim has one reader, but if multiple modules needed input they'd each need all events. |
| 2 | FrameStream | Should FetchClosestTo do interpolation or just nearest-timestamp? | Nearest-timestamp. Double-buffer only holds one frame (latest). FetchClosestTo is future-proofing for ring-buffer extension. For now it returns latest (same as FetchLatest). |
| 3 | Lifetime | Should streams be created lazily (first write) or eagerly (Application::Start)? | Eagerly. Created at Start from manifest. Handles connect immediately. No lazy path. |
| 4 | Multi-writer | How does multi_writer work with double-buffer? | Multi-writer FrameStream uses a mutex on write instead of lock-free swap. Acceptable — multi-writer is opt-in for rare cases. |
| 5 | Capacity | What's the EventStream buffer capacity? | Fixed-size ring buffer per reader (configurable, default 256 events). If full, oldest events dropped with warning log. |
| 6 | Typing | How does the framework know the concrete type T for a stream? | It doesn't at manifest level (type is a string). The framework creates type-erased stores. StreamWriter<T>/StreamReader<T> do the typed cast. Type mismatch is a compile error (different template instantiation = won't link). |

## Open Questions

None.

## Status

`Approved` — 2026-05-09
