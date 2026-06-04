---
schema: dia.module.v1
module_id: dia.api
name: DiaAPI
layer: foundation/services
path: Dia/DiaAPI
status: active
maturity: dev
parent_module_id: dia.root

summary: >
  Lightweight command-dispatch API layer. Modules register named JSON commands;
  external tools (editor, automation, debug clients) invoke them over the debug channel.

responsibilities:
  - Named command registration and dispatch
  - JSON request/response serialisation
  - Command registry (StringCRC → handler)

non_responsibilities:
  - Transport (owned by DiaDebugServer / DiaWebSocket)
  - Game logic

dependencies:
  required:
    - dia.core
    - dia.json
    - dia.observation
    - dia.python
  forbidden: []
---
