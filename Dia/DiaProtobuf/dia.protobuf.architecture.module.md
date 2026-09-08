---
schema: dia.module.v1
module_id: dia.protobuf
name: DiaProtobuf
layer: foundation/core
path: Dia/DiaProtobuf
status: active
maturity: dev
parent_module_id: dia.root

summary: >
  Thin wrapper around the protobuf runtime used for the debug protocol binary
  message format. Keeps the protobuf dependency isolated to one module.

responsibilities:
  - Protobuf runtime initialisation / shutdown
  - Generated .pb.cc / .pb.h compilation unit

non_responsibilities:
  - Protocol message definitions (DiaDebugProtocol)
  - Transport (DiaWebSocket / DiaDebugServer)

dependencies:
  required:
    - dia.json
  forbidden: []
---
