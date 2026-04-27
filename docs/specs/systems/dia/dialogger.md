# System Spec: DiaLogger

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaLogger is the engine-wide logging system for the Dia engine. It provides structured, levelled, channel-tagged logging with a zero-contention write path (thread-local ring buffers), pluggable sinks via `ISink`, and a `DIA_LOG` macro as the single entry point for all log output. DiaLogger is a singleton that any module above DiaCore can call, but it does not manage its own lifecycle — concrete applications (CluicheEditor, CluicheTest, future games) own a `LoggerModule` that registers sinks, flushes buffers per frame, and tears everything down on shutdown.

## Responsibilities

- Provide `DIA_LOG(level, channel, fmt, ...)` macro as the universal logging entry point
- Define log levels: Trace, Debug, Info, Warning, Error
- Compile out Trace/Debug levels in Release builds
- Tag every log entry with a `StringCRC` channel (e.g. "Rendering", "Physics", "Editor", "GameConnection")
- Maintain per-thread ring buffers (1024 entries) with zero contention on the write path
- Flush all registered thread buffers into sinks on demand (called once per frame by the owning `LoggerModule`)
- Define `ISink` interface for pluggable log output destinations
- Support multiple sinks registered simultaneously, each with independent level threshold and channel filter
- Ship `DebugOutputSink` as the only built-in sink (wraps `Dia::Core::Log::OutputLine`)
- Provide thread buffer registration/unregistration for threads that want to log
- Singleton access via `Dia::Logger::Logger::Instance()`
- Drop oldest entries when a thread buffer is full before flush (no blocking, no allocation)

## Non-Responsibilities

- Does NOT manage its own lifecycle (no self-init, no auto-flush)
- Does NOT provide concrete sinks beyond `DebugOutputSink` — `EditorConsoleSink`, `FileSink`, etc. are assembled at the application level
- Does NOT depend on or know about DiaApplication, ProcessingUnits, Phases, or Modules
- Does NOT replace `Dia::Core::Log` — that stays in DiaCore for Assert and low-level debug output
- Does NOT provide file I/O, log rotation, or disk persistence (that is a `FileSink` concern at the app level)
- Does NOT use mutexes on the write path

## Public Interfaces

### Core Types

**LogLevel:**
```cpp
namespace Dia::Logger {
    enum class LogLevel : unsigned char {
        kTrace,
        kDebug,
        kInfo,
        kWarning,
        kError
    };
}
```

**LogEntry:**
```cpp
namespace Dia::Logger {
    struct LogEntry {
        LogLevel level;
        Dia::Core::StringCRC channel;
        char message[1024];
        unsigned int timestamp;  // Milliseconds since logger init
    };
}
```

**ISink:**
```cpp
namespace Dia::Logger {
    class ISink {
    public:
        virtual ~ISink() {}
        virtual void OnLogEntry(const LogEntry& entry) = 0;
        virtual const char* GetName() const = 0;

        void SetLevelThreshold(LogLevel minLevel);
        LogLevel GetLevelThreshold() const;

        void SetChannelFilter(const Dia::Core::StringCRC& channel, bool enabled);
        void ClearChannelFilter();  // Accept all channels
        bool AcceptsEntry(const LogEntry& entry) const;

    protected:
        LogLevel mMinLevel = LogLevel::kInfo;
        // Channel filter: empty = accept all, non-empty = whitelist
    };
}
```

**Logger (Singleton):**
```cpp
namespace Dia::Logger {
    class Logger {
    public:
        static Logger& Instance();

        // Sink management
        void RegisterSink(ISink* sink);
        void UnregisterSink(ISink* sink);

        // Thread buffer management
        void RegisterThreadBuffer();
        void UnregisterThreadBuffer();

        // Flush all thread buffers into registered sinks
        void FlushBuffers();

        // Direct log entry (called by DIA_LOG macro)
        void Log(LogLevel level, const Dia::Core::StringCRC& channel,
                 const char* fmt, ...);

    private:
        Logger();
        ~Logger();
    };
}
```

