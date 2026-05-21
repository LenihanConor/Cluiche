# Implementation Plan: DiaMailbox System

**Spec:** [diamailbox.md](diamailbox.md)
**Created:** 2026-05-21
**Updated:** 2026-05-21 (Tasks 1–2 Done)

---

## Session Notes

### Spec Decisions Summary

DiaMailbox is a generic typed deferred messaging primitive depending only on DiaCore. `Address = (StringCRC routerId, uint64_t payload)` — fully opaque to DiaMailbox (SD-MBX-001). Per-type ring buffers with compile-time capacity (`Register<T, kCap>()`, SD-MBX-002). Delivery is polled via `Drain<T>(visitor)` — not callback-on-send (SD-MBX-003). Caller manages subscription lifetime; Mailbox lifetime is upper bound (SD-MBX-004). Routers register by StringCRC ID, not type (SD-MBX-005). Default overflow = DropOldest + DIA_LOG_WARNING once per Drain; Assert opt-in (SD-MBX-006). Single-threaded, no locks (SD-MBX-007). Mailbox is non-copyable/non-movable (SD-MBX-008). Namespace `Dia::Mailbox::` (SD-MBX-009). `Resolve` called by domain consumers, never internally during `Send` (SD-MBX-010). Type key = compile-time `StringCRC(__FUNCSIG__)`. `SubscriptionHandle = Handle<Subscription>` backed by `HandlePool<Subscription, 256>`. `SubscriptionHandle::IsValid()` is UB after Mailbox destruction (documented precondition, no runtime safety net). Test utilities in `Dia/DiaMailbox/Testing/` per platform pattern.

### Key Implementation Notes

- `module-and-build` creates the **empty** vcxproj skeleton first; each subsequent feature adds its own files.
- Type-key derivation must be identical across `typed-queue` and `subscriptions` — share the same `typeKey<T>()` helper.
- Drop warning: accumulate `dropsThisDrain` counter on `Send`; emit once on `Drain`. Never per-drop.
- Drain snapshot-tail pattern: snapshot tail before visitor loop to defer Send-during-Drain messages to next pass.
- `SubscriberSet` capacity 64 is a placeholder — will be revisited when DiaEntity sizes its component count.
- Library solution folder GUID: `{2291F464-9B87-42F1-AA90-34FE22DD5F9B}`. DiaCore project GUID: `{8D41CBE3-C493-428D-95F3-627E56500667}`.

---

## Task Table

| # | Feature | Spec | Status | Model | Notes |
|---|---------|------|--------|-------|-------|
| 1 | module-and-build | [module-and-build.md](../../features/dia/diamailbox/module-and-build.md) | Done | haiku | Empty vcxproj skeleton + sln registration + YAML doc. Must be first. |
| 2 | address-and-types | [address-and-types.md](../../features/dia/diamailbox/address-and-types.md) | Done | haiku | `MailboxTypes.h` + `MailboxTypes.cpp` + tests. 9/9 GREEN. Note: `Address` is not trivially copyable — `StringCRC` has user-defined copy ctor; AC9 corrected in spec. |
| 3 | typed-queue | [typed-queue.md](../../features/dia/diamailbox/typed-queue.md) | Planned | sonnet | `Mailbox.h/.cpp` ring buffer + overflow + tests. |
| 4 | subscriptions | [subscriptions.md](../../features/dia/diamailbox/subscriptions.md) | Planned | sonnet | `Subscription.h/.cpp` + HandlePool wiring + tests. |
| 5 | routers | [routers.md](../../features/dia/diamailbox/routers.md) | Planned | sonnet | `IMailboxRouter.h` + `Resolve<T>` + `MockRouter` + `MailboxFixture` + tests. |
