---
module: dia.diamailboxvisualdebugger.v1
id: DiaMailboxVisualDebugger
layer: tools
parent: DiaMailbox
description: IDebugDomain panel visualization for DiaMailbox — per-type queue fill, send/drop/drain counters, drop alert section.
status: active
dependent_modules:
  - DiaMailbox
  - DiaVisualDebugger
  - DiaCore
public_headers:
  - MailboxVisualDebugger.h
namespaces:
  - Dia::Mailbox
entry_points:
  - MailboxVisualDebugger
non_responsibilities:
  - Message routing
  - Type registration
  - Any runtime behaviour in Release
compile_guard: DIA_DEBUG
---
