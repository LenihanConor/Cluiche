# Log Locations and Parsing Guide

## DiaCLI Pipeline Logs

Written during `dia run` / `dia pipeline` / `dia launch`:

- **Path:** `Cluiche/out/DiaCLI/logs/<system>/last-run.ndjson`
- **Format:** NDJSON (`dia.output.v1`), one JSON object per line
- **Key fields:** `event`, `system`, `stage`, `step`, `level`, `message`, `durationMs`, `ts` (Unix float)
- **Useful events:** `OnStepFailed`, `OnStageFailed`, `OnRunFailed`, `OnLogLine` (with `level: "error"`)

Quick parse — errors only:
```powershell
Get-Content Cluiche/out/DiaCLI/logs/pipeline/last-run.ndjson |
  ConvertFrom-Json | Where-Object { $_.event -in @('OnStepFailed','OnStageFailed','OnRunFailed') -or ($_.event -eq 'OnLogLine' -and $_.level -eq 'error') }
```

## DiaObservation Runtime Logs (C++ sessions)

Written by `SessionManager` during app execution:

- **Base path:** `<outRootDir>/sessions/<sessionId>/`
- **`log.jsonl`** — application logs; fields: `ts_unix_nano`, `level`, `channel`, `thread_id`, `msg`
- **`trace.jsonl`** — span traces; fields: `trace_id`, `span_id`, `name`, `start_unix_nano`, `end_unix_nano`
- **`profile.jsonl`** — frame profiling scopes
- **`metric.jsonl`** — metric snapshots every 100 ms (configurable)
- **`metrics-final.json`** — aggregated min/max/avg/stddev at session end
- **`health.json`** — health reporter status at exit
- **`session.json`** — metadata: version, build, exit reason

Typical `outRootDir`: `Cluiche/out/` — look for the most-recently-modified session directory.

Quick parse — warnings and errors from last session:
```powershell
$session = (Get-ChildItem Cluiche/out/sessions -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
Get-Content "$session/log.jsonl" | ConvertFrom-Json | Where-Object { $_.level -in @('warning','error') }
```

Log levels (C++ macros → JSONL `level` values):

| Macro | Level string | Notes |
|-------|-------------|-------|
| `DIA_LOG_TRACE` | `trace` | Debug builds only |
| `DIA_LOG_DEBUG` | `debug` | Debug builds only |
| `DIA_LOG_INFO` | `info` | — |
| `DIA_LOG_WARNING` | `warning` | — |
| `DIA_LOG_ERROR` | `error` | — |

Channel names (e.g. `"Graphics"`, `"WebSocket"`, `"Render"`) are stored as `StringCRC` internally but decoded to strings in the JSONL output.

Per-channel log levels are controlled via `.diaobservation` config embedded in asset manifests.

## MSBuild / Compile Logs

- **Path:** `Cluiche/bin/intermediate/<ProjectName>/<Config>/x64/<ProjectName>.log`
- DiaCLI runs MSBuild with `/v:minimal` — errors and warnings only in the log.
- For full verbosity: `msbuild ... /v:detailed`

## GoogleTest Output

- No file by default — output goes to stdout/terminal.
- Capture to file: `dia run googletest > out.txt` or pass `--gtest_output=xml:results.xml`
- Filter pattern: `--filter="SuiteName*"` (passed through `dia run googletest --filter=...`)

## Observation Config (`.diaobservation`)

Embedded as `"observation"` key in `.diaapp`/`.diagame`/`.diastage` manifests:

```json
{
  "schema": "diaobservation/1.0",
  "log_level": "info",
  "log_channels": { "WebSocket": "warning", "Render": "debug" },
  "sinks": { "stdout": true, "observation_file": true, "trace_file": false, "metrics_file": true },
  "metrics": { "snapshot_interval_ms": 100 },
  "health": { "file": true, "poll_interval_ms": 500 }
}
```

## Key Source Files

| Purpose | Path |
|---------|------|
| DiaCLI output events | `Dia/DiaCLI/dia_cli/utils/dia_output.py` |
| Session manager (C++) | `Dia/DiaObservation/Session/SessionManager.h` |
| Log macros (C++) | `Dia/DiaObservation/Log/DiaLog.h` |
| Observation config loader | `Dia/DiaObservation/Config/ObservationConfigLoader.cpp` |
