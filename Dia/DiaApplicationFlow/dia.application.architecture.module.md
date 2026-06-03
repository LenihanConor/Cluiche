---
schema: dia.module.v1
module_id: dia.application
name: Application
owner_team: TBD
layer: foundation/application
status: active
maturity: dev

path: Dia/DiaApplicationFlow
language: cpp
parent_module_id: dia.root

summary: >
  DiaApplicationFlow — config-driven application framework (v3). Defines Application,
  Module, ProcessingUnit, TypeRegistry, ModuleRef, three stream primitives
  (ServiceStream / FrameStream / EventStream), IApplicationInspectable (read-only
  introspection) and IApplicationControl (narrow control interface exposed to modules).
  Replaces the v1 Phase-based system; v1 files are present but superseded.

intent: >
  Provide a config-driven, stage-based application lifecycle framework where a .diaapp JSON
  manifest is the sole source of truth for structural wiring: which modules exist, which stages
  they belong to, and how processing units and streams connect them.

responsibilities:
  - Module base class (DoStart/DoUpdate/DoStop lifecycle, async kLoading startup, timeout handling,
    atomic cross-thread state, narrow IApplicationControl handle via GetApplication())
  - ProcessingUnit — pure scheduler: owns modules, drives their FrameTick loop in manifest array
    order, supports dedicated thread; does NOT hold a back-pointer to Application
  - Application — manifest validation, PU/module creation, stage transition algorithm, error policy
    (boot failure → shutdown; non-boot failure → capped rollback retries, then shutdown)
  - TypeRegistry + DIA_MODULE macro — static-init factory registration
  - ModuleRef<T> — lazy, lifecycle-safe inter-module access within a PU
  - Three stream primitives — all manifest-authoritative, declared in .diaapp streams[]:
    - ServiceStream (once per scope): stable lifecycle handle (e.g. canvas, texture handler)
      shared from a provider PU to consumer PUs. Collected-then-committed model — all
      providers Register() during DoStart; framework Commits after all providers in scope
      have registered; Get() asserts before commit. Roles: provides / consumes.
    - FrameStream (every tick): composite per-PU-pair latest-wins data transport.
      One struct per direction (e.g. SimToRender, MainToRender). New features extend the
      struct, never add streams. Roles: reads / writes.
    - EventStream (frame-batched): discrete events flushed at producer tick boundary.
      Consumer gets exactly the previous frame's batch. Roles: reads / writes.
  - Unified channels[] array on ModuleDeclaration — each entry is {id, role} where role
    is one of reads / writes / provides / consumes.
  - ServiceStreamStore / ServiceStreamWriter / ServiceStreamReader — ServiceStream impl.
  - FrameStreamStore / EventStreamStore — FrameStream and EventStream impl; IStreamStore
    tap API: AttachTap(callback) → TapHandle, DetachTap(handle), GetTapCount()
  - StreamWriter/Reader, EventStreamWriter/Reader, ServiceStreamWriter/Reader — typed module handles
  - StreamTypeRegistry — process-static registry mapping C++ type → StringCRC type ID for stream payload type checking
  - ApplicationManifestV3 POD structs — in-memory representation of .diaapp v3 + .diastage files; stages are objects {name, transitions[], auto_advance} (SD-019)
  - ApplicationManifestLoaderV2 — JSON → ApplicationManifestV3 (v3 schema; rejects v2)
  - ManifestComposerV2 — .diagame → merged manifest (base + stage overlays)
  - ManifestValidatorV2 — full structural + dependency + cycle validation, including
    array-order enforcement for declared dependencies (DEPENDENCY_ORDER error)
  - IApplicationInspectable — read-only runtime introspection for debug tools and tests; FindStream(StringCRC) → IStreamStore*
  - IApplicationControl — narrow control interface (TransitionTo, RequestShutdown,
    GetCurrentStage, RegisterTransitionGuard, UnregisterTransitionGuards) exposed to
    runtime module code via Module::GetApplication()

non_responsibilities:
  - Game-specific module implementations (belong in game application)
  - Rendering or window creation (BootstrapResources created externally, passed to Application)
  - Asset loading (DiaAssetRuntime)
  - Networking / WebSocket (DiaDebugServer)
  - High-level editor UI (DiaApplicationFlowEditor)

public_api:
  headers:
    - Dia/DiaApplicationFlow/Application.h
    - Dia/DiaApplicationFlow/Module.h
    - Dia/DiaApplicationFlow/ProcessingUnit.h
    - Dia/DiaApplicationFlow/TypeRegistry.h
    - Dia/DiaApplicationFlow/StreamTypeRegistry.h
    - Dia/DiaApplicationFlow/ModuleRefV2.h
    - Dia/DiaApplicationFlow/RegistrationMacrosV2.h
    - Dia/DiaApplicationFlow/IApplicationInspectable.h
    - Dia/DiaApplicationFlow/IApplicationControl.h
    - Dia/DiaApplicationFlow/Manifest/ApplicationManifestV3.h
    - Dia/DiaApplicationFlow/Manifest/ApplicationManifestLoaderV2.h
    - Dia/DiaApplicationFlow/Manifest/ManifestComposerV2.h
    - Dia/DiaApplicationFlow/Manifest/ManifestValidatorV2.h
    - Dia/DiaApplicationFlow/Streams/IStreamStore.h
    - Dia/DiaApplicationFlow/Streams/FrameStreamStore.h
    - Dia/DiaApplicationFlow/Streams/EventStreamStore.h
    - Dia/DiaApplicationFlow/Streams/StreamWriter.h
    - Dia/DiaApplicationFlow/Streams/StreamReader.h
    - Dia/DiaApplicationFlow/Streams/EventStreamWriter.h
    - Dia/DiaApplicationFlow/Streams/EventStreamReader.h
    - Dia/DiaApplicationFlow/Streams/ServiceStreamWriter.h
    - Dia/DiaApplicationFlow/Streams/ServiceStreamReader.h
    - Dia/DiaApplicationFlow/Streams/ServiceStreamStore.h
  namespaces:
    - Dia::ApplicationFlow
  entry_points:
    - Application
    - Module
    - ProcessingUnit
    - TypeRegistry
    - StreamTypeRegistry
    - ModuleRef<T>
    - DIA_MODULE
    - DIA_STREAM_TYPE(T)
    - IApplicationInspectable
    - IApplicationControl
    - ApplicationManifestV3
    - ApplicationManifestLoaderV2 (loads v3 schema)
    - ManifestComposerV2
    - ManifestValidatorV2
    - StreamWriter<T>
    - StreamReader<T>
    - EventStreamWriter<T>
    - EventStreamReader<T>
    - ServiceStreamWriter<T>
    - ServiceStreamReader<T>
    - IStreamStore (AttachTap / DetachTap / GetTapCount)
    - TapHandle

dependencies:
  required:
    - dia.core.containers.arrays
    - dia.core.containers.hashtables
    - dia.core.core
    - dia.core.crc
    - dia.core.strings
    - dia.core.time
    - dia.core.memory
    - dia.logger
    - dia.serializer
  forbidden: []
---
