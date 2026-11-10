---
schema: dia.module.v1
module_id: dia.streams
name: DiaStreams
owner_team: TBD
layer: foundation/services
status: active
maturity: dev

path: Dia/DiaStreams
language: cpp

summary: >
  Stream types for cross-PU communication: FrameStream, EventStream, ServiceStream.
  Extracted from DiaApplicationFlow so modules that only need stream handles do not
  pull in the full application lifecycle framework.

intent: >
  Provide stream read/write handles and the type registry without requiring
  the PU/Module/Phase application framework.

responsibilities:
  - Stream store interfaces and implementations (IStreamStore, EventStreamStore, FrameStreamStore, ServiceStreamStore)
  - Stream reader/writer handles (StreamReader, StreamWriter, EventStreamReader/Writer, ServiceStreamReader/Writer)
  - Stream type serialization registry (StreamTypeRegistry)
  - Stream diagnostics (FrameStreamDiagnostics)
  - Honour manifest-declared EventStream settings: EventStreamReader/Writer::Connect() asks the IStreamConnector (GetDeclaredEventStreamConfig) for the declared per-reader ring capacity (nonzero overrides the default 256) and overflow policy; undeclared streams keep defaults. The first Connect() for a stream creates the store.

non_responsibilities:
  - Application lifecycle (DiaApplicationFlow)
  - Stream creation/ownership — streams are created by DiaApplicationFlow

dependencies:
  required:
    - dia.core
    - dia.debugserver
---
