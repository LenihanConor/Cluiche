---
schema: dia.module.v1
module_id: dia.automation
name: DiaAutomation
owner_team: TBD
layer: foundation/application
status: active
maturity: dev

path: Dia/DiaAutomation
language: cpp
parent_module_id: dia.root

summary: >
  DiaAutomation — pure capability layer for making Dia applications externally driveable.
  Provides AutomationService: checkpoint registry, navigation hold (transition guard),
  pause/resume callbacks, CI safety heartbeat, and dia.automation.* DiaAPI commands.

intent: >
  Make Dia applications driveable by external automation (pytest orchestrator, debug console)
  via DiaDebugServer's WebSocket. AutomationService is not a Module — it is instantiated and
  wired by an application-level AutomationModule (game-side, in CluicheGameBaseline).

responsibilities:
  - Checkpoint registry — modules register named CheckpointFn; auto-clear on module stop
  - Navigation hold — registers a transition guard that blocks auto-advance; re-arms after each transition
  - Pause/resume — cooperative callback registry; invoked by dia.automation.pause/resume commands
  - CI safety — heartbeat monitor (configurable timeout) + OnDisconnect handler (release + shutdown)
  - Register dia.automation.navigate_to/pause/resume/validate commands via DiaAPI JSON path

non_responsibilities:
  - Transport (WebSocket) — DiaDebugServer concern
  - Application lifecycle — AutomationModule (game-side) wires this service
  - Orchestration logic — Python/pytest owns scenario flow
  - Stage-specific validation — game modules register checkpoints

public_api:
  headers:
    - Dia/DiaAutomation/AutomationService.h
  namespaces:
    - Dia::Automation
  entry_points:
    - AutomationService
    - CheckpointResult
    - CheckpointFn
    - PauseResumeFn

dependencies:
  required:
    - dia.core.containers.arrays
    - dia.core.crc
    - dia.core.core
    - dia.application (DiaApplicationFlow)
    - dia.api
    - dia.observation
  forbidden: []
---
