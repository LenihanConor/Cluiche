---
schema: dia.module.v1
module_id: dia.mailboxvisualdebugger
name: DiaMailboxVisualDebugger
layer: domain/visual/tools
path: Dia/DiaMailboxVisualDebugger
status: active
maturity: dev
parent_module_id: dia.mailbox

summary: >
  IDebugDomain panel visualization for DiaMailbox — per-type queue fill,
  send/drop/drain counters, drop alert section.

dependencies:
  required:
    - dia.core
    - dia.mailbox
  forbidden: []

public_api:
  headers:
    - DiaMailboxVisualDebugger/MailboxVisualDebugger.h
  namespaces:
    - Dia::Mailbox
  entry_points:
    - MailboxVisualDebugger

non_responsibilities:
  - Message routing
  - Type registration
  - Any runtime behaviour in Release
---
