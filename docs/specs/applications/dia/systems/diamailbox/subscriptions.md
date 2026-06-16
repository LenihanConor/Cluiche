# Feature Spec: DiaMailbox — Subscriptions

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia/dia.md | - |
| System | @docs/specs/applications/dia/systems/diamailbox/diamailbox.md | **subscriptions** |

**Status:** `Approved` — 2026-05-21. Steps 3 (Binding Decisions) and 4 (AI Review Questions) complete and confirmed.

**Depends on:** `address-and-types` (provides `SubscriberId`, `SubscriptionHandle` forward declaration, `SubscriberSet` type alias) and `typed-queue` (establishes type-key derivation pattern reused for per-type subscriber lists). Both must be Done before implementation begins.

---

## Problem Statement

Without subscriptions, there is no way to track which consumers care about which message types. `Drain` can iterate queued messages and `Send` can stamp an opaque address on each one, but there is no registry that maps a type to the set of `SubscriberId` values that declared interest. The router's `Resolve` call (delivered in the `routers` feature) reads exactly this registry when it builds the matched subscriber set for a given address.

`Subscribe<T>` and `Unsubscribe` must satisfy three constraints simultaneously. First, subscription lifetime is caller-managed (SD-MBX-004): the mailbox does not know when a subscriber's scope ends; the caller drives `Unsubscribe` explicitly. Second, handle safety: a double-`Unsubscribe` must be a no-op and a handle from a destroyed mailbox must not dereference stale memory. Third, per-type isolation: subscribing to type `A` has no effect on the subscriber set for type `B`.

The generation-tracked `HandlePool<Subscription, kMaxSubs>` owned by `Mailbox` satisfies all three constraints cleanly. It is also the first concrete reuse of `HandlePool` in DiaMailbox, proving the layering between `DiaMailbox` and `DiaCore`.

## Solution Overview

`Mailbox` owns a `HandlePool<Subscription, kMaxSubs>` where `kMaxSubs = 256` (placeholder; revisited when diaentitytemplate sizes its component count). Each slot stores a `Subscription` struct carrying the `SubscriberId` and the `typeKey` (the same compile-time CRC used by `typed-queue` to identify a registered type).

`Subscribe<T>` acquires a free slot from the pool, writes `{subscriberId, typeKey<T>()}` into it, and returns a `SubscriptionHandle` wrapping the generation-tracked `Handle<Subscription>`. The per-type subscriber list is updated immediately so that `GetSubscribersForType<T>` returns the new subscriber.

`Unsubscribe(handle)` validates the handle's generation against the pool. A stale generation (already unsubscribed, or mailbox rebuilt) resolves to a no-op. A live handle removes the subscriber from the per-type list, then releases the pool slot, advancing the generation. Because the pool is the single source of truth for slot validity, there is no separate "is-alive" flag to keep in sync.

`Mailbox::~Mailbox` releases the entire pool. Any `SubscriptionHandle` instances still held by callers become stale (generation mismatch) — `Unsubscribe` on such handles is a no-op. Callers must not assume handles survive mailbox destruction; this constraint is documented on `SubscriptionHandle::IsValid()`.