**DIA_LOG Macro:**
```cpp
// Release: Trace and Debug compile to nothing
#if defined(NDEBUG)
    #define DIA_LOG_TRACE(channel, fmt, ...) ((void)0)
    #define DIA_LOG_DEBUG(channel, fmt, ...) ((void)0)
#else
    #define DIA_LOG_TRACE(channel, fmt, ...) \
        Dia::Logger::Logger::Instance().Log( \
            Dia::Logger::LogLevel::kTrace, \
            Dia::Core::StringCRC(channel), fmt, ##__VA_ARGS__)
    #define DIA_LOG_DEBUG(channel, fmt, ...) \
        Dia::Logger::Logger::Instance().Log( \
            Dia::Logger::LogLevel::kDebug, \
            Dia::Core::StringCRC(channel), fmt, ##__VA_ARGS__)
#endif

#define DIA_LOG_INFO(channel, fmt, ...) \
    Dia::Logger::Logger::Instance().Log( \
        Dia::Logger::LogLevel::kInfo, \
        Dia::Core::StringCRC(channel), fmt, ##__VA_ARGS__)

#define DIA_LOG_WARNING(channel, fmt, ...) \
    Dia::Logger::Logger::Instance().Log( \
        Dia::Logger::LogLevel::kWarning, \
        Dia::Core::StringCRC(channel), fmt, ##__VA_ARGS__)

#define DIA_LOG_ERROR(channel, fmt, ...) \
    Dia::Logger::Logger::Instance().Log( \
        Dia::Logger::LogLevel::kError, \
        Dia::Core::StringCRC(channel), fmt, ##__VA_ARGS__)

// Convenience: DIA_LOG(level, channel, fmt, ...) for dynamic level
#define DIA_LOG(level, channel, fmt, ...) \
    Dia::Logger::Logger::Instance().Log( \
        level, Dia::Core::StringCRC(channel), fmt, ##__VA_ARGS__)
```

### Built-in Sink

**DebugOutputSink:**
```cpp
namespace Dia::Logger {
    class DebugOutputSink : public ISink {
    public:
        void OnLogEntry(const LogEntry& entry) override;
        const char* GetName() const override { return "DebugOutput"; }
    };
}
```
Wraps `Dia::Core::Log::OutputLine` — writes formatted `[level][channel] message` to `OutputDebugStringA`.

### Events Emitted

None — DiaLogger is a passive system. Sinks receive entries during `FlushBuffers()`, which is called by the owning module.

### Data Contracts

**Thread Buffer:**
- Fixed-size ring buffer of 1024 `LogEntry` structs per thread
- `thread_local` storage — zero contention on write
- Write index advanced via simple increment (no atomics needed, single writer)
- When full, oldest entries are overwritten (ring wrap)
- Threads must call `RegisterThreadBuffer()` before logging and `UnregisterThreadBuffer()` before exit

**Flush Semantics:**
- `FlushBuffers()` iterates all registered thread buffers, drains pending entries in FIFO order
- Each entry is passed to every registered sink that accepts it (per sink level/channel filter)
- Entries are consumed (buffer reset) after flush
- Flush is single-threaded — called by the owning `LoggerModule` on the update thread

**Sink Filtering:**
- Each sink has an independent `LogLevel` threshold — entries below the threshold are skipped
- Each sink has an optional channel whitelist — empty means accept all, non-empty means only listed channels
- Filtering checked via `ISink::AcceptsEntry()` before calling `OnLogEntry()`

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| Core Logger | Singleton, DIA_LOG macro, LogEntry, LogLevel, thread-local ring buffers, RegisterThreadBuffer/UnregisterThreadBuffer, FlushBuffers | [core-logger.md](../../features/dia/dialogger/core-logger.md) | Done |
| Sink System | ISink interface with level threshold and channel filtering, DebugOutputSink built-in sink | [sink-system.md](../../features/dia/dialogger/sink-system.md) | Done |

## Log Level Taxonomy

See [Log Level Taxonomy](../../../reference/architecture/log-level-taxonomy.md) for the authoritative guide on choosing log levels, channel naming conventions, and anti-patterns. All modules using `DIA_LOG_*` must follow that document.

