---
schema: dia.module.v1
module_id: dia.schemabrowser
name: DiaSchemaBrowser
layer: tools/inspector
path: Dia/DiaSchemaBrowser
status: active
maturity: dev
parent_module_id: dia.editor

summary: >
  Read-only, offline editor plugin browsing *.diagamemessages declaration files.
  Discovery and JSON parsing happen in C++; all semantic interpretation (graph
  building, duplicate detection, orphan analysis) lives in the TS UI so parsing
  logic isn't duplicated across languages. Never opens a *.diagamemessages file
  for writing; zero dependency on DiaMessageBus/DiaDebugServer/live game state.

dependencies:
  required:
    - dia.core
    - dia.editor
    - dia.observation
  forbidden:
    - dia.messagebus
    - dia.debugserver
---