`GetSubscribersForType<T>` returns a `const SubscriberSet&` to the live per-type subscriber list. This is the read path that `Resolve` (routers feature) uses at drain time to populate the matched set.

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `Subscribe<T>(id)` returns a `SubscriptionHandle` where `handle.IsValid()` returns `true` | Unit test |
| AC2 | After `Subscribe<T>(id)`, `GetSubscribersForType<T>()` contains exactly `id` | Unit test |
| AC3 | Two successive `Subscribe<T>` calls with different `SubscriberId` values both appear in `GetSubscribersForType<T>()` | Unit test |
| AC4 | `Subscribe<A>` does not affect `GetSubscribersForType<B>()` for any type `B != A` (per-type isolation) | Unit test: subscribe to `MsgA`, assert `GetSubscribersForType<MsgB>()` is empty |
| AC5 | `Unsubscribe(handle)` causes `handle.IsValid()` to return `false` | Unit test |
| AC6 | After `Unsubscribe(handle)`, the corresponding `SubscriberId` is absent from `GetSubscribersForType<T>()` | Unit test |
| AC7 | `Unsubscribe` called a second time with the same (now-stale) handle is a no-op — no crash, no assertion, no change to the subscriber set | Unit test: unsubscribe twice; assert no crash and subscriber set unchanged |
| AC8 | Callers must not use `SubscriptionHandle::IsValid()` after the issuing `Mailbox` is destroyed — this is undefined behaviour; the contract is documented on `SubscriptionHandle` | Code review: documented precondition in header comment |
| AC9 | `Subscribe<T>` when the pool is at `kMaxSubs` capacity returns an invalid handle (`IsValid() == false`) and emits `DIA_LOG_WARNING` | Unit test: exhaust pool, assert returned handle is invalid |
| AC10 | `Subscribe<T>` for an unregistered type `T` (i.e. `RegisterType<T>` was never called) returns an invalid handle and emits `DIA_LOG_WARNING` | Unit test |
| AC11 | `GetSubscribersForType<T>()` for an unregistered type `T` returns an empty `SubscriberSet` (does not crash) | Unit test |
| AC12 | Subscribing the same `SubscriberId` twice to the same type produces two independent handles; both appear in `GetSubscribersForType<T>()`; unsubscribing one removes exactly one occurrence | Unit test: subscribe id twice, unsubscribe first handle, assert id still in subscriber set once |
| AC13 | `Subscribe<T>` when the per-type subscriber list is at capacity (64) returns an invalid handle and emits `DIA_LOG_WARNING`, even if the pool (`kMaxSubs`) has free slots | Unit test: subscribe 64 distinct IDs to the same type, attempt 65th, assert invalid handle + warning |

## Public API

```cpp
// Dia/DiaMailbox/Subscription.h
#pragma once
#include <DiaMailbox/MailboxTypes.h>
#include <DiaCore/Architecture/Handle/Handle.h>

namespace Dia::Mailbox {

    // Internal slot type owned by HandlePool<Subscription, kMaxSubs>.
    // Exposed in this header for HandlePool instantiation; callers do not
    // construct Subscription directly.
    struct Subscription {
        SubscriberId subscriberId;
        uint32_t     typeKey;   // compile-time CRC identifying the message type
    };

    // Maximum number of live subscriptions per Mailbox instance.
    // Placeholder — revisit when diaentitytemplate sizes its component count.
    constexpr uint32_t kMaxSubs = 256;

    // Caller-held subscription lifetime token.
    // Obtained from Mailbox::Subscribe<T>(); released via Mailbox::Unsubscribe().
    // IsValid() returns false after Unsubscribe, after Mailbox destruction, or if
    // Subscribe failed (pool full or type unregistered).
    class SubscriptionHandle {
    public:
        SubscriptionHandle() = default;

        // Returns true iff the subscription is still live in its issuing Mailbox.
        // Validity is determined by the handle's generation against the pool.
        // PRECONDITION: The issuing Mailbox must still be alive. Calling IsValid()
        // after the Mailbox is destroyed is undefined behaviour.
        bool IsValid() const;

    private:
        friend class Mailbox;
        explicit SubscriptionHandle(Dia::Core::Handle<Subscription> handle);

        Dia::Core::Handle<Subscription> mHandle;  // generation-tracked
    };

} // namespace Dia::Mailbox


// Amendments to Dia/DiaMailbox/Mailbox.h
// (the following methods are added to the existing Mailbox class from typed-queue)
namespace Dia::Mailbox {

    class Mailbox {
    public:
        // ... existing RegisterType / Send / Drain from typed-queue feature ...

        // Register caller as a subscriber to messages of type T.
        // Returns an invalid handle if:
        //   - the pool is at kMaxSubs capacity (DIA_LOG_WARNING emitted), or
        //   - type T has not been registered via RegisterType<T> (DIA_LOG_WARNING emitted).
        // Caller is responsible for calling Unsubscribe before the SubscriberId
        // becomes invalid. Mailbox destruction invalidates all outstanding handles.
        template <class T>
        SubscriptionHandle Subscribe(SubscriberId subscriber);

        // Remove the subscription associated with handle.
        // Idempotent: if handle is already invalid (stale generation, double-unsubscribe,
        // or Mailbox destroyed), this is a no-op. No crash, no assertion.
        void Unsubscribe(SubscriptionHandle handle);

        // Returns the live subscriber set for type T.
        // Used by Resolve (routers feature) at drain time to build matched sets.
        // If type T is unregistered, returns a reference to a static empty SubscriberSet.
        template <class T>
        const SubscriberSet& GetSubscribersForType() const;

    private:
        Dia::Core::HandlePool<Subscription, kMaxSubs> mSubscriptionPool;
        // Per-type subscriber lists — keyed by typeKey (same CRC as typed-queue uses).
        // DynamicArrayC of SubscriberId per registered type; stored alongside ring buffer
        // metadata in the per-type type registry.
    };

} // namespace Dia::Mailbox
```

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Write `Dia/DiaMailbox/Subscription.h` — `Subscription` struct, `kMaxSubs` constant, `SubscriptionHandle` class (full definition, replaces forward declaration from `address-and-types`) | AC1, AC5, AC8 | Planned | haiku | Plain header; mHandle is `Handle<Subscription>` |
| 2 | Write `Dia/DiaMailbox/Subscription.cpp` — `SubscriptionHandle` constructor and `IsValid()` implementation | AC1, AC5, AC7, AC8 | Planned | haiku | Delegates to `mHandle.IsValid()` against pool |
| 3 | Extend `Mailbox` per-type registry to store a `DynamicArrayC<SubscriberId, 64>` subscriber list alongside each ring buffer | AC2, AC3, AC4 | Planned | sonnet | Amends typed-queue's type registry structure |
| 4 | Implement `Mailbox::Subscribe<T>` — type-key lookup, pool acquisition, per-type list update, handle construction; overflow and unregistered-type guard paths | AC1, AC2, AC3, AC9, AC10 | Planned | sonnet | |
| 5 | Implement `Mailbox::Unsubscribe` — generation validation via pool, per-type list removal, slot release; idempotent on stale handle | AC5, AC6, AC7 | Planned | sonnet | Generation check is the no-op gate |
| 6 | Implement `Mailbox::GetSubscribersForType<T>` — type-key lookup; return static empty set for unregistered types | AC4, AC11 | Planned | haiku | Read path used by routers feature |
| 7 | Ensure `Mailbox::~Mailbox` releases the pool, rendering all outstanding handles stale | AC8 | Planned | haiku | Pool destructor handles slot state; verify generation advances on release |
| 8 | Write GoogleTests for AC1–AC12 in `GoogleTests/DiaMailbox/SubscriptionTests.cpp` | All ACs | Planned | sonnet | |
| 9 | Update `DiaMailbox.vcxproj` + `.vcxproj.filters` to include `Subscription.h` and `Subscription.cpp` | — | Planned | haiku | vcxproj created by `module-and-build` which runs first |

