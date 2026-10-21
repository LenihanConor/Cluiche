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
  forbidden: []
public_api:
  headers:
    - DiaValueSchema/ValueSchemaVersion.h
    - DiaValueSchema/ValueInfo.h
    - DiaValueSchema/ValueSchemaRegistry.h
    - DiaValueSchema/ValueSchemaLoader.h
    - DiaValueSchema/ValueClamper.h
    - DiaValueSchema/SchemaDirectoryLoader.h
    - DiaValueSchema/ValueSchemaService.h
    - DiaValueSchema/IValueSchemaContributor.h
    - DiaValueSchema/ValueSchemaHealth.h
    - DiaValueSchema/ValueSchemaModule.h
  namespaces:
    - Dia::ValueSchema::
  entry_points:
    - kFormatVersion
    - ValueSchemaModule
    - ValueSchemaService
    - IValueSchemaContributor
responsibilities:
  - Sidecar *.schema.json format definition and parsing
  - ValueInfo registry for storing and querying schema metadata
  - ValueSchemaRegistry for managing schema registrations
  - ValueSchemaModule and ValueSchemaService lifecycle integration
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
| `ValueSchemaService` | Read-only `Find` / `Find(for, key)` / `ForFormat` / `FindEvent` / `Count` over the published registry |
| `IValueSchemaContributor` | `ContributeEntries(ValueSchemaRegistry&)`: code-side entries (Features 4 / 7), called before publish |
| `ValueSchemaHealth` | `IHealthReporter` `dia.valueschema`: Degraded when any file failed, OK otherwise |
| `ValueSchemaModule` | `Module`, `PUAffinity::kAny`; loads, publishes the `ValueSchemaService` ServiceStream; `AddContributor`, `GetService`, `GetClamper`, `GetLoadStats`, `GetHealthReporter` |

## ValueSchemaModule

At start: resolve every `schema_roots` alias with `PathStore` → walk each directory recursively for
`*.schema.json` → sort the combined list by full path (normalised to `/`; a file reachable from two
roots loads once) → load each file (a bad file is skipped with an Error; duplicate full keys: first
wins) → call each `IValueSchemaContributor` → `Publish` → register `ValueSchemaService` on the
`ValueSchemaService` ServiceStream. An unregistered alias or missing directory is a Warning.

The module derives from `Module` directly with `PUAffinity::kAny` (no per-frame work), so a manifest
can place it on any PU; consumers on other PUs read through the stream without locks. DoStop
deregisters the stream; the registry and service live until the next start or module destruction.

Observation: trace zone `ValueSchema.Load` and profile scope `ValueSchema.LoadFile` (category
`kDiaAssetRuntime`); gauges `valueschema.entries_loaded`, `valueschema.files_failed`; counter
`valueschema.clamp.count` (bound to `GetClamper()`); logs on channel `VSC`.

### .diaapp module config

Applications adopt it by adding the module, the ServiceStream and a `schema_roots` alias list
(aliases must be registered path aliases; the config string is limited to 255 characters):

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
    "config": { "schema_roots": ["data", "cow_schema"] },
    "channels": [ { "id": "ValueSchemaService", "role": "provides" } ]
  }
]
```

Consumers declare `{ "id": "ValueSchemaService", "role": "consumes" }` and hold a
`ServiceStreamReader<ValueSchemaService>`.
