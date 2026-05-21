# Feature Spec: DiaMailbox — Address and Types

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diamailbox.md | **address-and-types** |

**Status:** `Approved` — 2026-05-21. Steps 3 (Binding Decisions) and 4 (AI Review Questions) complete and confirmed.

**Depends on:** `module-and-build` — the vcxproj skeleton must exist so this feature's files can compile and be tested via TDD. All other DiaMailbox features (`typed-queue`, `subscriptions`, `routers`) include this header.

---

## Problem Statement

The DiaMailbox system requires a shared set of value types (`Address`, `SubscriberId`, `OverflowPolicy`, `SubscriberSet`, `SubscriptionHandle` forward declaration) that every other DiaMailbox feature includes. These types are the vocabulary of the mailbox API — they appear in `Send`, `Drain`, `Subscribe`, `Unsubscribe`, `RegisterRouter`, and `Resolve` signatures. Without this foundation header, no other DiaMailbox feature can compile. There is no logic here: only type definitions, a forward declaration, and a type alias. The goal is to get these into a well-formed header that satisfies `PD-004` (no STL in public APIs), `PD-007` (C++20), and `SD-MBX-009` (namespace `Dia::Mailbox::`) before any behaviour is layered on top.

## Solution Overview

A single header `Dia/DiaMailbox/MailboxTypes.h` defines all public value types for the DiaMailbox system. It has no implementation file. The header pulls in only `DiaCore/StringId/StringCRC.h` and `DiaCore/Containers/Arrays/DynamicArrayC.h` — both already available as DiaMailbox's declared dependencies. `SubscriptionHandle` is declared as an opaque class (forward declaration only); its implementation is deferred to the `subscriptions` feature spec. `SubscriberSet` is a type alias for `Dia::Core::Containers::DynamicArrayC<SubscriberId, 64>`. The `OverflowPolicy` enum is `unsigned char` backed so it composes into per-type ring buffer metadata without padding cost. All types live in `Dia::Mailbox::`.

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `Address` is a value type with `Dia::Core::StringCRC routerId` and `uint64_t payload` members; both members are zero-initialised by a defaulted default constructor | Unit test: default-construct `Address`, assert `routerId == StringCRC{}` and `payload == 0` |
| AC2 | `Address::operator==` returns `true` iff both `routerId` and `payload` are equal; `operator!=` is its complement | Unit test: construct two equal and two differing `Address` instances, assert both operators |
| AC3 | `SubscriberId` is a value type with a single `uint64_t value` member; `operator==` compares `value` | Unit test: construct two `SubscriberId` instances with equal/unequal values, assert `==` and `!=` behaviour |
| AC4 | `OverflowPolicy` is an `enum class` backed by `unsigned char` with exactly two enumerators: `DropOldest` and `Assert` | Unit test: `static_assert(sizeof(OverflowPolicy) == 1)` and compile-time enum value checks |
| AC5 | `SubscriptionHandle` is declared as an opaque class (forward declaration only) in `Dia::Mailbox::` — it is legal to form a pointer or reference to it, but the type is incomplete | Compilation test: `SubscriptionHandle* p = nullptr;` compiles; `sizeof(SubscriptionHandle)` does not compile |
| AC6 | `SubscriberSet` is a type alias for `Dia::Core::Containers::DynamicArrayC<SubscriberId, 64>` | `static_assert(std::is_same_v<SubscriberSet, Dia::Core::Containers::DynamicArrayC<SubscriberId, 64>>)` |
| AC7 | `MailboxTypes.h` compiles in isolation (no other DiaMailbox headers required) — only `DiaCore` includes are permitted | Standalone compilation test: include only `MailboxTypes.h`, assert no compile error |
| AC8 | `MailboxTypes.h` contains no STL includes and no STL types in any public type definition | Code review + grep: no `#include <vector>`, `<string>`, `<map>`, `<unordered_map>`, `<array>`, or equivalent STL headers |
| AC9 | `Address` is copyable and copy-assignable; it is not trivially copyable because `StringCRC` has a user-defined copy constructor | Unit test: copy-construct an `Address`, assert equality with source |
| AC10 | `SubscriberId` is trivially copyable (`std::is_trivially_copyable_v<SubscriberId> == true`) | `static_assert` in unit test |

## Public API