## Channel Registry

All `Dia::Core::Log::OutputVaradicLine` calls outside DiaCore have been migrated to `DIA_LOG_*`. The current channel assignments are:

| Channel | Modules | Typical Content |
|---------|---------|-----------------|
| `Application` | DiaApplication, CluicheEditor phases/modules | Phase transitions, module lifecycle, manifest loading, type registry |
| `Editor` | DiaEditor (plugins, MVC, WebUIBridge, LiveConnection) | Editor framework, plugin lifecycle, game connection, UI bridge |
| `DebugServer` | DiaDebugServer | Debug server lifecycle, protocol handling, client connections |
| `WebSocket` | DiaWebSocket | Client/server transport errors, queue drops, connection lifecycle |
| `API` | DiaAPI | Command registry, argument parsing, Python bindings |
| `UI` | DiaUICEF, DiaUIUltralight, DiaUIAwesomium | CEF initialization, scheme handling, console messages |
| `Physics` | DiaRigidBody2D, DiaSoftBody2D | Simulation warnings: maxSubSteps hit, velocity safety threshold exceeded, constraint drift, sleep state changes, torn constraints. Debug builds only. |

DiaCore cannot use `DIA_LOG_*` (circular dependency) and continues to use `Dia::Core::Log::OutputLine`.

## Platform Primitives Used

- **DiaCore/CRC** — `StringCRC` for channel identifiers
- **DiaCore/Strings** — `String1024` for message formatting
- **DiaCore/Core** — `Log::OutputLine` used by `DebugOutputSink`
- **C++ thread_local** — per-thread ring buffers with no mutex

## Dependencies on Other Systems

**Required:**
- **DiaCore** — StringCRC, String1024, Log (for DebugOutputSink)

**Optional:**
- None

**Dependents:**
- **DiaApplication** — calls `DIA_LOG` from modules/phases; does not manage logger lifecycle
- **DiaAPI** — calls `DIA_LOG` from command registry, argument parser, Python bindings
- **DiaWebSocket** — calls `DIA_LOG` from client/server transport
- **DiaUICEF / DiaUIUltralight / DiaUIAwesomium** — call `DIA_LOG` from UI system implementations
- **DiaEditor** — calls `DIA_LOG` from editor framework; provides `EditorConsoleSink` (concrete sink at app level)
- **DiaDebugServer** — calls `DIA_LOG` from debug server module
- **CluicheEditor** — owns `LoggerModule` that manages lifecycle, registers `EditorConsoleSink` + `DebugOutputSink`
- **CluicheTest** — owns `LoggerModule` that manages lifecycle, registers `DebugOutputSink`
- All future Dia-based applications

## Out of Scope