## Binding Decisions Compliance

| ID | Decision (summary) | Compliance |
|----|--------------------|------------|
| PD-001 | StringCRC for all entity/component IDs | `typeKey` in `Subscription` is the same compile-time CRC used by `typed-queue` to key the type registry — consistent with `StringCRC` conventions. `SubscriberId::value` is opaque; DiaMailbox does not interpret it. |
| PD-004 | No STL containers in public APIs | `SubscriberSet` is `DynamicArrayC<SubscriberId, 64>`. `HandlePool<Subscription, kMaxSubs>` is a DiaCore fixed-capacity container. No STL type in any public signature. |
| PD-005 | x64 only | `Handle<Subscription>` stores a 32-bit index + 32-bit generation; fits naturally in x64 register. No 32-bit size assumptions. |
| PD-006 | Visual Studio project files are source of truth | `DiaMailbox.vcxproj` and `.vcxproj.filters` updated manually in Task 9 (or deferred to `module-and-build`). |
| PD-007 | C++20 required | Template methods `Subscribe<T>` and `GetSubscribersForType<T>` compiled under `/std:c++20`. No C++20 features strictly required; the feature must not block compilation under that standard. |
| PD-008 | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaMailbox.vcxproj` must not override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard`. |
| AD-001 | Module system with YAML frontmatter | `dia.mailbox.architecture.module.md` updated in `module-and-build` feature to describe the subscription subsystem as part of the public API. |
| AD-002 | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Namespace `Dia::<Module>::` | All types and methods in `Dia::Mailbox::` per SD-MBX-009. |
| SD-MBX-001 | Address is `(StringCRC routerId, uint64_t payload)` — opaque to DiaMailbox | Not directly applicable to this feature; subscriptions record `SubscriberId` and `typeKey`, neither of which is an `Address`. |
| SD-MBX-002 | Per-type ring buffers with compile-time capacity | Subscriber lists are per-type, stored alongside each ring buffer in the type registry. `SubscriberSet` is `DynamicArrayC<SubscriberId, 64>` — compile-time capacity. |
| SD-MBX-003 | Delivery is polled (`Drain`), not callback-on-send | Subscriptions are a registry — no delivery callbacks introduced here. `GetSubscribersForType` is a pure read; invocation timing is the routers feature's concern. |
| SD-MBX-004 | Subscription lifetime: caller manages, mailbox lifetime is upper bound | `Subscribe` returns a handle; caller must call `Unsubscribe`. Mailbox destruction invalidates all handles via generation advancement. No automatic RAII coupling to caller scope. |
| SD-MBX-005 | Routers register against `StringCRC` ID | Not applicable to this feature; routing registration is in the `routers` feature. |
| SD-MBX-006 | Default overflow is `DropOldest` with `DIA_LOG_WARNING`; `Assert` is opt-in | Not directly applicable; overflow policy governs ring buffers, not subscription pool. Pool overflow (AC9) emits `DIA_LOG_WARNING` and returns an invalid handle — consistent spirit. |
| SD-MBX-007 | Single-threaded | No locks in `Subscribe`, `Unsubscribe`, or `GetSubscribersForType`. Concurrent access is undefined; callers must not call from multiple threads. |
| SD-MBX-008 | Mailbox is non-copyable, non-movable | `HandlePool` is non-copyable; `Mailbox`'s deleted copy/move constructors (established in `typed-queue`) remain in force. `SubscriptionHandle` is copyable (it is a value type wrapping a handle), but the underlying pool slot is not duplicated — each copy of the handle refers to the same subscription. |
| SD-MBX-009 | Namespace is `Dia::Mailbox::` | `Subscription`, `kMaxSubs`, `SubscriptionHandle`, and all new methods are in `Dia::Mailbox::`. |
| SD-MBX-010 | `Resolve` called by domain consumers, not internally during `Send` | `GetSubscribersForType<T>()` is the read path; it is not invoked by `Subscribe`, `Unsubscribe`, or `Send`. Calling convention is the routers feature's responsibility. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Handle safety after Mailbox destruction | A caller holds a `SubscriptionHandle` and calls `IsValid()` after the issuing `Mailbox` is destroyed. Is this safe? | No — it is undefined behaviour. `IsValid()` delegates to the pool via a pointer; after Mailbox destruction the pool is gone. The handle stores (index, generation) as pure values, but has no mechanism to detect that the pool no longer exists. Resolution: **document that `IsValid()` must only be called while the issuing Mailbox is alive**. This matches SD-MBX-004 (caller manages lifetime) and is consistent with how `Handle<T>` works elsewhere — you don't query a handle whose owning pool is destroyed. The caller relies on scoping: the mailbox dies with its owning module; handles die with their owning subscribers. AC8 encodes this as a documented precondition, not a runtime safety net. |
| 2 | Double-subscribe same SubscriberId | AC12 specifies that subscribing the same `SubscriberId` twice produces two independent handles. Does this mean duplicates appear in `GetSubscribersForType<T>()`? | Yes — by design. DiaMailbox does not enforce uniqueness of `SubscriberId` in the subscriber list; that is a caller concern. A broadcast router would deliver to the same subscriber twice if it is registered twice. If the caller wants exactly-once delivery, it must ensure at most one subscription per ID. Documenting this as caller responsibility avoids a per-subscribe O(n) dedup scan, consistent with the fixed-capacity, no-allocation design. |
| 3 | typeKey derivation | `Subscription::typeKey` reuses the type-CRC pattern from `typed-queue`. Is the derivation guaranteed to be identical across translation units? | Yes — `typeKey` is derived via the same compile-time `StringCRC` of the type's name (or equivalent type-identity mechanism) used in `RegisterType<T>` and `Send<T>`. Both features must use the same constexpr expression for consistency. The `subscriptions` implementation must import or share the `typeKey<T>()` helper introduced in `typed-queue` rather than defining a second derivation. Any divergence is a linking bug detectable by AC4 (per-type isolation). |
| 4 | kMaxSubs overflow behaviour | AC9 requires a `DIA_LOG_WARNING` and an invalid handle when the pool is full. Should `Subscribe` also trigger `DIA_ASSERT` in Debug? | No — `Assert` overflow mode is reserved for ring buffers (SD-MBX-006). Subscription pool exhaustion is a configuration issue (kMaxSubs is too small), not necessarily a programming error. `DIA_LOG_WARNING` is the correct severity: it surfaces the problem without halting Debug sessions. If a game needs hard guarantees on subscription count, it can static_assert on `kMaxSubs` at compile time. |
| 5 | SubscriptionHandle copyability | `SubscriptionHandle` is copyable — two copies refer to the same pool slot. What happens if the caller copies a handle and unsubscribes via one copy? | The first `Unsubscribe` releases the slot and advances the generation. The second copy's handle now has a stale generation; `Unsubscribe` via the copy is a no-op (AC7 idempotency guarantee). This is identical semantics to copying a raw `Handle<T>` — expected and correct. Callers that copy handles must be aware that unsubscribing one copy invalidates all copies. Documentation on `SubscriptionHandle` should note this. |
| 6 | GetSubscribersForType and routers | `GetSubscribersForType<T>()` returns a `const SubscriberSet&`. The routers feature will read this reference during `Resolve`. Is the reference stable across `Subscribe` / `Unsubscribe` calls? | The per-type `SubscriberSet` is a `DynamicArrayC` stored by value in the Mailbox's type registry. Its address does not change after the registry entry is created — `RegisterType<T>` allocates the slot; subsequent `Subscribe`/`Unsubscribe` calls mutate its contents in place. The reference is therefore stable for the lifetime of the Mailbox. The routers feature is single-threaded (SD-MBX-007); no concurrent mutation risk. |
| 7 | Subscribe before RegisterType | AC10 requires that `Subscribe<T>` for an unregistered type returns an invalid handle. What is the design rationale for this rather than auto-registering? | Auto-registration would require choosing a capacity and overflow policy at subscribe time — both of which are deliberate `RegisterType<T, kCapacity>` arguments. DiaMailbox must not silently pick defaults for ring buffer configuration on behalf of the caller. Failing early at subscribe time with a warning is clearer: it surfaces the missing `RegisterType` call rather than masking it with a silent default. |
| 8 | SubscriberSet capacity vs. kMaxSubs | `SubscriberSet` is `DynamicArrayC<SubscriberId, 64>` but `kMaxSubs = 256`. Can a single type have more than 64 subscribers? | Yes — `kMaxSubs = 256` is the total pool capacity across all types. A single type's subscriber list is bounded by `SubscriberSet`'s capacity (64). If more than 64 subscribers subscribe to the same type, the 65th `Subscribe<T>` call must fail with an invalid handle and `DIA_LOG_WARNING`, similar to pool exhaustion. The per-type list and the pool are two independent capacity constraints; implementation must check both. |
| 9 | Interaction with Drain | Can a `Drain<T>` visitor call `Subscribe<T>` or `Unsubscribe` for the type being drained? | `Subscribe`/`Unsubscribe` mutate the subscriber list, not the ring buffer. `Drain` iterates the ring buffer only — it does not iterate the subscriber list. Mutating the subscriber list during `Drain<T>` is therefore safe from a data-structure standpoint (no iterator invalidation). However, any new subscription registered during the drain will not appear in the current drain pass's `GetSubscribersForType` snapshot if `Resolve` is being called. This is analogous to the send-during-drain behaviour documented in the system spec (AQ9): new arrivals apply next pass. Documenting this as defined but take-effect-next-pass behaviour is sufficient for v1. |
| 10 | Empty SubscriberSet reference for unregistered types | AC11 requires `GetSubscribersForType<T>()` to return an empty set rather than crashing for unregistered `T`. This means returning a reference to a static or member empty set. Which? | A `static const SubscriberSet kEmpty{};` at function scope is the cleanest approach. It is zero-initialised, has stable address, and avoids adding a dedicated `mEmptySet` member to `Mailbox`. The same pattern is used for similar "return empty on miss" accessors throughout the codebase. |

---

## Status

`Approved` — 2026-05-21. Steps 3 (Binding Decisions) and 4 (AI Review Questions) complete and confirmed.
