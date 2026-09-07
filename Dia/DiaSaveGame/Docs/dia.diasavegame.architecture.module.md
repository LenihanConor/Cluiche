---
schema: dia.module.v1
module_id: dia.savegame
name: DiaSaveGame
owner_team: TBD
layer: foundation/services
status: active
maturity: dev

path: Dia/DiaSaveGame
language: cpp
parent_module_id: dia.root

summary: >
  Generic save/load system for the Dia engine. Coordinates serialization across registered
  ISaveable participants, manages versioned save slots on disk, and orchestrates I/O via
  SaveManager. Async I/O: in-memory serialize on sim thread, disk flush off-thread.

intent: >
  Provides a participant-registration model where any game system can implement ISaveable
  and register itself with SaveRegistry. SaveManager owns all path construction, manifest
  versioning, participant ordering, migration callbacks, and Observer events. Applications
  call Save(slotId)/Load(slotId) only — DiaSaveGame handles everything else.

responsibilities:
  - ISaveable — contract (Serialize/Deserialize/GetVersion) any participant implements
  - SaveRegistry — ordered participant registration and migration callback registration
  - SaveContext / LoadContext — typed read/write handles backed by DiaSerializer primitives
  - SaveManifest — versioned header with engine version, participant list+versions, format tag
  - SaveManager — Save()/Load() orchestration; slot management (enumerate, delete, rename, cap)
  - Versioning + migration — per-participant forward migration on load version mismatch
  - Async I/O — in-memory serialize on sim thread, off-thread disk flush/read
  - Observer events — OnSaveStarted/Completed, OnLoadStarted/Completed, OnMigrationApplied
  - Test utilities under DiaSaveGame/Testing/ — InMemorySaveBackend, MockSaveable, AssertSlotExists, AssertEventFired

non_responsibilities:
  - What each participant saves — content is the participant's concern
  - Triggering saves (autosave, save-on-quit) — application concern
  - Save UI (slot picker, progress overlay) — application concern
  - Compression or encryption — out of scope
  - Network / cloud sync — local disk only
  - DiaEntity, DiaEconomy, or any domain type — participants self-register; DiaSaveGame has no domain imports
  - IModule / PU wiring — application wires SaveRegistry init and Save()/Load() into its own SimPU module

dependent_modules:
  - dia.core
  - dia.serializer

public_api:
  headers:
    - Dia/DiaSaveGame/ISaveable.h
    - Dia/DiaSaveGame/SaveRegistry.h
    - Dia/DiaSaveGame/SaveConfig.h
    - Dia/DiaSaveGame/SaveContext.h
    - Dia/DiaSaveGame/LoadContext.h
    - Dia/DiaSaveGame/SaveManifest.h
    - Dia/DiaSaveGame/SaveManager.h
    - Dia/DiaSaveGame/Testing/SaveTestHelpers.h
  namespaces:
    - Dia::SaveGame
    - Dia::SaveGame::Testing

dependencies:
  required:
    - dia.core
    - dia.serializer
  optional: []
  forbidden:
    - dia.entity
    - dia.economy
    - dia.applicationflow
    - dia.blackboard
---
