# Feature Spec: DiaMailbox — Routers

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia/dia.md | - |
| System | @docs/specs/applications/dia/systems/diamailbox/diamailbox.md | **routers** |

**Status:** `Approved` — 2026-05-21. Steps 3 (Binding Decisions) and 4 (AI Review Questions) complete and confirmed.

**Depends on:** `address-and-types` (provides `Address`, `SubscriberId`, `SubscriberSet` type alias), `typed-queue` (establishes type-key derivation and per-type ring buffer registry), `subscriptions` (provides `GetSubscribersForType<T>()` read path used by `Resolve`). All three must be Done before implementation begins.

---

## Problem Statement

Without routers, DiaMailbox can queue messages and track subscribers but cannot resolve which subscribers should receive a message sent to a given address. `Send<T>` stamps an opaque `Address` on each queued message; `Drain<T>` iterates those messages — but nothing yet maps an `Address` to the set of `SubscriberId` values that should receive it.

The router system bridges that gap. `IMailboxRouter` is the pluggable contract that domain modules implement: given an `Address` and the live subscriber set for a type, produce the matched subset. `RegisterRouter` wires a router into the mailbox by its `StringCRC` ID. `GetRouter` lets domain consumers retrieve a registered router for direct use. `Resolve<T>` is the type-aware wrapper that the delivery pass calls: it looks up the router by `Address::routerId`, hands it the live `GetSubscribersForType<T>()` result, and fills `outMatched`.

This feature also ships the testing utilities agreed with the user — `MockRouter` and `MailboxFixture` — so that consumers writing routing tests do not need a full entity world. Both live in `Dia/DiaMailbox/Testing/` per the module test-utilities convention.

## Solution Overview

`IMailboxRouter` is a pure-virtual interface with two methods: `GetRouterId()` returns the `StringCRC` the router is registered under; `Resolve(addr, liveSubscribers, outMatched)` fills `outMatched` with the subscriber IDs from `liveSubscribers` that should receive a message sent to `addr`. The router is entirely type-agnostic: it sees `Address::payload` bits and a flat `SubscriberSet`; the type dimension is handled by `Resolve<T>` before the router is invoked.

`Mailbox` stores a fixed-capacity array of `IMailboxRouter*` raw pointers. Caller owns the pointed-to router (SD-MBX-005 / AI-Q5). `RegisterRouter` inserts by `routerId`; duplicate registration returns `false`. `GetRouter` performs a linear scan and returns the pointer or `nullptr`. Router count is expected to be small (one per domain module in scope) so linear scan is acceptable for v1.

`Resolve<T>` is a template method on `Mailbox`. It looks up the router for `addr.routerId` via `GetRouter`. If not found, it emits `DIA_LOG_WARNING` and returns `false`. If `addr.routerId` is `StringCRC{}` (null CRC — `kNullRouter`), it returns `false` with an empty `outMatched` and no warning (null router is a valid use-case: typed deferred buffer without routing). When the router is found, `Resolve<T>` calls `GetSubscribersForType<T>()` to obtain the live subscriber set, then delegates to `router->Resolve(addr, liveSubscribers, outMatched)`.

