# Feature Spec: Sink System

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaLogger | @docs/specs/systems/dia/dialogger.md |
| Feature | Sink System | (this document) |

## Problem Statement

The Core Logger writes entries into thread-local buffers and flushes them, but has no way to deliver those entries anywhere useful. The engine needs a pluggable output system where each destination (debug console, editor panel, file, network) can independently filter by level and channel, and where concrete sinks are assembled at the application level without DiaLogger knowing about them.

## Acceptance Criteria

1. `ISink` abstract interface with `OnLogEntry(const LogEntry&)` and `GetName()` pure virtuals
2. Each sink has a `LogLevel` threshold — entries below the threshold are skipped before calling `OnLogEntry`
3. Each sink has an optional channel whitelist using `DynamicArrayC<StringCRC, 16>`
4. Empty channel whitelist means accept all channels
5. Non-empty channel whitelist means only entries matching a listed channel are accepted
6. `ISink::SetLevelThreshold(LogLevel)` and `ISink::GetLevelThreshold()` for level filtering
7. `ISink::SetChannelFilter(StringCRC, bool)` to add/remove channels from the whitelist
8. `ISink::ClearChannelFilter()` to reset to accept-all
9. `ISink::AcceptsEntry(const LogEntry&)` checks both level threshold and channel whitelist
10. `Logger::FlushBuffers()` calls `AcceptsEntry` then `OnLogEntry` for each sink per entry
11. `DebugOutputSink` ships with DiaLogger as the only built-in sink
12. `DebugOutputSink::OnLogEntry` formats as `[LEVEL][channel] message` and calls `Dia::Core::Log::OutputLine`
13. `DebugOutputSink::GetName()` returns `"DebugOutput"`
14. Sinks registered/unregistered via `Logger::RegisterSink(ISink*)` / `Logger::UnregisterSink(ISink*)`
15. Max 8 sinks registered simultaneously (`kMaxSinks` from Core Logger feature)
16. Sink registration is only safe between flushes (during `LoggerModule::DoStart`/`DoStop`)

## API Shape

### ISink
```cpp
// Dia/DiaLogger/ISink.h
namespace Dia::Logger {
    class ISink {
    public:
        virtual ~ISink() {}
        virtual void OnLogEntry(const LogEntry& entry) = 0;
        virtual const char* GetName() const = 0;

        void SetLevelThreshold(LogLevel minLevel);
        LogLevel GetLevelThreshold() const;

        void SetChannelFilter(const Dia::Core::StringCRC& channel, bool enabled);
        void ClearChannelFilter();
        bool AcceptsEntry(const LogEntry& entry) const;

    protected:
        ISink();

    private:
        LogLevel mMinLevel;

        static const unsigned int kMaxChannelFilters = 16;
        Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, kMaxChannelFilters> mChannelWhitelist;
    };
}
```

### DebugOutputSink
```cpp
// Dia/DiaLogger/DebugOutputSink.h
namespace Dia::Logger {
    class DebugOutputSink : public ISink {
    public:
        void OnLogEntry(const LogEntry& entry) override;
        const char* GetName() const override { return "DebugOutput"; }
    };
}
```

**Output format:** `[WARNING][Rendering] Failed to load texture foo.png`

## Files Touched

**New files (Dia/DiaLogger/):**
- `ISink.h` / `ISink.cpp`
- `DebugOutputSink.h` / `DebugOutputSink.cpp`

**Modified:**
- `Dia/DiaLogger/DiaLogger.vcxproj` — add new files
- `Dia/DiaLogger/DiaLogger.vcxproj.filters` — add new files
- `Dia/DiaLogger/Logger.cpp` — `#include "ISink.h"` and implement sink dispatch in `FlushBuffers`

## Tasks

| # | Task | Description |
|---|------|-------------|
| 1 | Implement ISink | Abstract interface, level threshold, channel whitelist with DynamicArrayC, AcceptsEntry logic |
| 2 | Implement DebugOutputSink | Format `[LEVEL][channel] message`, call `Dia::Core::Log::OutputLine` |
| 3 | Wire sink dispatch into FlushBuffers | For each flushed entry, iterate sinks, check AcceptsEntry, call OnLogEntry |
| 4 | Add files to project | Update .vcxproj and .vcxproj.filters |
| 5 | Build and verify | Compile, register DebugOutputSink, log entries, verify output in Visual Studio Output window |

## Binding Decisions Compliance

| ID | Decision | Compliance |
|----|----------|------------|
| PD-001 | Use StringCRC for all IDs | Compliant — channel whitelist uses `StringCRC` |
| PD-004 | No STL containers in public APIs | Compliant — `DynamicArrayC<StringCRC, 16>` for channel whitelist, no STL in public interface |
| PD-005 | x64 only | Compliant |
| PD-006 | VS project files are source of truth | Compliant — files added to `.vcxproj` |
| PD-007 | C++20 required | Compliant |
| PD-008 | Directory.Build.props owns shared settings | Compliant |
| AD-001 | YAML frontmatter module docs | Compliant — covered by Core Logger module doc |
| AD-002 | No STL in public APIs | Compliant — see PD-004 |
| AD-003 | Namespace `Dia::<Module>::` | Compliant — `Dia::Logger::` |
| SD-L05 | ISink with per-sink filtering | Compliant — this feature implements SD-L05 |
| SD-L06 | Only DebugOutputSink ships with DiaLogger | Compliant — only built-in sink; EditorConsoleSink, FileSink live at app level |
| SD-L07 | Logger does not manage own lifecycle | Compliant — sinks registered by external LoggerModule |
| SD-L08 | Channel is open-ended StringCRC | Compliant — whitelist is StringCRC-based, no hardcoded channels |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | ISink | Should ISink provide a default mMinLevel in the constructor? | Yes — default to `LogLevel::kInfo`. Sinks see Info+ out of the box. Trace/Debug require explicit opt-in via `SetLevelThreshold`. |
| 2 | Filtering | Is linear scan of DynamicArrayC<StringCRC, 16> fast enough for AcceptsEntry? | Yes — 16 entries max, StringCRC comparison is a single uint32 compare, and this runs once per entry per sink during flush (not on the hot log path). |
| 3 | DebugOutputSink | Should DebugOutputSink include the channel name string in output? | Yes — `StringCRC::AsChar()` provides the original string in debug builds. In release, the CRC value could be printed instead, but for DebugOutputSink (which is a debug tool) this is fine. |
| 4 | Registration | Should RegisterSink assert if the sink is already registered? | Yes — double registration is a programming error. DIA_ASSERT and return early. |
| 5 | Lifetime | Who owns the ISink instance? | The caller. Logger stores raw pointers. The LoggerModule at app level creates sinks and must unregister + delete them in DoStop. |
| 6 | Formatting | Should ISink provide a default format helper, or leave formatting entirely to each sink? | Leave to each sink. Different sinks want different formats (DebugOutputSink wants `[LEVEL][channel] msg`, EditorConsoleSink wants raw fields for the UI). A shared helper would over-constrain. |
| 7 | Channel filter | Can a sink switch from whitelist mode back to accept-all? | Yes — `ClearChannelFilter()` empties the whitelist, returning to accept-all mode. |

## Open Questions

None — all resolved.

## Status

`Done` — Implemented and builds clean 2026-04-21.
