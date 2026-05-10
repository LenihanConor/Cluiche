# Feature Spec: Config Format v2

## Parent System
@docs/specs/systems/dia/diaapplicationflow.md

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationFlow | @docs/specs/systems/dia/diaapplicationflow.md |
| Feature | Config Format v2 | (this file) |

## Problem Statement

DiaApplicationFlow needs a manifest schema that fully describes application topology — PUs, modules, stages, streams, dependencies, and timeouts — in a single parseable JSON format. This is the file format that config-as-source-of-truth reads from.

## Acceptance Criteria

1. Manifest is a JSON file (`.diaapp` extension)
2. `"version": 2` field identifies the schema version
3. `"stages"` array declares all stage names
4. `"initial_stage"` names the stage entered on Application::Start()
5. `"auto_stages"` array lists stages that auto-advance
6. `"streams"` array declares all inter-PU streams with id, type, from, to
7. `"processing_units"` array declares PUs with instance_id, frequency_hz, dedicated_thread, modules
8. Each module entry has: instance_id, type_id, stages, dependencies
9. Optional module fields: start_timeout_ms, stop_timeout_ms, reads, writes, config
10. PU startup order = array order in `processing_units`
11. `ApplicationManifest` C++ struct represents the parsed manifest in memory
12. `ApplicationManifestLoader` deserializes JSON into ApplicationManifest via DiaSerializer
13. Manifest merging: stage-specific `.diaapp` files are merged into the base manifest (modules added to existing PUs)
14. `.diastage` file format: `{ "name": "StageName", "manifest": "path.diaapp" }`

## Schema

```json
{
    "version": 2,
    "stages": ["Boot", "DummyStage"],
    "initial_stage": "Boot",
    "auto_stages": ["Boot"],
    "streams": [
        {
            "id": "InputToSim",
            "type": "EventStream<InputEvent>",
            "from": "MainPU",
            "to": "SimPU",
            "multi_writer": false
        }
    ],
    "processing_units": [
        {
            "instance_id": "MainPU",
            "frequency_hz": 30,
            "dedicated_thread": false,
            "modules": [
                {
                    "instance_id": "Kernel",
                    "type_id": "KernelModule",
                    "stages": ["all"],
                    "dependencies": ["Logger"],
                    "reads": [],
                    "writes": ["InputToSim"],
                    "start_timeout_ms": 10000,
                    "stop_timeout_ms": 5000,
                    "config": {}
                }
            ]
        }
    ]
}
```

## In-Memory Representation

```cpp
namespace Dia::ApplicationFlow {

    struct StreamDeclaration {
        StringCRC id;
        StringCRC type;
        StringCRC fromPU;
        StringCRC toPU;
        bool multiWriter = false;
    };

    struct ModuleDeclaration {
        StringCRC instanceId;
        StringCRC typeId;
        DynamicArrayC<StringCRC, 16> stages;     // or contains "all" sentinel
        DynamicArrayC<StringCRC, 8> dependencies;
        DynamicArrayC<StringCRC, 4> reads;
        DynamicArrayC<StringCRC, 4> writes;
        float startTimeoutMs = 10000.0f;
        float stopTimeoutMs = 5000.0f;
        // config: opaque JSON blob passed to module on creation
    };

    struct ProcessingUnitDeclaration {
        StringCRC instanceId;
        float frequencyHz;
        bool dedicatedThread;
        DynamicArrayC<ModuleDeclaration, 32> modules;
    };

    struct ApplicationManifest {
        int version = 2;
        DynamicArrayC<StringCRC, 16> stages;
        StringCRC initialStage;
        DynamicArrayC<StringCRC, 16> autoStages;
        DynamicArrayC<StreamDeclaration, 16> streams;
        DynamicArrayC<ProcessingUnitDeclaration, 4> processingUnits;
    };
}
```

## Manifest Merging

Stage-specific manifests (referenced from `.diastage` files) are merged into the base manifest:

1. `.diagame` lists imports: one `manifest` type (base) + N `stage` types
2. Base manifest loaded first — provides PUs, streams, infrastructure modules
3. Each `.diastage` references a stage `.diaapp` — loaded and merged:
   - Stage's `processing_units` entries match by `instance_id` to base PUs
   - Stage's modules are appended to the matching PU's module list
   - Duplicate instance_ids within a PU = validation error
4. Final merged manifest is what Application receives

## Files Touched

| File | Action |
|------|--------|
| `Dia/DiaApplicationFlow/Manifest/ApplicationManifest.h` | Rewrite — v2 struct |
| `Dia/DiaApplicationFlow/Manifest/ApplicationManifest.cpp` | Rewrite |
| `Dia/DiaApplicationFlow/Manifest/ApplicationManifestLoader.h` | Rewrite — v2 JSON deserialization |
| `Dia/DiaApplicationFlow/Manifest/ApplicationManifestLoader.cpp` | Rewrite |
| `Dia/DiaApplicationFlow/Manifest/ManifestComposer.h` | Rewrite — stage-merge logic |
| `Dia/DiaApplicationFlow/Manifest/ManifestComposer.cpp` | Rewrite |
| `Dia/DiaApplicationFlow/Manifest/JsonApplicationManifestSerializer.h` | Delete (v1) |
| `Dia/DiaApplicationFlow/Manifest/JsonApplicationManifestSerializer.cpp` | Delete (v1) |
| `Dia/DiaApplicationFlow/Manifest/TypedImport.h` | Delete (v1 — import logic in ManifestComposer now) |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj` | Update |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj.filters` | Update |

## Dependencies

- **DiaSerializer** — ISerializer for JSON I/O
- **DiaCore/CRC/StringCRC** — all IDs
- **DiaCore/Containers** — DynamicArrayC for declaration arrays
- **DiaCore/Json** — jsoncpp for JSON parsing (internal to loader)

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all IDs | Compliant — all IDs stored as StringCRC in manifest struct |
| PD-004 | Platform | No STL in public APIs | Compliant — DynamicArrayC throughout |
| PD-007 | Platform | C++20 | Compliant |
| PD-010 | Platform | .diagame root, .diastage declares stage metadata | Compliant — manifest loading starts from .diagame, .diastage provides name+manifest path |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | Compliant — Dia::ApplicationFlow:: |
| SD-001 | DiaAppFlow | Config is sole source of truth | Compliant — this feature IS the config format |
| SD-006 | DiaAppFlow | Streams declared in config | Compliant — StreamDeclaration in manifest |
| SD-010 | DiaAppFlow | PU startup order = array order | Compliant — processingUnits array order preserved |
| SD-014 | DiaAppFlow | Full validation at load | Compliant — manifest loaded before validation feature runs (Validation feature consumes this struct) |
| SD-017 | DiaAppFlow | Clean break | Compliant — v1 serializer deleted |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Schema | Should `"config"` field be typed or opaque JSON? | Opaque JSON blob. Framework passes it to module. Module interprets it in DoStart. No schema enforcement at framework level. |
| 2 | Merging | What happens if a stage .diaapp references a PU that doesn't exist in base? | Validation error. Stage manifests can only add modules to existing PUs. PU creation is base-manifest-only. |
| 3 | Versioning | Should the loader support v1 manifests with migration? | No. Clean break (SD-017). v1 manifests must be rewritten. Loader rejects version != 2. |
| 4 | Defaults | Should optional fields (timeout, reads, writes) be omitted in JSON or explicitly null? | Omitted. Loader uses defaults when field is absent. Keeps manifests concise. |
| 5 | Streams | Should `multi_writer` default to false or require explicit declaration? | Default false. Only declared when needed. Fewer fields = simpler manifests. |
| 6 | Arrays | Are DynamicArrayC capacity limits (16 stages, 32 modules per PU) sufficient? | Yes for foreseeable use. If exceeded, compile-time increase. No dynamic allocation needed — games rarely have 32+ modules per PU. |

## Open Questions

None.

## Status

`Approved` — 2026-05-09
