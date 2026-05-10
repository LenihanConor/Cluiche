---
schema: dia.module.v1
module_id: dia.application
name: Application
owner_team: TBD
layer: platform
status: active
maturity: dev

path: Dia/DiaApplicationFlow
language: cpp
parent_module_id: dia.root

summary: >
  DiaApplicationFlow — config-driven application framework (v2). Defines Application,
  Module, ProcessingUnit, TypeRegistry, ModuleRef, stream handles (FrameStream / EventStream),
  and IApplicationInspectable. Replaces the v1 Phase-based system; v1 files (ApplicationModule,
  ApplicationPhase, MessageBus, HotReloadManager) are present but superseded.

intent: >
  Provide a config-driven, stage-based application lifecycle framework where a .diaapp JSON
  manifest is the sole source of truth for structural wiring: which modules exist, which stages
  they belong to, and how processing units and streams connect them.

responsibilities:
  - Module base class (DoStart/DoUpdate/DoStop lifecycle, async kLoading startup, timeout handling)
  - ProcessingUnit — owns modules, drives their FrameTick loop, supports dedicated thread
  - Application — manifest validation, PU/module creation, stage transition algorithm, error policy
  - TypeRegistry + DIA_MODULE macro — static-init factory registration
  - ModuleRef<T> — lazy, lifecycle-safe inter-module access within a PU
  - FrameStreamStore / EventStreamStore — framework-owned inter-PU data channels
  - StreamWriter/Reader, EventStreamWriter/Reader — typed module-side handles
  - ApplicationManifestV2 POD structs — in-memory representation of .diaapp + .diastage files
  - ApplicationManifestLoaderV2 — JSON → ApplicationManifestV2
  - ManifestComposerV2 — .diagame → merged manifest (base + stage overlays)
  - ManifestValidatorV2 — full structural + dependency + cycle validation
  - IApplicationInspectable — read-only runtime introspection for debug tools and tests

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
    - Dia/DiaApplicationFlow/ModuleRefV2.h
    - Dia/DiaApplicationFlow/RegistrationMacrosV2.h
    - Dia/DiaApplicationFlow/IApplicationInspectable.h
    - Dia/DiaApplicationFlow/Manifest/ApplicationManifestV2.h
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
  namespaces:
    - Dia::ApplicationFlow
  entry_points:
    - Application
    - Module
    - ProcessingUnit
    - TypeRegistry
    - ModuleRef<T>
    - DIA_MODULE
    - IApplicationInspectable
    - ApplicationManifestV2
    - ApplicationManifestLoaderV2
    - ManifestComposerV2
    - ManifestValidatorV2
    - StreamWriter<T>
    - StreamReader<T>
    - EventStreamWriter<T>
    - EventStreamReader<T>

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