`MockRouter` stores a rule table mapping `uint64_t` payload values to `SubscriberSet` results. On each `Resolve` call it looks up `addr.payload`; if found it copies the matched set into `outMatched` and increments an internal call counter. `MailboxFixture` constructs a `Mailbox` with common test message types pre-registered at default capacities and exposes a `GetRouter` factory that creates and registers a `MockRouter` on demand.

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `RegisterRouter(router)` returns `true` and makes the router retrievable by its `GetRouterId()` | Unit test |
| AC2 | `RegisterRouter(router)` with a `routerId` already registered returns `false`; the original router remains registered | Unit test: register twice, assert second call returns false, GetRouter still returns first router |
| AC3 | `GetRouter(routerId)` returns the registered router pointer | Unit test |
| AC4 | `GetRouter(routerId)` for an unregistered `routerId` returns `nullptr` | Unit test |
| AC5 | `Resolve<T>(addr, outMatched)` calls `router->Resolve(addr, liveSubscribers, outMatched)` exactly once | Unit test using `MockRouter::ResolveCallCount()` |
| AC6 | `Resolve<T>(addr, outMatched)` correctly fills `outMatched` with subscribers returned by the router | Unit test: register subscribers A and B, configure `MockRouter` to match A, assert `outMatched` contains A and not B |
| AC7 | `Resolve<T>` with `addr.routerId == StringCRC{}` (null CRC) returns `false` and leaves `outMatched` empty; no warning emitted | Unit test |
| AC8 | `Resolve<T>` with an unregistered `addr.routerId` (non-null CRC) returns `false`, leaves `outMatched` empty, and emits `DIA_LOG_WARNING` | Unit test: capture log output; assert false returned, outMatched empty, warning present |
| AC9 | `Resolve<T>` picks the correct subscriber list: subscribers registered for type `A` are not visible when resolving for type `B` | Unit test: subscribe to MsgA and MsgB separately, resolve MsgA, assert `outMatched` contains only MsgA subscribers |
| AC10 | The mailbox does not own router lifetime: destroying the router without calling `RegisterRouter` for a different instance does not crash `GetRouter` or `Resolve` when the old pointer is stale — the caller must unregister first (documented contract, not enforced) | Code review + documented precondition in `IMailboxRouter` header |
| AC11 | `MockRouter::AddRule(payload, subscribers)` causes `Resolve` to return `subscribers` when `addr.payload == payload` | Unit test |
| AC12 | `MockRouter::ResolveCallCount()` starts at zero and increments by one per `Resolve` call | Unit test: assert count is 0, call Resolve, assert count is 1, call Resolve again, assert count is 2 |
| AC13 | `MockRouter::ClearRules()` causes a subsequent `Resolve` call for a previously configured payload to produce an empty `outMatched` | Unit test |
| AC14 | `MailboxFixture` constructs successfully; `GetMailbox()` returns a `Mailbox` with at least one pre-registered test message type usable via `Send` and `Drain` without an additional `RegisterType` call | Unit test |

## Public API

