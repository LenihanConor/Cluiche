---
schema: dia.module.v1
module_id: dia.valueschema
name: DiaValueSchema
path: Dia/DiaValueSchema
parent_module_id: dia.root
layer: foundation/application
status: active
maturity: stub
version: "1.0"
dependencies:
  required:
    - dia.core
    - dia.applicationflow
    - dia.streams
    - dia.observation
    - dia.api
    - dia.entity
    - dia.dia.diadebugserver
  forbidden: []
public_api:
  headers:
    - DiaValueSchema/ValueSchemaVersion.h
    - DiaValueSchema/ValueInfo.h
    - DiaValueSchema/ValueSchemaRegistry.h
    - DiaValueSchema/ValueSchemaLoader.h
    - DiaValueSchema/ValueClamper.h
    - DiaValueSchema/SchemaDirectoryLoader.h
    - DiaValueSchema/RampRegistry.h
    - DiaValueSchema/RampSetLoader.h
    - DiaValueSchema/ValueSchemaService.h
    - DiaValueSchema/IValueSchemaContributor.h
    - DiaValueSchema/CodeSideCollector.h
    - DiaValueSchema/ComponentFieldCollector.h
    - DiaValueSchema/ComponentAttributeAudit.h
    - DiaValueSchema/ValueSchemaHealth.h
    - DiaValueSchema/ValueSchemaQuery.h
    - DiaValueSchema/ValueSchemaModule.h
  namespaces:
    - Dia::ValueSchema::
  entry_points:
    - kFormatVersion
    - ValueSchemaModule
    - ValueSchemaService
    - IValueSchemaContributor
    - ValueSchemaQueryHandler
responsibilities:
  - Sidecar *.schema.json format definition and parsing
  - ValueInfo registry for storing and querying schema metadata
  - ValueSchemaRegistry for managing schema registrations
  - ValueSchemaModule and ValueSchemaService lifecycle integration
  - valueschema.get DiaDebugServer query and snapshot Version() (Feature 3); result also carries the full `ramps` array (Feature 5)
  - ClampAndWarn utility for value validation and warning logging
non_responsibilities:
  - Validation CLI tooling
  - Tooltip UI and visualization
  - Ramp/curve animation systems
  - Data export and serialization
---
# DiaValueSchema

Sidecar schema definition and registry system for the Dia engine.

Provides machine-readable value schema definitions via *.schema.json files that pair with game/editor data files. Includes schema parsing, metadata registry, and validation utilities.

## Public API

| Type | Purpose |
|---|---|
| `ValueInfo` | One schema entry (full key `<for>/<key>`, label, description, range, enum values, ...) |
| `ValueSchemaRegistry` | `Register` / `Publish` / `Find(fullKey)` / `Find(for, key)` / `ForFormat` / `FindEvent` / `Count`; immutable after `Publish` |
| `ValueSchemaLoader::LoadFromText(text, fileName)` | Parses one `dia.value_schema` v1 file into a `LoadResult` |
| `ValueClamper` | Clamp-and-warn: one Warning per key, counts clamps, optional bound `Counter` |
| `SchemaDirectoryLoader` | `AddRoot(dir)`, `Discover()`, `FileAt(i)`, `LoadInto(registry) -> SchemaLoadStats`; plain class, no Application |
| `RampRegistry` | `Register(ramp, isDia, sourceFile) -> RampRegisterResult`, `Publish`, `Find(id)`, `Count`; Dia wins id clashes, first game file wins between game files, immutable after `Publish` |
| `RampSetLoader` | `AddDiaRoot(dir)`, `AddGameRoot(dir)`, `Load(RampRegistry&) -> RampLoadStats`; plain class, no Application |
| `ValueSchemaService` | Read-only `Find` / `Find(for, key)` / `ForFormat` / `FindEvent` / `Count` over the published registry; `FindRamp(id)` (null if unknown) / `RampCount()` |
| `IValueSchemaContributor` | `ContributeEntries(ValueSchemaRegistry&)`: code-side entries (Features 4 / 7), called before publish |
| `ValueSchemaHealth` | `IHealthReporter` `dia.valueschema`: Degraded when any schema file or whole ramp file failed, OK otherwise |
| `ValueSchemaModule` | `Module`, `PUAffinity::kAny`; loads, publishes the `ValueSchemaService` ServiceStream; `AddContributor`, `GetService`, `GetClamper`, `GetLoadStats`, `GetHealthReporter` |

