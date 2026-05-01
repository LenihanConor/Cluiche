# Feature Spec: Core Logger

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaLogger | @docs/specs/systems/dia/dialogger.md |
| Feature | Core Logger | (this document) |

## Problem Statement

The Dia engine has no structured logging system — all output goes through `Dia::Core::Log::OutputVaradicLine` which is a thin `OutputDebugStringA` wrapper with no levels, channels, filtering, or ability to route output to multiple destinations. Modules across the engine need a zero-contention, structured logging path that works from any thread.

## Acceptance Criteria

1. `Logger` singleton accessible via `Dia::Logger::Logger::Instance()`
2. `LogLevel` enum with values: `kTrace`, `kDebug`, `kInfo`, `kWarning`, `kError`
3. `LogEntry` struct containing: `LogLevel level`, `StringCRC channel`, `char message[1024]`
4. `DIA_LOG(level, channel, fmt, ...)` macro writes a formatted `LogEntry` into the calling thread's ring buffer
5. `DIA_LOG_TRACE` and `DIA_LOG_DEBUG` macros compile to `((void)0)` in Release builds (`NDEBUG` defined)
6. `DIA_LOG_INFO`, `DIA_LOG_WARNING`, `DIA_LOG_ERROR` convenience macros work in all builds
7. Each thread gets a `thread_local` ring buffer of 1024 `LogEntry` structs
8. Writing to the ring buffer involves no mutexes, no atomics, no heap allocation
9. When a ring buffer is full, the oldest entry is overwritten (ring wrap)
10. `Logger::RegisterThreadBuffer()` registers the calling thread's buffer with the singleton
11. `Logger::UnregisterThreadBuffer()` removes the calling thread's buffer
12. Thread buffer registry protected by a mutex (only hit on register/unregister, never on log write)
13. `Logger::FlushBuffers()` drains all registered thread buffers in FIFO order, dispatching entries to registered sinks
14. After flush, buffers are reset (read index catches up to write index)
15. `Logger::Log()` performs `va_args` formatting into `char[1024]` and writes the baked entry into the ring buffer
16. If a thread logs without having called `RegisterThreadBuffer()`, the entry is silently dropped
17. `LogLevelToString(LogLevel)` static helper returns `"trace"`, `"debug"`, `"info"`, `"warning"`, `"error"`

## API Shape

### LogLevel
```cpp
// Dia/DiaLogger/LogLevel.h
namespace Dia::Logger {
    enum class LogLevel : unsigned char {
        kTrace,
        kDebug,
        kInfo,
        kWarning,
        kError
    };

    const char* LogLevelToString(LogLevel level);
}
```

### LogEntry
```cpp
// Dia/DiaLogger/LogEntry.h
namespace Dia::Logger {
    struct LogEntry {
        LogLevel level;
        Dia::Core::StringCRC channel;
        char message[1024];
    };
}
```

### ThreadLogBuffer
```cpp
// Dia/DiaLogger/ThreadLogBuffer.h
namespace Dia::Logger {
    class ThreadLogBuffer {
    public:
        static const unsigned int kCapacity = 1024;

        ThreadLogBuffer();

        void Push(const LogEntry& entry);
        bool Pop(LogEntry& outEntry);

        unsigned int PendingCount() const;
        bool IsEmpty() const;
        void Reset();

    private:
        LogEntry mEntries[kCapacity];
        unsigned int mWriteIndex;
        unsigned int mReadIndex;
    };
}
```

### Logger
```cpp
// Dia/DiaLogger/Logger.h
namespace Dia::Logger {
    class ISink;  // Forward declaration

    class Logger {
    public:
        static Logger& Instance();

        // Sink management (Feature 2)
        void RegisterSink(ISink* sink);
        void UnregisterSink(ISink* sink);

        // Thread buffer management
        void RegisterThreadBuffer();
        void UnregisterThreadBuffer();

        // Flush all thread buffers into registered sinks
        void FlushBuffers();

        // Write a formatted log entry to the calling thread's buffer
        void Log(LogLevel level, const Dia::Core::StringCRC& channel,
                 const char* fmt, ...);

    private:
        Logger();
        ~Logger();

        static const unsigned int kMaxThreadBuffers = 8;
        static const unsigned int kMaxSinks = 8;

        ThreadLogBuffer* mThreadBuffers[kMaxThreadBuffers];
        unsigned int mThreadBufferCount;
        std::mutex mRegistryMutex;

        ISink* mSinks[kMaxSinks];
        unsigned int mSinkCount;
    };
}
```

### DIA_LOG Macros
```cpp
// Dia/DiaLogger/DiaLog.h
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

#define DIA_LOG(level, channel, fmt, ...) \
    Dia::Logger::Logger::Instance().Log( \
        level, Dia::Core::StringCRC(channel), fmt, ##__VA_ARGS__)
```

## Files Touched