```cpp
// Dia/DiaMailbox/IMailboxRouter.h
#pragma once
#include <DiaMailbox/MailboxTypes.h>
#include <DiaCore/Core/StringCRC/StringCRC.h>

namespace Dia::Mailbox {

    // Pluggable address resolver. Domain modules implement this interface and register
    // an instance with Mailbox::RegisterRouter. The mailbox does not own the pointer.
    //
    // Precondition for Resolve: addr.routerId matches GetRouterId().
    // Postcondition: outMatched contains the subset of liveSubscribers that should
    //   receive the message; outMatched may be empty (no-match is valid).
    //
    // Thread safety: single-threaded (SD-MBX-007). Do not call Resolve from multiple
    //   threads concurrently.
    //
    // Lifetime contract: The router pointer must remain valid for as long as it is
    //   registered with the Mailbox. The caller must unregister (by registering a
    //   replacement or letting the Mailbox be destroyed) before the router's storage
    //   is released. DiaMailbox does NOT manage router lifetime.
    class IMailboxRouter {
    public:
        virtual ~IMailboxRouter() = default;

        virtual Dia::Core::StringCRC GetRouterId() const = 0;

        // Given an opaque address (routerId guaranteed to match GetRouterId()) and the
        // current live subscriber set for the resolved message type, fill outMatched
        // with the subscribers that should receive this message.
        // liveSubscribers is read-only. outMatched is pre-cleared by the caller.
        virtual void Resolve(const Address& addr,
                             const SubscriberSet& liveSubscribers,
                             SubscriberSet& outMatched) = 0;
    };

} // namespace Dia::Mailbox


// Amendments to Dia/DiaMailbox/Mailbox.h
// (the following methods are added to the existing Mailbox class from prior features)
namespace Dia::Mailbox {

    // Maximum number of routers that can be registered with a single Mailbox instance.
    // Small bound: one router per domain module (entity router, editor router, etc.).
    constexpr uint32_t kMaxRouters = 16;

    class Mailbox {
    public:
        // ... existing RegisterType / Send / Drain / Subscribe / Unsubscribe
        //     from typed-queue and subscriptions features ...

        // Register a router. The Mailbox stores the raw pointer; caller owns lifetime.
        // Returns false if a router with the same routerId is already registered.
        // routerId is obtained from router->GetRouterId().
        bool RegisterRouter(IMailboxRouter* router);

        // Return the router registered under routerId, or nullptr if not found.
        IMailboxRouter* GetRouter(Dia::Core::StringCRC routerId);

        // Resolve an address to the set of matching subscribers for message type T.
        //
        // Steps:
        //   1. If addr.routerId == StringCRC{} (kNullRouter): clear outMatched, return false.
        //      No warning — null router is a valid unrouted use-case.
        //   2. Look up router via GetRouter(addr.routerId).
        //      If not found: clear outMatched, emit DIA_LOG_WARNING, return false.
        //   3. Obtain liveSubscribers = GetSubscribersForType<T>().
        //   4. Clear outMatched.
        //   5. Call router->Resolve(addr, liveSubscribers, outMatched).
        //   6. Return true.
        //
        // outMatched is always cleared before the router is invoked (step 4), so
        // the router receives a clean output buffer regardless of its prior state.
        template <class T>
        bool Resolve(const Address& addr, SubscriberSet& outMatched);

    private:
        // Fixed-capacity router table — IMailboxRouter* pointers, not owned.
        // Linear scan on GetRouter; router count expected to be small (kMaxRouters).
        Dia::Core::Containers::DynamicArrayC<IMailboxRouter*, kMaxRouters> mRouters;
    };

} // namespace Dia::Mailbox


// Testing utilities — Dia/DiaMailbox/Testing/MockRouter.h
#pragma once
#include <DiaMailbox/IMailboxRouter.h>

namespace Dia::Mailbox::Testing {

    // Configurable IMailboxRouter for unit tests. Rule-based: for each registered
    // payload value, Resolve returns the associated SubscriberSet. Payloads with no
    // rule produce an empty outMatched.
    class MockRouter : public IMailboxRouter {
    public:
        explicit MockRouter(Dia::Core::StringCRC routerId);

        // Add a rule: when addr.payload == payload, outMatched receives matchedSubscribers.
        // Replaces any existing rule for the same payload.
        void AddRule(uint64_t payload, const SubscriberSet& matchedSubscribers);

        // Remove all rules. Subsequent Resolve calls will produce empty outMatched.
        void ClearRules();

        // Number of times Resolve has been called since construction (or last ClearRules).
        int ResolveCallCount() const;

        // IMailboxRouter
        Dia::Core::StringCRC GetRouterId() const override;
        void Resolve(const Address& addr,
                     const SubscriberSet& liveSubscribers,
                     SubscriberSet& outMatched) override;

    private:
        Dia::Core::StringCRC mRouterId;
        int mResolveCallCount;

        // Rule storage: parallel arrays of payload keys and matched sets.
        // Fixed capacity matching kMaxRouters for v1 test scenarios.
        static constexpr uint32_t kMaxRules = 16;
        Dia::Core::Containers::DynamicArrayC<uint64_t,     kMaxRules> mRulePayloads;
        Dia::Core::Containers::DynamicArrayC<SubscriberSet, kMaxRules> mRuleResults;
    };

} // namespace Dia::Mailbox::Testing


// Testing utilities — Dia/DiaMailbox/Testing/MailboxFixture.h
#pragma once
#include <DiaMailbox/Mailbox.h>
#include <DiaMailbox/Testing/MockRouter.h>

namespace Dia::Mailbox::Testing {

    // Pre-built Mailbox with common test message types registered at default capacities.
    // Intended for use as a base in GoogleTest fixtures. Eliminates boilerplate RegisterType
    // calls in every routing test.
    //
    // Pre-registered types (capacity 64, OverflowPolicy::DropOldest each):
    //   - TestMessageA  (defined in Testing/TestMessages.h)
    //   - TestMessageB  (defined in Testing/TestMessages.h)
    //
    // MockRouter instances are created on demand via GetRouter and remain owned by the fixture.
    class MailboxFixture {
    public:
        MailboxFixture();

        Mailbox& GetMailbox();

        // Return the MockRouter registered under routerId. Creates and registers a new
        // MockRouter if none exists for this ID. Ownership stays with the fixture.
        MockRouter& GetRouter(Dia::Core::StringCRC routerId);

    private:
        Mailbox mMailbox;

        static constexpr uint32_t kMaxFixtureRouters = kMaxRouters;
        Dia::Core::Containers::DynamicArrayC<MockRouter, kMaxFixtureRouters> mMockRouters;
    };

} // namespace Dia::Mailbox::Testing
```

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Write `Dia/DiaMailbox/IMailboxRouter.h` — pure-virtual interface with `GetRouterId()` and `Resolve()` | AC5, AC6 | Planned | haiku | Plain header; no .cpp needed |
| 2 | Implement `Mailbox::RegisterRouter` — insert into `mRouters`; return false on duplicate `routerId`; `kMaxRouters` capacity guard with `DIA_LOG_WARNING` | AC1, AC2 | Planned | sonnet | Add `mRouters` member and `kMaxRouters` constant |
| 3 | Implement `Mailbox::GetRouter` — linear scan of `mRouters` by `routerId`; return nullptr if not found | AC3, AC4 | Planned | haiku | Small array; linear scan is correct for v1 |
| 4 | Implement `Mailbox::Resolve<T>` — null-CRC guard (false, no warning); unregistered router guard (false + DIA_LOG_WARNING); live-subscriber fetch via `GetSubscribersForType<T>()`; outMatched clear; router dispatch; return true | AC5, AC6, AC7, AC8, AC9 | Planned | sonnet | Template method; see Public API steps 1–6 |
| 5 | Write `Dia/DiaMailbox/Testing/TestMessages.h` — `TestMessageA` and `TestMessageB` plain structs used by `MailboxFixture` and unit tests | AC14 | Planned | haiku | |
| 6 | Write `Dia/DiaMailbox/Testing/MockRouter.h/.cpp` — rule table, `AddRule`, `ClearRules`, `ResolveCallCount`, `IMailboxRouter::Resolve` implementation | AC11, AC12, AC13 | Planned | sonnet | |
| 7 | Write `Dia/DiaMailbox/Testing/MailboxFixture.h/.cpp` — pre-registers `TestMessageA`/`TestMessageB`; `GetMailbox()`; `GetRouter` create-on-demand | AC14 | Planned | sonnet | |
| 8 | Write GoogleTests — `GoogleTests/DiaMailbox/RouterTests.cpp` covering AC1–AC14 | All ACs | Planned | sonnet | Use `MailboxFixture` as base; `MockRouter` for all Resolve tests |
| 9 | Update `DiaMailbox.vcxproj` + `.vcxproj.filters` to include `IMailboxRouter.h`, `Testing/MockRouter.h/.cpp`, `Testing/MailboxFixture.h/.cpp`, `Testing/TestMessages.h` | — | Planned | haiku | vcxproj created by `module-and-build` which runs first |