## ValueSchemaModule

At start: resolve every `schema_roots` alias with `PathStore` → walk each directory recursively for
`*.schema.json` → sort the combined list by full path (normalised to `/`; a file reachable from two
roots loads once) → load each file (a bad file is skipped with an Error; duplicate full keys: first
wins) → call each `IValueSchemaContributor` → `Publish` → register `ValueSchemaService` on the
`ValueSchemaService` ServiceStream. An unregistered alias or missing directory is a Warning.

Ramps (`ramps.md`): before publishing, each `ramp_roots` alias is resolved and the file named exactly
`ramps.json` in that directory (if present) is loaded as the Dia ramp set; then every `*.ramps.json`
under the `schema_roots` directories (recursive, sorted full path, de-duplicated) loads as game ramps.
Dia ramps register first and win id clashes (the game ramp is rejected with an Error naming both files);
between game files the first in sorted order wins. A whole-file failure is an Error, counted in
`RampLoadStats::mFilesFailed` and degrades health (reason `valueschema.ramp_files_failed`); a single bad
ramp inside a good file is skipped (logged, counted in `mRampsRejected`) and does not degrade health.
Without `ramp_roots` there are no Dia ramps and no error. Apps add a `dia_data` alias pointing at
`Dia/DiaValueSchema/Data` and list it in `ramp_roots`; tests load by direct path via `RampSetLoader`.

The module derives from `Module` directly with `PUAffinity::kAny` (no per-frame work), so a manifest
can place it on any PU; consumers on other PUs read through the stream without locks. DoStop
deregisters the stream; the registry and service live until the next start or module destruction.

Code-side pass (Feature 4): after sidecars, ramps and contributors and before `Publish`, DoStart runs
`CodeSideCollector::Collect` (profile scope `ValueSchema.CollectCodeSide`, `kDiaAssetRuntime`) on an
`ICodeSideSource` (default module-owned `LiveCodeSideSource`; `SetCodeSideSource` injects one for tests) and
keeps the result in `GetCodeSideReport()`. Info log: `code-side: <M> metrics (<d> described, <u> undescribed),
<C> commands, <A> args (<ad> described)`. Config `"code_side": false` (default true) skips the pass.
Late registrations (AC 6): metrics/commands registered after publish are not in the immutable service.
`LateRegistrationCount()` = source generation deltas since collection; `CheckLateRegistrations()` logs one
Warning per changed non-zero count (`LateWarningCount()` for tests). It is the hook `valueschema.get` answering
and describe-mode dumps (Features 3 and 6, not built) are to call; nothing calls it automatically.

Observation: gauges `valueschema.metrics_described`, `valueschema.metrics_undescribed`,
`valueschema.commands_described` (commands collected), `valueschema.args_described`; trace zone `ValueSchema.Load` and profile scope `ValueSchema.LoadFile` (category
`kDiaAssetRuntime`); gauges `valueschema.entries_loaded`, `valueschema.files_failed`, `valueschema.ramps_loaded`,
`valueschema.ramp_files_failed`; profile scope `ValueSchema.LoadRamps` per ramp file; counter
`valueschema.clamp.count` (bound to `GetClamper()`); logs on channel `VSC`.

### .diaapp module config

Applications adopt it by adding the module, the ServiceStream, a `schema_roots` alias list and
(optionally) a `ramp_roots` alias list for the Dia ramp set. Aliases must be registered path aliases.
The whole config string is limited to 255 characters (`String256`): keep alias lists short.

```json
"streams": [
  { "id": "ValueSchemaService", "kind": "ServiceStream", "payload_type": "ValueSchemaService", "from": "SimPU" }
],
"modules": [
  {
    "instance_id": "ValueSchemaModule",
    "type_id": "ValueSchemaModule",
    "stages": ["all"],
    "dependencies": [],
    "config": { "schema_roots": ["data", "cow_schema"], "ramp_roots": ["dia_data"] },
    "channels": [ { "id": "ValueSchemaService", "role": "provides" } ]
  }
]
```

Consumers declare `{ "id": "ValueSchemaService", "role": "consumes" }` and hold a
`ServiceStreamReader<ValueSchemaService>`.