**New files (Dia/DiaLogger/):**
- `LogLevel.h` / `LogLevel.cpp`
- `LogEntry.h`
- `ThreadLogBuffer.h` / `ThreadLogBuffer.cpp`
- `Logger.h` / `Logger.cpp`
- `DiaLog.h` (macros)
- `ISink.h` (forward declaration only — full interface is Feature 2)

**New project files:**
- `Dia/DiaLogger/DiaLogger.vcxproj`
- `Dia/DiaLogger/DiaLogger.vcxproj.filters`

**Modified:**
- `Cluiche/Cluiche.sln` — add DiaLogger project
- `Directory.Build.props` — if DiaLogger needs additional include paths

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Create DiaLogger project | `.vcxproj`, `.vcxproj.filters`, add to solution, set up include paths and DiaCore dependency |
| 2 | Implement LogLevel | Enum + `LogLevelToString` helper |
| 3 | Implement LogEntry | Struct with level, channel, message[1024] |
| 4 | Implement ThreadLogBuffer | Ring buffer: Push, Pop, PendingCount, Reset |
| 5 | Implement Logger singleton | Instance(), thread buffer registry with mutex, FlushBuffers, Log() with va_args formatting |
| 6 | Implement DIA_LOG macros | All macro variants, Release compile-out for Trace/Debug |
| 7 | Build and verify | Compile DiaLogger, verify no circular dependencies, verify thread_local works |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | Use StringCRC for all IDs | Compliant — channels are `StringCRC` |
| PD-004 | No STL containers in public APIs | Compliant — public API uses plain C arrays and fixed-size buffers. `std::mutex` is internal only (registry). |
| PD-005 | x64 only | Compliant — `thread_local` well-supported on x64 MSVC |
| PD-006 | Visual Studio project files are source of truth | Compliant — new `.vcxproj` created |
| PD-007 | C++20 required | Compliant |
| PD-008 | Directory.Build.props owns shared settings | Compliant — DiaLogger.vcxproj will not override shared properties |
| AD-001 | YAML frontmatter module docs | Compliant — will create `dia.logger.architecture.module.md` |
| AD-002 | No STL in public APIs | Compliant — see PD-004 |
| AD-003 | Namespace `Dia::<Module>::` | Compliant — all code in `Dia::Logger::` |
| SD-L01 | Thread-local ring buffers with per-frame flush | Compliant — this feature implements SD-L01 |
| SD-L02 | 1024 entries, drop oldest | Compliant — `ThreadLogBuffer::kCapacity = 1024`, ring wrap overwrites oldest |
| SD-L03 | Singleton access | Compliant — `Logger::Instance()` |
| SD-L04 | Trace/Debug compile out in Release | Compliant — `#if defined(NDEBUG)` guards |
| SD-L07 | Logger does not manage own lifecycle | Compliant — no auto-init, no auto-flush; caller drives everything |
| SD-L08 | Channel is open-ended StringCRC | Compliant — no predefined channel list |
| SD-L09 | 1024-char message buffer | Compliant — `char message[1024]` in LogEntry |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Singleton | When is the Logger constructed? First call to Instance() (lazy)? | Yes — lazy via static local in Instance(). No explicit init step needed. |
| 2 | Threading | What's the max number of thread buffers? | 8 (`kMaxThreadBuffers`). Sufficient for typical game engine threads (main, render, sim, audio, worker pool). Can be increased later if needed. |
| 3 | Registry mutex | Does FlushBuffers also lock the registry mutex? | Yes — FlushBuffers takes a lock_guard on the registry mutex to safely iterate buffers. This is acceptable because flush happens once per frame, not per-log-call. |
| 4 | Format failure | What happens if fmt string produces output longer than 1024 chars? | Truncated by `snprintf` with 1024 limit. No crash, no allocation. Last char is null terminator. |
| 5 | Sink forward decl | ISink is forward-declared but RegisterSink/UnregisterSink are on Logger — is that OK for Feature 1? | Yes — Logger stores `ISink*` pointers. Feature 1 implements the storage and dispatch loop. Feature 2 implements the ISink interface itself. Logger.cpp will need ISink.h included. |
| 6 | Buffer ownership | Who allocates the ThreadLogBuffer? | `thread_local` static inside Logger.cpp. `RegisterThreadBuffer` takes the address and adds it to the registry. Deallocation happens automatically when the thread exits. |
| 7 | Flush ordering | If multiple threads have pending entries, what order are they flushed? | Per-thread FIFO, threads iterated in registration order. No global ordering guarantee across threads — this is acceptable for logging. |
| 8 | Max sinks | Is 8 sinks enough? | Yes — typical setup is 2-3 (DebugOutput, EditorConsole, File). 8 provides ample headroom. |

## Open Questions

None — all resolved.

## Status

`Done` — Implemented and builds clean 2026-04-21.
