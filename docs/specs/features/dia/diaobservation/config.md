# Feature Spec: DiaObservation Config

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diaobservation.md | **config** |

**Status:** `Approved` — 2026-05-17

**Research:** @docs/research/observ_telemetry/summary.md

**Depends on:** Feature #2 (foundation) — `ObservationConfig` is passed to `SessionManager::Start`; implementation is serial.

---

## Problem Statement

After Feature #2, `SessionConfig` is hardcoded in `SessionModule::DoInit` and the logger uses whatever level the engine defaults to — there is no way to change log verbosity, disable console sinks, or tune per-channel thresholds without a recompile. This feature adds the runtime configuration layer: a `.diagame` `observation` block read at startup, with CLI overrides applied on top.

---

## Solution Overview

`ObservationConfigLoader` (lives in `Dia/DiaObservation/Config/`) reads the `.diagame` file's `config.observation` block and returns an `ObservationConfig` struct. `SessionModule::DoInit` calls the loader, then performs a linear `argc`/`argv` scan to apply any CLI overrides on top. The merged `ObservationConfig` is passed into `SessionManager::Start`, which wires the values into the Logger (per-channel thresholds, sink enable/disable) before the drain thread starts.

No global minimum is applied before the config is read — the Logger already has a compile-time Trace/Debug strip in Release (existing behaviour from Feature #1).

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `ObservationConfigLoader::Load(path, &config)` reads the `config.observation` block from the `.diagame` file at `path`; returns `true` on success, `false` if file missing or block absent (uses defaults) | Unit test: valid file with `observation` block → fields populated; missing file → defaults |
| AC2 | Global log level (`log_level`) sets the Logger's minimum threshold; entries below it are not passed to any sink | Unit test: set level `"warning"`, log Info entry, assert it does not reach `ObservationFileSink` |
| AC3 | Per-channel override in `log_channels` takes precedence over the global level for that channel | Unit test: global `"info"`, channel `"asset"` → `"trace"`; log Trace on `asset` channel, assert it reaches sink |
| AC4 | Channels without an explicit override use the global level | Unit test: global `"warning"`, log Info on an unconfigured channel, assert not delivered |
| AC5 | `StdOutSink` is registered/not-registered based on `sinks.stdout` (default: `true`) | Unit test: set `stdout: false`, assert `StdOutSink` is not registered after `SessionManager::Start` |
| AC6 | `DebugOutputSink` is registered/not-registered based on `sinks.debug_output` (default: `true`) | Unit test: set `debug_output: false`, assert `DebugOutputSink` is not registered |
| AC7 | `ObservationFileSink` is registered/not-registered based on `sinks.observation_file` (default: `true`) | Unit test: set `observation_file: false`, assert `ObservationFileSink` is not registered |
| AC8 | CLI `--log-level=<level>` overrides the file's `log_level` value | Unit test: file has `"info"`, CLI has `--log-level=trace`; assert effective level is Trace |
| AC9 | CLI `--log-channel=<channel>:<level>` overrides a per-channel level | Unit test: file has no `log_channels`, CLI has `--log-channel=asset:trace`; assert `asset` resolves to Trace |
| AC10 | Multiple `--log-channel` flags are all applied | Unit test: two `--log-channel` flags on different channels, assert both overrides take effect |
| AC11 | Unknown CLI flags in the `--log-*` namespace are silently ignored (no crash, no error) | Unit test |
| AC12 | An invalid level string (e.g. `"verbose"`) in the config file falls back to `"info"` and logs a warning via `StdOutSink` | Unit test |
| AC13 | An invalid level string in a CLI override falls back to the file value (or default) and logs a warning | Unit test |
| AC14 | Config is read once at startup (`DoInit`); no live reload in v1 (format must not foreclose it — no compile-time templates or hardcoded key counts) | Code review / implementation note |
| AC15 | `dia pipeline --target cluichetest` green in Debug + Release with an `observation` block in CluicheTest's `.diagame` | Build + run verification |

---

## Public API

### `ObservationConfig` struct (lives in `Dia/DiaObservation/Config/`)

```cpp
namespace Dia::Observation {

enum class LogLevelConfig : uint8_t {
    kTrace, kDebug, kInfo, kWarning, kError,
    kDefault = kInfo
};

struct ObservationConfig {
    // Log level filtering
    LogLevelConfig globalLogLevel = LogLevelConfig::kDefault;

    // Per-channel overrides — fixed-size table (max 16 channels)
    struct ChannelOverride {
        Dia::Core::StringCRC channel;
        LogLevelConfig       level;
    };
    ChannelOverride channelOverrides[16] = {};
    unsigned int    channelOverrideCount = 0;

    // Sink enable/disable
    bool enableStdOutSink         = true;
    bool enableDebugOutputSink    = true;
    bool enableObservationFileSink = true;  // default true; disable deliberately breaks E2E tooling
};

} // namespace Dia::Observation
```

### `ObservationConfigLoader` (lives in `Dia/DiaObservation/Config/`)

```cpp
namespace Dia::Observation {

class ObservationConfigLoader {
public:
    // Reads config.observation block from .diagame file at path.
    // Returns true if the block was found and parsed; false if missing (config
    // is left at defaults). Never crashes — malformed values fall back to defaults.
    static bool Load(const char* diagamePath, ObservationConfig& config);

private:
    static LogLevelConfig ParseLevel(const char* str, LogLevelConfig fallback);
};

} // namespace Dia::Observation
```

### CLI override application (in `SessionModule::DoInit`, not a public API)

```cpp
// Linear scan of argv for --log-level=X and --log-channel=channel:level
// Applied on top of ObservationConfig after Load() returns.
// Unknown --log-* flags are silently skipped.
static void ApplyCliOverrides(int argc, const char* argv[],
                              Dia::Observation::ObservationConfig& config);
```

### `.diagame` `observation` block shape

```json
"observation": {
  "log_level": "info",
  "log_channels": {
    "asset":  "trace",
    "render": "warning"
  },
  "sinks": {
    "stdout":           true,
    "debug_output":     true,
    "observation_file": true
  }
}
```

All sub-keys are optional. Missing keys use defaults. The block itself is optional — absence is equivalent to all defaults. The shape is forward-compatible: future sub-blocks (`metrics`, `traces`, `health`, `live_reload`) can be added without breaking v1 readers.

---

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaObservation/Config/ObservationConfig.h` | New |
| `Dia/DiaObservation/Config/ObservationConfigLoader.h/.cpp` | New |
| `Dia/DiaObservation/DiaObservation.vcxproj` | Add new files |
| `Dia/DiaObservation/DiaObservation.vcxproj.filters` | Add new files |
| `Cluiche/CluicheGameBaseline/Modules/SessionModule.h/.cpp` | Add `Load` + CLI scan in `DoInit`; pass `ObservationConfig` to `SessionManager::Start` |
| `Dia/DiaObservation/Session/SessionManager.h/.cpp` | `Start` accepts `ObservationConfig`; wires level thresholds + sink registration |
| CluicheTest `.diagame` file | Add `config.observation` block (or confirm it merges into existing `config`) |
| `Dia/DiaObservation/Testing/ObservationConfigFixture.h` | New test utility |

---

## Binding Decisions Compliance

| ID | Decision (summary) | Compliance |
|----|--------------------|------------|
| PD-001 | StringCRC for all identifiers | Channel names in `ChannelOverride` are `StringCRC`. `ObservationConfig` itself carries no raw-string maps in the public API. |
| PD-002 | ProcessingUnit/Phase/Module architecture | Config is read in `SessionModule::DoInit` — correct lifecycle hook. No config access outside module boundary. |
| PD-003 | Component-based entities | Orthogonal. |
| PD-004 | No STL containers in public APIs | `ObservationConfig` uses a fixed-size `ChannelOverride[16]` array, not `std::vector` or `std::map`. |
| PD-005 | x64 only | No platform-specific concerns. |
| PD-006 | VS project files are source of truth | `DiaObservation.vcxproj` updated manually. |
| PD-007 | C++20 required | No new C++20 features introduced; existing baseline applies. |
| PD-008 | Directory.Build.props owns build settings | No overrides added to vcxproj. |
| PD-009 | Generated output under `Cluiche/out/<AppName>/` | Config feature reads input; does not write output. |
| PD-010 | `.diagame` is project root file; `config` block is the config home | `observation` block slots into `config.observation` as specified by SD-O19 and PD-010. |
| AD-001 | Module system with YAML frontmatter | `dia.dia.observation.architecture.module.md` updated to declare `Config/` subsystem. |
| AD-002 | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Namespace `Dia::<Module>::` | All new code in `Dia::Observation::`. |
| SD-O01 | One module for all four pillars | `Config/` is a subdirectory of `Dia/DiaObservation/`, not a sibling module. |
| SD-O05 | `schema_version: "1.0"` on every record | Config does not emit records; N/A. |
| SD-O06 | OTel wire-format, no SDK | Config does not affect record shape. |
| SD-O19 | Config in v1: minimal surface; format must not foreclose live reload | `ObservationConfig` is a plain struct with no compile-time templates. `.diagame` block uses a flat JSON shape with explicit optional sub-blocks — a future `live_reload` sub-block can be added without breaking v1 parsing. |
| SD-O20 | Test utilities in `Dia/DiaObservation/Testing/` | `ObservationConfigFixture.h` placed there. |
| SD-O21 | DiaCore is the only required dependency | `ObservationConfigLoader` uses `DiaCore/Json` only. No `DiaApplicationFlow`, no `DiaAPI`. |
| SD-O22 | DiaCore cannot include observation headers | Config loader does not touch DiaCore internals. |

---

## Open Questions

| # | Question | Resolution |
|---|----------|------------|
| OQ1 | 16-channel override slots — is this enough? | DiaLogger's channel registry has ~10 channels today. 16 gives headroom for future pillars (trace channels, metric channels). If it becomes a constraint, Feature #3 can be amended; the struct layout is internal. Acceptable for v1. |
| OQ2 | Does CluicheTest's `.diagame` already have a `config` block, or does this feature add it? | Verify at implementation time; `ObservationConfigLoader` handles absent block gracefully (returns false, uses defaults) so this is not a blocker. |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | API | `SessionManager::Start` currently takes a `SessionConfig`. Does it now take both `SessionConfig` and `ObservationConfig`, or are they merged into one struct? | Two separate structs, both passed to `Start`. `SessionConfig` carries identity/path info (app name, build version, out dir); `ObservationConfig` carries runtime behaviour knobs. Merging them would couple concerns that have different owners and different lifetimes (identity is fixed; config could grow). Signature: `bool Start(const SessionConfig&, const ObservationConfig&)`. |
| 2 | Filtering | Where does the per-channel level check live — in `Logger::Log` (producer side) or in each sink's `OnLog`? | Producer side in `Logger::Log`. Dropping below the threshold before the entry is written to the ring means filtered entries consume no ring space, no drain-thread CPU, and no sink I/O. The existing `Logger` already has a level gate; this feature adds a channel-aware lookup before that gate. |
| 3 | Filtering | How does `Logger` look up a channel override at ~200 ns producer budget? | `ObservationConfig::ChannelOverride[16]` is a linear-scan array (max 16 entries). At 16 entries, a linear scan is ~3–5 cache-line reads — well within budget. A hash table would be faster asymptotically but overkill for ≤16 channels. The array is copied into `Logger` at `RegisterSinks` time so the Logger never holds a pointer to `ObservationConfig`. |
| 4 | CLI | `SessionModule::DoInit` receives `argc`/`argv` — how does it get them? `IModule::DoInit` does not currently carry argc/argv. | `Main.cpp` does not thread argc/argv into the module system; the `.diagame` path is hardcoded and `Application` takes only a manifest + registry. `SessionModule::DoInit` reads `__argc`/`__argv` directly from the MSVC CRT globals (`<stdlib.h>`). Windows-only but the platform is x64/Windows (PD-005). Zero framework changes required. |
| 5 | CLI | `--log-channel=asset:trace` — what if the channel string contains characters that don't map to a valid `StringCRC` construction? | `StringCRC` is constructed from any `const char*` — it never rejects input. An unrecognised channel name simply produces a CRC that no logger channel matches, so the override has no effect. No crash, no error needed. |
| 6 | Defaults | What are the compiled-in defaults when no `.diagame` and no CLI flags are present? | `globalLogLevel = kInfo`, no channel overrides, `enableStdOutSink = true`, `enableDebugOutputSink = true`. Matches current DiaLogger behaviour so existing apps see no change. |
| 7 | Forward-compat | SD-O19 says "format must not foreclose live reload". Does the JSON shape satisfy this? | Yes. The `observation` block uses plain key-value JSON with no integer indexes or positional arrays. A future live-reload reader can re-parse the same block at any time and produce a fresh `ObservationConfig`. No compile-time templates or schema codegen involved. |
| 9 | Sinks | What happens at runtime if `observation_file: false` is set — does `session.json` still get written? | Yes. `session.json` is written by `SessionManager::Stop()` regardless of sink config — it is a session manifest, not a log sink. Disabling `ObservationFileSink` only suppresses `log.jsonl`. The `files` array in `session.json` will simply omit the `log.jsonl` entry. |
| 8 | Testing | AC15 requires a `.diagame` with an `observation` block to exist in CluicheTest. Does this feature own creating/updating that file? | Yes — this feature adds the `observation` block to CluicheTest's `.diagame` as part of the implementation tasks. The block uses all defaults initially (so behaviour is unchanged); developers can then tune it. |

---

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | `ObservationConfig` struct + `LogLevelConfig` enum | AC2, AC3, AC4 | Planned | haiku | Plain header; no .cpp |
| 2 | `ObservationConfigLoader::Load` — parse `.diagame` `config.observation` block via DiaCore Json | AC1, AC2, AC3, AC4, AC5, AC6, AC12 | Planned | sonnet | Fallback to defaults on any parse error |
| 3 | `ApplyCliOverrides` — linear `argv` scan for `--log-level` and `--log-channel` | AC8, AC9, AC10, AC11, AC13 | Planned | haiku | Lives in SessionModule; ~20 lines |
| 4 | `SessionManager::Start` updated to accept `ObservationConfig`; wires level thresholds + conditional sink registration into Logger | AC2–AC7 | Planned | sonnet | Amends Feature #2 API |
| 5 | `SessionModule::DoInit` updated — calls loader, applies CLI overrides, passes merged config to `Start` | AC8, AC9, AC15 | Planned | haiku | Needs argc/argv access pattern resolved (AI-Q4) |
| 6 | Add `observation` block to CluicheTest `.diagame` | AC15 | Planned | haiku | All-defaults initially |
| 7 | Test utility `ObservationConfigFixture.h` in `Testing/` | Supporting all ACs | Planned | haiku | |
| 8 | GoogleTests — all AC1–AC14 | All ACs | Planned | sonnet | |
| 9 | Update `DiaObservation.vcxproj` + `.vcxproj.filters` | AC15 | Planned | haiku | |
| 10 | Update `dia.dia.observation.architecture.module.md` — add `Config/` subsystem | — | Planned | haiku | |

---

## Status

`Done` — 2026-05-20.