- Lifecycle management (owned by concrete `LoggerModule` at app level)
- File I/O, log rotation, disk persistence (owned by `FileSink` at app level)
- Network logging (future `NetworkSink`)
- Log aggregation, search, or indexing
- Integration with `DiaApplication` framework (no `Module` subclass in this system)
- Replacing `Dia::Core::Log` — Assert and DiaCore internals continue using it

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-L01 | Thread-local ring buffers with per-frame flush | Zero contention on write path; one-frame latency is acceptable; matches MessageBus PostMessage/ProcessQueue pattern | All features | Accepted | Yes |
| SD-L02 | 1024 entries per thread buffer, drop oldest on wrap | Fixed memory footprint, no allocation on log path; if flushing once per frame at 60fps, 1024 entries per thread is more than sufficient | All features | Accepted | Yes |
| SD-L03 | Singleton access | Logging is truly global infrastructure; every module needs it without plumbing; one of the few justified singletons | All features | Accepted | Yes |
| SD-L04 | Trace/Debug compile out in Release | Zero overhead in shipping builds; matches DIA_ASSERT pattern | DIA_LOG macro | Accepted | Yes |
| SD-L05 | ISink interface with per-sink filtering | Sinks decide what they care about; logger just dispatches; allows EditorConsoleSink to show Info+ while FileSink captures everything | ISink | Accepted | Yes |
| SD-L06 | Only DebugOutputSink ships with DiaLogger | Keeps DiaLogger dependency-light; concrete sinks assembled at app level where the right pieces meet (EditorView, file I/O, etc.) | All features | Accepted | Yes |
| SD-L07 | DiaLogger does not manage its own lifecycle | Concrete apps own a LoggerModule that calls RegisterSink/FlushBuffers/UnregisterSink; DiaLogger is passive | All features | Accepted | Yes |
| SD-L08 | Channel is a StringCRC, open-ended | Any module defines its own channels; no central registry; consistent with PD-001 | All features | Accepted | Yes |
| SD-L09 | 1024-char message buffer (String1024) | Sufficient for all log messages; matches existing DiaCore formatting infrastructure | LogEntry | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | Use StringCRC for all entity/component IDs | Channels identified by StringCRC. Sink registration could use StringCRC names. |
| PD-004 | Platform | No STL containers in public APIs | Use DiaCore containers for sink list, thread buffer registry. Internal thread_local buffer is plain C array. |
| PD-005 | Platform | x64 is the only supported build target | thread_local well-supported on x64 MSVC. |
| PD-006 | Platform | Visual Studio project files are source of truth | DiaLogger.vcxproj with filters. All headers/cpp files manually added. |
| PD-007 | Platform | C++20 is the required language standard | Can use constexpr, thread_local guaranteed. |
| PD-008 | Platform | Directory.Build.props owns OutDir, IntDir, etc. | DiaLogger.vcxproj must not override shared build properties. |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | Create dia.logger.architecture.module.md. |
| AD-002 | Dia App | No STL containers in public APIs | Public API uses DiaCore containers or plain arrays. |
| AD-003 | Dia App | Namespace convention: `Dia::<Module>::` | All code in `Dia::Logger::` namespace. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Threading | What happens if a thread logs without calling RegisterThreadBuffer? | Logger::Log() should check for a valid buffer. If none registered, the entry is silently dropped. This is a programming error but should not crash. |
| 2 | Threading | Is FlushBuffers thread-safe if called from multiple threads? | No — FlushBuffers is designed to be called from a single thread (the LoggerModule's update). Calling from multiple threads is undefined. Document this constraint. |
| 3 | Sinks | Can sinks be registered/unregistered during FlushBuffers? | No — sink registration is only safe between flushes (i.e. during DoStart/DoStop of LoggerModule). Document this constraint. |
| 4 | Memory | Should LogEntry use a fixed char[1024] or a pointer to a formatted string? | Fixed char[1024]. Avoids allocation on the log path. The thread-local ring buffer is a flat array of LogEntry structs — no heap involved. |
| 5 | Channels | Should there be predefined channel constants (e.g. kChannelRendering)? | Optional convenience — modules can define their own static StringCRC constants. DiaLogger itself does not mandate any channel names. |
| 6 | Migration | How do existing Dia::Core::Log::OutputVaradicLine calls migrate? | Gradually. Modules above DiaCore replace calls with DIA_LOG_INFO/WARNING/ERROR. DiaCore internal code continues using Log:: directly. No big-bang migration required. |
| 7 | Performance | What is the memory cost per thread? | 1024 entries * ~1040 bytes per LogEntry ≈ ~1MB per thread. Acceptable for a game engine with typically 2-4 threads. |
| 8 | Lifecycle | What happens to buffered entries if a thread exits without calling UnregisterThreadBuffer? | Entries are lost. RegisterThreadBuffer should be paired with Unregister. Document as a requirement for well-behaved threads. |
| 9 | Scope | Should DiaLogger provide log level names as strings (e.g. "info", "warning")? | Yes — useful for sinks that format output. Provide a static `LogLevelToString(LogLevel)` helper. |
| 10 | Filtering | Could a sink want to filter by multiple channels (whitelist)? | Yes — the channel filter is a whitelist. Empty = accept all. Non-empty = only accept listed channels. Sinks can add multiple channels. |

## Status

`Approved` — Spec approved 2026-04-21.