```cpp
// Dia/DiaMailbox/MailboxTypes.h
#pragma once
#include <DiaCore/StringId/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia::Mailbox {

    // Opaque destination. routerId names which router knows how to interpret payload.
    // payload is router-defined: entity handle bits, type CRC, broadcast tag, etc.
    // DiaMailbox never inspects or decodes payload — that is the router's contract.
    struct Address {
        Dia::Core::StringCRC routerId;
        uint64_t             payload = 0;

        bool operator==(const Address& rhs) const;
        bool operator!=(const Address& rhs) const;
    };

    // Caller-managed identity for subscribers. Opaque to DiaMailbox. Common patterns:
    //   - A Handle<EntityType> reinterpreted to bits
    //   - A StringCRC module name cast to uint64_t
    // DiaMailbox treats the value as an opaque 64-bit key.
    struct SubscriberId {
        uint64_t value = 0;
        bool operator==(const SubscriberId& rhs) const { return value == rhs.value; }
        bool operator!=(const SubscriberId& rhs) const { return value != rhs.value; }
    };

    // Controls behaviour when a typed ring buffer is full at Send time.
    // DropOldest: overwrite the oldest entry and emit DIA_LOG_WARNING (default).
    // Assert:     DIA_ASSERT in Debug; Send returns false in Release without dropping.
    enum class OverflowPolicy : unsigned char {
        DropOldest,
        Assert
    };

    // Opaque subscription lifetime token. Implementation in the subscriptions feature.
    // Callers may hold pointers/references to SubscriptionHandle but never instantiate it
    // directly from this header.
    class SubscriptionHandle;

    // Caller-provided fixed-capacity subscriber list used by IMailboxRouter::Resolve
    // and Mailbox::Resolve. Capacity 64 is a v1 assumption: <64 subscribers per
    // resolution call. Sized to kMaxEntities when DiaEntity is specced.
    using SubscriberSet = Dia::Core::Containers::DynamicArrayC<SubscriberId, 64>;

} // namespace Dia::Mailbox
```

## Tasks

| # | Task | Status |
|---|------|--------|
| 1 | Create `Dia/DiaMailbox/` directory structure (matching module layout convention) | Planned |
| 2 | Write `MailboxTypes.h` — `Address`, `SubscriberId`, `OverflowPolicy`, `SubscriptionHandle` forward declaration, `SubscriberSet` alias | Planned |
| 3 | Write `MailboxTypes.cpp` — implement `Address::operator==` and `Address::operator!=` | Planned |
| 4 | Write GoogleTests for AC1–AC10 in `GoogleTests/DiaMailbox/MailboxTypesTests.cpp` | Planned |
| 5 | Update `DiaMailbox.vcxproj` to include `MailboxTypes.h` and `MailboxTypes.cpp` (vcxproj created by `module-and-build` which runs first) | Planned |

## Binding Decisions Compliance

