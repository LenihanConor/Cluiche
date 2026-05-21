---
module_id: dia.mailbox
display_name: DiaMailbox
version: 1.0.0
parent_module: dia
type: static_library
namespace: Dia::Mailbox
include_root: Dia/DiaMailbox
public_headers:
  - Mailbox.h
  - MailboxTypes.h
  - IMailboxRouter.h
dependencies:
  required:
    - dia.core
responsibilities:
  - Typed deferred message queuing (ring buffers with compile-time capacity)
  - Opaque address routing via pluggable IMailboxRouter
  - Caller-managed subscriptions with generation-tracked handles
non_responsibilities:
  - Knowledge of entities, components, or any domain concept
  - Thread safety (single-threaded primitive)
  - Cross-frame message persistence
---

# DiaMailbox

Generic typed deferred messaging primitive for the Dia engine. Modules use it to publish typed messages addressed to opaque destinations; subscribers drain the relevant typed queues at a controlled point in their frame.

See `docs/specs/systems/dia/diamailbox.md` for the full system spec.