## Binding Decisions Compliance

| ID | Decision (summary) | Compliance |
|----|--------------------|------------|
| PD-001 | StringCRC for all entity/component IDs | Router IDs are `Dia::Core::StringCRC`. `SubscriberId::value` is opaque; DiaMailbox does not interpret it. No raw string keys in any router registration path. |
| PD-004 | No STL containers in public APIs | `mRouters` is `DynamicArrayC<IMailboxRouter*, kMaxRouters>`. `SubscriberSet` is `DynamicArrayC<SubscriberId, 64>`. `MockRouter` rule storage uses parallel `DynamicArrayC` arrays. No STL type in any public signature. |
| PD-005 | x64 only | `DynamicArrayC` and `StringCRC` are x64-native. No 32-bit size assumptions. |
| PD-006 | Visual Studio project files source of truth | `DiaMailbox.vcxproj` and `.vcxproj.filters` updated manually in Task 9 (or deferred to `module-and-build`). |
| PD-007 | C++20 required | `Resolve<T>` is a template method compiled under `/std:c++20`. No C++20 features strictly required beyond the existing baseline; the feature must not block compilation under that standard. |
| PD-008 | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaMailbox.vcxproj` must not override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard`. |
| AD-001 | Module system with YAML frontmatter | `dia.mailbox.architecture.module.md` updated in `module-and-build` feature to declare the router subsystem and `Testing/` directory as part of the public API. |
| AD-002 | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Namespace `Dia::<Module>::` | All production types in `Dia::Mailbox::`. Test utilities in `Dia::Mailbox::Testing::`. Both are sub-namespaces of `Dia::Mailbox::` per SD-MBX-009. |
| SD-MBX-001 | Address is `(StringCRC routerId, uint64_t payload)` — opaque to DiaMailbox | `Resolve<T>` reads `addr.routerId` only to look up the registered router. `addr.payload` is passed unchanged to `router->Resolve`; DiaMailbox never interprets payload bits. |
| SD-MBX-002 | Per-type ring buffers with compile-time capacity | Not directly exercised by this feature. `Resolve<T>` reads the per-type subscriber list (not the ring buffer) via `GetSubscribersForType<T>()`. |
| SD-MBX-003 | Delivery is polled (`Drain`), not callback-on-send | Routers are invoked by domain consumers at their chosen drain point, not during `Send`. `Resolve` is never called by `Send` or `Drain` internally. |
| SD-MBX-004 | Subscription lifetime: caller manages, mailbox lifetime is upper bound | Not directly exercised; lifetime management is the `subscriptions` feature's contract. Routers depend on subscriptions being live; they do not manage them. |
| SD-MBX-005 | Routers register against `StringCRC` ID, not type | `RegisterRouter` keys on `router->GetRouterId()` (a `StringCRC`). Multiple routers with different IDs can coexist in the same mailbox. Type-based registration is explicitly not used. |
| SD-MBX-006 | Default overflow is `DropOldest` with `DIA_LOG_WARNING`; `Assert` is opt-in | Not directly applicable to router registration. Router table overflow (`kMaxRouters` exceeded) emits `DIA_LOG_WARNING` and returns false — same defensive spirit. |
| SD-MBX-007 | Single-threaded | No locks in `RegisterRouter`, `GetRouter`, or `Resolve`. Concurrent access is undefined; documented on `IMailboxRouter`. |
| SD-MBX-008 | Mailbox is non-copyable, non-movable | `mRouters` is a `DynamicArrayC` by value; copy/move would silently alias router pointers. Existing deleted copy/move constructors (from prior features) remain in force. |
| SD-MBX-009 | Namespace is `Dia::Mailbox::` | All new production types in `Dia::Mailbox::`. `Dia::Mailbox::Testing::` is a sub-namespace for test utilities. |
| SD-MBX-010 | `Resolve` called by domain consumers, not internally during `Send` | `Resolve<T>` is a public method on `Mailbox` with no internal caller. `Send<T>` and `Drain<T>` do not invoke any router. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Router registration capacity | `kMaxRouters = 16` — is this enough? | Yes for v1. Expected registrants: one entity router (diaentitytemplate), one editor preview router (DiaEditor, future), and a small number of system-keyed routers for cross-module messaging. Sixteen covers that topology with headroom. If a consumer hits the limit it will receive `DIA_LOG_WARNING`; the constant can be raised without any API change. |
| 2 | Null router semantics | `addr.routerId == StringCRC{}` returns false with no warning. Is silent failure appropriate? | Yes. Null router is a documented valid use-case: a caller using DiaMailbox as a typed deferred buffer without routing (e.g. broadcasting to all subscribers via a separate path, or deferring routing setup). Emitting a warning on every null-router `Resolve` would create log spam in that usage pattern. The distinction from unregistered-non-null is: null is intentional, unregistered-non-null is likely a bug (missing `RegisterRouter` call). |
| 3 | `Resolve<T>` clearing outMatched before router call | The spec mandates that `Resolve<T>` clears `outMatched` before invoking `router->Resolve`. Could this mask a double-call bug? | It is the right contract. `outMatched` is an output parameter; the caller should not rely on its pre-call state. Clearing upfront makes `router->Resolve` implementations simpler (append-only into a clean buffer) and prevents stale data from a prior `Resolve` call leaking into a new one. A double-call bug (calling `Resolve` twice and accumulating results) is a caller bug, not a DiaMailbox bug; clearing does not mask it since the second call overwrites the first. |
| 4 | `GetSubscribersForType<T>()` snapshot vs. live reference | `Resolve<T>` passes `GetSubscribersForType<T>()` (a `const SubscriberSet&`) as `liveSubscribers` to the router. If the router calls `Subscribe`/`Unsubscribe` during `Resolve`, does this invalidate the reference? | Mutating the subscriber list during `Resolve` is documented as a caller error (SD-MBX-007 single-threaded; concurrent mutation is undefined). In practice, routers are domain modules that do not hold a reference to the same `Mailbox` they are being called from. The per-type `SubscriberSet` is stored by value in the type registry and its address is stable (see `subscriptions` AI-Q6). The reference does not become dangling from within a single-threaded `Resolve` call. |
| 5 | Router pointer ownership and stale pointer access | The spec documents that the caller must unregister before destroying the router. Is there any safety net? | No runtime safety net — DiaMailbox stores raw pointers intentionally (SD-MBX-005). Providing a safety net (e.g. a weak-pointer wrapper) would introduce per-call overhead and complicate the interface for a contract that is straightforward to uphold: domain modules own their routers as members and unregister in their destructor. The documentation on `IMailboxRouter` (lifetime contract section) and AC10 (code review) are the enforcement mechanisms. |
| 6 | `MockRouter` rule lookup on missing payload | If `Resolve` is called with a payload not in the rule table, `outMatched` is left empty. Should `MockRouter` assert or warn? | No assertion, no warning — empty `outMatched` is a valid router outcome. A test that expects a match but got none will fail on its own AC assertion (e.g. asserting `outMatched` contains subscriber A). Adding a warning inside `MockRouter` would couple test helper behaviour to production warning infrastructure, which is undesirable for a test utility. |
| 7 | `MailboxFixture::GetRouter` creates on demand | `GetRouter` registers a new `MockRouter` if none exists for the given ID. Does this race with `RegisterRouter` returning false on duplicates? | No — `MailboxFixture` is single-threaded and idempotent per ID. The first call creates and registers the `MockRouter`; subsequent calls return the already-created instance without attempting re-registration. The underlying `RegisterRouter` duplicate-ID guard is not triggered for the same ID after the first call. |
| 8 | `Resolve<T>` template and router type-agnosticism | The router's `Resolve` method is not templated — it sees `SubscriberSet` regardless of `T`. How does `Resolve<T>` thread type information through? | `Resolve<T>` uses the type parameter only to call `GetSubscribersForType<T>()`, which returns the correct per-type subscriber list for `T`. The router receives a `const SubscriberSet&` that happens to contain only subscribers for type `T`, but the router itself is unaware of `T`. This is the correct design: the type-to-subscriber-set mapping is DiaMailbox's responsibility, and the router's job is address-to-subscriber-subset, not type resolution. |
| 9 | `Resolve` called during `Drain` visitor | A `Drain<T>` visitor could call `Resolve<T>` for each message to find its matching subscribers. Is this safe? | Yes — `Drain<T>` iterates the ring buffer; `Resolve<T>` reads the subscriber list (a separate data structure). There is no iterator invalidation between the two. `Resolve` does not mutate any structure that `Drain` is iterating. This is the intended usage pattern: the delivery pass drains the queue, and for each message calls `Resolve` to route it to the right subscribers. |
| 10 | Router not found: warning frequency | `Resolve<T>` emits `DIA_LOG_WARNING` every time it is called with an unregistered non-null `routerId`. Could this cause log spam? | Yes, if a consumer calls `Resolve` on every queued message and a router is missing. This is a configuration bug (missing `RegisterRouter` call) rather than a runtime condition. The warning is the correct signal: fix the missing registration rather than silencing the warning. If spam becomes a problem in a specific scenario, the consumer can check `GetRouter(routerId) != nullptr` once before entering the drain loop and handle the missing router case explicitly. A per-frame dedup mechanism (like the overflow counter in `typed-queue`) is not warranted here since unregistered-router calls should never occur in production. |
| 11 | `SubscriberSet` capacity and `MockRouter` rule storage | `SubscriberSet` is `DynamicArrayC<SubscriberId, 64>`. `MockRouter` stores `SubscriberSet` values in its rule table. Are these stored by value in the `DynamicArrayC<SubscriberSet, kMaxRules>`? | Yes — stored by value. Each rule slot holds a full `SubscriberSet` (64 × 8 bytes = 512 bytes per slot; 16 slots = 8 KB). This is acceptable for a test utility that lives in `Testing/` and is never linked into production binaries. A production router resolves directly into a caller-provided `outMatched` buffer; no per-rule storage overhead. |
| 12 | Interaction with `subscriptions` feature's `GetSubscribersForType` for unregistered types | `Resolve<T>` calls `GetSubscribersForType<T>()`. If `T` was never registered, `GetSubscribersForType` returns a static empty `SubscriberSet` (per `subscriptions` AC11). `Resolve` then calls `router->Resolve` with an empty live-subscriber set. Is this the right behaviour? | Yes. The router receives an empty `liveSubscribers` and produces an empty `outMatched`. `Resolve<T>` returns `true` (the router was found and called). The empty result is semantically correct: there are no subscribers for an unregistered type. The missing `RegisterType<T>` call is a separate configuration concern; `Send<T>` for unregistered types already guards with its own warning (from the `typed-queue` feature). |

---

## Status

`Approved` — 2026-05-21. Steps 3 (Binding Decisions) and 4 (AI Review Questions) complete and confirmed.