| ID | Decision (summary) | Compliance |
|----|--------------------|------------|
| PD-001 | StringCRC for all entity/component IDs | `Address::routerId` is `Dia::Core::StringCRC`. `SubscriberId::value` is an opaque `uint64_t` — not an engine ID; callers (e.g. DiaEntity) will populate it from handle bits. DiaMailbox does not interpret it. |
| PD-004 | No STL containers in public APIs | `SubscriberSet` is `DynamicArrayC`. No STL type appears in any public type in `MailboxTypes.h`. |
| PD-005 | x64 only | `uint64_t payload` in `Address` and `SubscriberId::value` rely on 64-bit layout. `DiaMailbox.vcxproj` targets x64 exclusively. |
| PD-006 | Visual Studio project files are source of truth | `DiaMailbox.vcxproj` and `.vcxproj.filters` will be maintained manually (deferred to `module-and-build` feature). This feature's task list calls out the vcxproj addition explicitly. |
| PD-007 | C++20 required | `MailboxTypes.h` compiled under `/std:c++20`. Uses `static_assert` with `std::is_same_v` and `std::is_trivially_copyable_v` in tests. No new C++20 features strictly required by this header, but it must not block compilation under `/std:c++20`. |
| PD-008 | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaMailbox.vcxproj` must not override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard`. |
| AD-001 | Module system with YAML frontmatter | `dia.mailbox.architecture.module.md` created in `module-and-build` feature; this feature is the first to define public API for that doc to describe. |
| AD-002 | No STL containers in public APIs | Reinforces PD-004. `MailboxTypes.h` is strictly DiaCore-only for includes. |
| AD-003 | Namespace `Dia::<Module>::` | All types defined in `Dia::Mailbox::` namespace per SD-MBX-009. |
| SD-MBX-001 | Address is `(StringCRC routerId, uint64_t payload)` — opaque to DiaMailbox | `Address` struct matches this definition exactly. No payload decoding anywhere in this feature. |
| SD-MBX-002 | Per-type ring buffers with compile-time capacity | `SubscriberSet` is `DynamicArrayC<SubscriberId, 64>` — compile-time capacity. Ring buffers themselves are in the `typed-queue` feature but `SubscriberSet` establishes the pattern. |
| SD-MBX-003 | Delivery is polled (`Drain`), not callback-on-send | No callbacks in `MailboxTypes.h`. Types are passive data; delivery model is enforced in `typed-queue`. |
| SD-MBX-004 | Subscription lifetime: caller manages, mailbox lifetime is upper bound | `SubscriptionHandle` is forward-declared only. No lifetime coupling in this feature. |
| SD-MBX-005 | Routers register against `StringCRC` ID | `Address::routerId` is `StringCRC`. Router registration is in the `routers` feature. |
| SD-MBX-006 | Default overflow is DropOldest with `DIA_LOG_WARNING`; Assert is opt-in | `OverflowPolicy` enum provides exactly these two enumerators. Default value application is in `typed-queue`. |
| SD-MBX-007 | Single-threaded | No synchronisation primitives in `MailboxTypes.h`. Thread-safety policy is enforced at `Mailbox` level (typed-queue feature). |
| SD-MBX-008 | Mailbox is non-copyable, non-movable | `MailboxTypes.h` defines value types, not `Mailbox` itself. `Address` and `SubscriberId` are intentionally copyable (value semantics required for queue entries). |
| SD-MBX-009 | Namespace is `Dia::Mailbox::` | All types in `Dia::Mailbox::`. |
| SD-MBX-010 | `Resolve` called by domain consumers, not internally during `Send` | Not applicable to this feature — no Send or Resolve logic here. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | `Address` layout | Is `Address` guaranteed to be 16 bytes on x64? Could compiler padding break assumptions? | `StringCRC` is a `uint32_t` (4 bytes). With natural alignment, `uint64_t payload` requires 8-byte alignment, yielding 4 bytes of padding between `routerId` and `payload` — making `Address` 16 bytes total. This is expected. No code should rely on `Address` being 12 bytes. If stack size is a concern, passing by `const Address&` is the convention in Send/Drain signatures. |
| 2 | `SubscriberId` zero value | `SubscriberId{0}` is the default-constructed value. Is zero a valid subscriber ID or a sentinel? | Zero is treated as "uninitialised" by convention. DiaMailbox does not enforce this — callers (DiaEntity) are responsible for ensuring real subscriber IDs are non-zero. A `SubscriberSet` containing a zero-value `SubscriberId` would cause a delivered message to be discarded silently; documenting this as caller responsibility is sufficient for v1. |
| 3 | `SubscriptionHandle` forward declaration | Is a forward declaration in the same header as `SubscriberSet` correct, or does it create order-dependency problems for consumers? | A class forward declaration in `MailboxTypes.h` is legal and will not create order issues. Consumers that need the full definition (to call `Unsubscribe`) will include the subscriptions feature's header, which includes `MailboxTypes.h` and then provides the full `SubscriptionHandle` definition. This is the standard two-header pattern (forward in types, definition in implementation header). |
| 4 | `SubscriberSet` capacity | `DynamicArrayC<SubscriberId, 64>` — what if a router needs more than 64 subscribers? | v1 assumption is <64 subscribers per resolve call. If a broadcast to all entities exceeds 64, the router must either truncate (dropping delivery) or call `Resolve` in batches. Once DiaEntity is specced and sets `kMaxEntities`, this capacity is updated — it is a compile-time constant change with no API churn. The 64 capacity is explicitly documented as a placeholder in the spec. |
| 5 | `OverflowPolicy` enum size | Why `unsigned char` backing rather than the default `int`? | `OverflowPolicy` is stored per registered type inside `Mailbox`'s type registry. Using `unsigned char` keeps the per-type metadata compact (1 byte vs 4). It also makes intent explicit: this field never takes values outside 0–1. No runtime cost difference on x64, but self-documents the cardinality constraint. |
| 6 | Header include order | `MailboxTypes.h` includes `StringCRC.h` and `DynamicArrayC.h`. Could a transitive include from DiaCore accidentally pull in STL? | DiaCore headers are already constrained by PD-004 at the platform level. If a DiaCore header violates PD-004, that is a DiaCore bug — not a DiaMailbox concern. Grep verification of `MailboxTypes.h` itself (AC8) is sufficient to assert compliance for this feature. |
| 7 | `Address::operator==` implementation file | `Address` is a struct with trivial member types. Why have a `.cpp` at all? Could `operator==` be `inline` in the header? | Either is valid. An `inline` definition in the header is marginally cleaner for a struct this small. The `.cpp` is listed in tasks to follow the project's pattern of non-trivial operators having implementation files, but if the implementation team finds the operator trivially expressible inline, inlining it in the header is acceptable. The spec does not mandate either; implementation discretion applies. |
| 8 | `static_assert` in tests vs. header | ACs for `sizeof(OverflowPolicy) == 1` and trivial-copyability use `static_assert`. Should these live in the header or only in tests? | Tests only. Embedding size assertions in public headers creates a maintenance burden if the types evolve. The test is the verification gate; the header should not assert on its own layout. |
| 9 | `SubscriberId::operator!=` | The system spec only shows `operator==` for `SubscriberId`. Should `operator!=` also be provided? | Yes — omitting `operator!=` when `operator==` exists violates the rule of symmetry and forces consumers to write `!(a == b)`. The feature spec adds `operator!=` to the public API. This does not conflict with the system spec (which shows the minimum interface, not the exhaustive one). |
| 10 | Dependency on `DynamicArrayC` version | `DynamicArrayC` is used for `SubscriberSet`. Is the correct include path `DiaCore/Containers/Arrays/DynamicArrayC.h`? | Yes — confirmed from existing DiaCore usage patterns in the codebase (e.g. `DiaCore/Containers/Arrays/DynamicArrayC.h`). If the header path has moved, the `module-and-build` feature's vcxproj verification pass will catch it. |

---

## Status

`Approved` — 2026-05-21. Steps 3 (Binding Decisions) and 4 (AI Review Questions) complete and confirmed.
