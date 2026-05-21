# Feature Spec: DiaMailbox — Typed Queue

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diamailbox.md | **typed-queue** |

**Status:** `Approved` — 2026-05-21. Steps 3 (Binding Decisions) and 4 (AI Review Questions) complete and confirmed.

**Depends on:** `module-and-build` (vcxproj skeleton) and `address-and-types` — `Address`, `OverflowPolicy`, and `SubscriberSet` types must be defined in `MailboxTypes.h` before this feature can compile.

---

## Problem Statement

Without `RegisterType`, `Send`, and `Drain`, the DiaMailbox system has no transport capability. The `address-and-types` feature establishes the vocabulary types, but there is no container, no enqueue path, and no delivery path. This feature delivers the per-type ring buffer that lets producers queue typed messages and consumers drain them in FIFO order at a controlled point in the frame. It is the core transport primitive on which all higher-level DiaMailbox features (`subscriptions`, `routers`) sit.

The specific problems solved:

1. **No typed queue storage.** There is nowhere to put a message. This feature adds per-type fixed-capacity ring buffers allocated at registration time.
2. **No enqueue path.** There is no `Send`. This feature adds `Send<T>(addr, msg)` — a guaranteed-O(1) copy into the type's ring with the opaque `Address` attached.
3. **No drain path.** There is no way to consume messages. This feature adds `Drain<T>(visitor)` — FIFO delivery to a caller-supplied visitor, then clear.
4. **No overflow safety.** Without overflow handling, a full ring is undefined behaviour. This feature wires `DropOldest` (log-once-per-Drain) and `Assert` policies per SD-MBX-006, closing that gap.

Without this feature, `Mailbox` cannot accept, store, or deliver any message, making the system inoperable.

## Solution Overview

`Mailbox` maintains an internal type registry — a fixed-capacity table mapping a type key (derived from a compile-time CRC of `typeid(T)`) to a type-erased ring buffer descriptor. Each descriptor holds:

- A raw byte buffer sized `kCapacity * (sizeof(Address) + sizeof(T))`, allocated inline (no heap) via a fixed-size slot array.
- Head and tail indices (integers, wrapping at `kCapacity`).
- The overflow policy selected at `RegisterType` time.
- A `dropsThisDrain` counter (reset to zero at the start of each `Drain`).
- Typed `construct`, `copy`, and `destroy` function pointers so the type-erased layer can manage `T` lifetimes without knowing `T`.

**RegisterType\<T, kCapacity\>()** checks the registry for an existing entry; returns `false` if already registered. Otherwise it allocates a new descriptor into the registry's fixed slot array (no STL, no heap). The buffer layout stores slots of `{Address addr; T message;}` contiguously. Capacity is compile-time — the buffer is fixed in the descriptor object. The registry uses a `StringCRC` of the mangled type name as the key.

**Send\<T\>(addr, msg)** looks up the descriptor by type key. If unregistered, logs `DIA_LOG_WARNING` and returns `false`. If registered, it value-copies `addr` and `msg` into the next tail slot. Under `DropOldest`: if the ring is full, it advances head (dropping oldest), increments `dropsThisDrain`, then writes to the new tail. Under `Assert`: if the ring is full, `DIA_ASSERT` fires in Debug; in Release `Send` returns `false` without dropping or writing. On success, returns `true`.

**Drain\<T\>(visitor)** looks up the descriptor by type key. If unregistered or the ring is empty, it is a no-op. Otherwise it snapshots the current tail index before visiting (so messages sent during drain are deferred to the next pass — see AC12), then iterates head..snapshotTail calling `visitor(addr, msg)` for each slot in FIFO order. After the loop, if `dropsThisDrain > 0`, it emits `DIA_LOG_WARNING` once with the drop count and type name, then resets `dropsThisDrain` to zero. Finally it resets the ring (head = tail = 0, effectively clearing). The visitor signature is `void(const Address&, const T&)`.

**No STL.** The type registry is a `DynamicArrayC` of descriptor pointers (or a hand-rolled fixed array — implementation discretion). Each ring buffer slot array is a raw byte buffer in the descriptor. `DynamicArrayC` from DiaCore is the only permitted container.

**Single-threaded.** No locks, no atomics. Concurrent `Send`/`Drain` is undefined per SD-MBX-007.

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `RegisterType<T, kCapacity>()` returns `true` on first call for a type; a subsequent call for the same `T` returns `false` without modifying the existing buffer | Unit test: register same type twice, assert first true, second false, queue unaffected |
| AC2 | `RegisterType<T, kCapacity>(OverflowPolicy::DropOldest)` is the default; a type registered with no policy argument uses `DropOldest` | Unit test: fill queue past capacity with no explicit policy; assert oldest message absent from Drain, no assert fired |
| AC3 | `Send<T>` on an unregistered type returns `false` and emits `DIA_LOG_WARNING`; the warning is captured by the test fixture | Unit test |
| AC4 | `Send<T>` on a registered type with capacity > 0 returns `true` and the message is retrievable via `Drain<T>` | Unit test: send one message, drain, assert visitor called exactly once with matching `addr` and `message` |
| AC5 | `Drain<T>` delivers messages in FIFO order — first message sent is first visited | Unit test: send three messages with distinct values in order A, B, C; drain; assert visitor receives A then B then C |
| AC6 | `Drain<T>` on an empty ring (nothing sent, or called a second time after a first drain) is a no-op — visitor is never called | Unit test: drain empty queue; drain again after a first drain; assert visitor call count is zero in both cases |
| AC7 | `Drain<T>` clears the queue — a second `Drain<T>` immediately after the first visits zero messages | Unit test: send one message, drain (assert visited), drain again (assert visitor not called) |
| AC8 | Under `OverflowPolicy::DropOldest`: when the ring is full and `Send` is called, the oldest message is overwritten; the newest message is retained; `Send` returns `true` | Unit test: register with capacity 2, send A then B (full), send C (overflow), drain — assert B then C; A absent |
| AC9 | Under `OverflowPolicy::DropOldest`: `DIA_LOG_WARNING` is emitted once on `Drain` (not on each `Send`) when at least one drop occurred during the fill cycle; the warning carries the drop count and type name | Unit test: register capacity 2, send 4 messages, drain — assert exactly one warning emitted (not two), assert warning text contains drop count and type name |
| AC10 | Under `OverflowPolicy::DropOldest`: no warning is emitted on `Drain` when no drops occurred in the current fill cycle | Unit test: send one message into capacity-4 queue, drain, assert zero warnings |
| AC11 | Under `OverflowPolicy::Assert`: `DIA_ASSERT` fires in Debug when `Send` is called on a full ring; in Release `Send` returns `false` without modifying the ring or dropping any message | Unit test (Debug build): assert fires on overflow. Unit test (Release build): fill ring, send one more, assert returns false, drain visits original messages only |
| AC12 | Messages sent by a `Drain` visitor for the same type `T` are NOT delivered in the current `Drain` pass; they are deferred to the next `Drain` call | Unit test: in visitor, call `Send<T>` with new message; after drain completes assert visitor was not called a second time; call drain again, assert deferred message now delivered |
| AC13 | `Drain<T>` for an unregistered type is a no-op — visitor is never called, no crash, no warning | Unit test |
| AC14 | Two independently registered types (`T1`, `T2`) do not interfere: filling `T1`'s ring to overflow has no effect on `T2`'s ring; draining `T1` does not affect `T2`'s queue state | Unit test: register T1 cap 2 and T2 cap 4; overflow T1; send two messages to T2; drain T1; drain T2 — assert T2 still delivers both messages intact |
| AC15 | A large `T` (e.g. a struct of 128 bytes) is value-copied correctly into the ring and out via the visitor — no truncation, no aliasing, no use-after-free | Unit test: define a 128-byte struct with sentinel fields, send two, drain, assert all fields correct |

## Public API

```cpp
// Dia/DiaMailbox/Mailbox.h
#pragma once
#include <DiaMailbox/MailboxTypes.h>

namespace Dia::Mailbox {

    class Mailbox {
    public:
        Mailbox();
        ~Mailbox();  // destroys all registered type buffers

        Mailbox(const Mailbox&) = delete;
        Mailbox& operator=(const Mailbox&) = delete;

        // Register a typed queue with compile-time capacity.
        // Must be called before Send<T> or Drain<T> for type T.
        // Returns false if T is already registered — existing buffer is unchanged.
        // kCapacity must be > 0.
        template <class T, uint32_t kCapacity>
        bool RegisterType(OverflowPolicy policy = OverflowPolicy::DropOldest);

        // Append a message to type T's ring buffer with the given opaque address.
        // Returns true on success.
        // Returns false + DIA_LOG_WARNING if T is not registered.
        // Under OverflowPolicy::Assert: returns false (no drop) if ring is full in Release;
        //   DIA_ASSERT fires in Debug.
        // Under OverflowPolicy::DropOldest: drops oldest on full, returns true; drop counted
        //   for end-of-Drain warning.
        template <class T>
        bool Send(const Address& addr, const T& message);

        // Visit every queued message of type T in FIFO order via visitor, then clear the ring.
        // visitor signature: void(const Address&, const T&)
        // Messages sent during the visitor (Send<T> inside visitor) are NOT visited in the
        //   current pass — they are deferred to the next Drain call.
        // If T is not registered or the ring is empty, this is a no-op.
        // Emits DIA_LOG_WARNING once (not per drop) if any drops occurred since the last Drain.
        template <class T, class Visitor>
        void Drain(const Visitor& visitor);
    };

}  // namespace Dia::Mailbox
```

### Internal type-erased descriptor (not public — shown for implementers)

```cpp
// Internal: one instance per registered type, stored in Mailbox's registry.
// T-specific construction, copying, and destruction are captured as function pointers
// at RegisterType<T, kCapacity>() time so the Mailbox implementation file can manage
// typed slots without knowing T.
struct TypedQueueDescriptor {
    void*         slotBuffer;      // raw byte block: kCapacity * slotStride
    uint32_t      slotStride;      // sizeof(Address) + sizeof(T) + alignment padding
    uint32_t      capacity;
    uint32_t      head;
    uint32_t      tail;
    uint32_t      count;
    uint32_t      dropsThisDrain;
    OverflowPolicy policy;
    uint32_t      typeKey;         // compile-time CRC key for type T

    // Typed operations captured at registration time
    void (*copyConstruct)(void* dst, const void* src);  // copy-construct T at dst from src
    void (*destruct)(void* ptr);                         // destroy T at ptr
};
```

The `slotBuffer` is heap-allocated at `RegisterType` time (single `new uint8_t[kCapacity * slotStride]`) because capacity is a runtime parameter of the descriptor, even though it is a compile-time constant at the call site. The `slotStride` accounts for `sizeof(Address)` + `sizeof(T)` padded to alignment of `T`. The `Mailbox` destructor iterates descriptors and calls `destruct` on any live slots before freeing `slotBuffer`.

The type registry inside `Mailbox` is a `Dia::Core::Containers::DynamicArrayC<TypedQueueDescriptor*, kMaxTypes>` where `kMaxTypes` is a compile-time constant (e.g. 32). This is the only dynamic container in `Mailbox` — no STL, no `std::unordered_map`.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Define `TypedQueueDescriptor` internal struct in `Mailbox.cpp` — fields, typed function pointer slots, type key field | — | Planned | haiku | Internal only; no header surface |
| 2 | Implement `Mailbox` constructor/destructor — initialise empty registry; destructor iterates descriptors, calls `destruct` on live slots, frees `slotBuffer` | — | Planned | sonnet | Destructor must not crash on empty registry |
| 3 | Implement `RegisterType<T, kCapacity>()` — type key derivation, duplicate check, `slotBuffer` allocation, `copyConstruct`/`destruct` capture, descriptor push into registry | AC1, AC2 | Planned | sonnet | Compute `slotStride` carefully for alignment |
| 4 | Implement `Send<T>(addr, msg)` — registry lookup, unregistered path (warn + return false), DropOldest overflow path (advance head, increment dropsThisDrain), Assert overflow path (DIA_ASSERT / return false), normal enqueue path | AC3, AC4, AC8, AC11 | Planned | sonnet | Return false on full under Assert; true on drop under DropOldest |
| 5 | Implement `Drain<T>(visitor)` — registry lookup (no-op if absent), snapshot tail before loop, FIFO visit loop, post-loop dropsThisDrain warning (once), reset ring | AC5, AC6, AC7, AC9, AC10, AC12, AC13 | Planned | sonnet | Snapshot tail BEFORE visiting to enforce AC12 |
| 6 | Write `GoogleTests/DiaMailbox/TypedQueueTests.cpp` — tests for AC1–AC15 | AC1–AC15 | Planned | sonnet | Use `MailboxTestFixture.h` for warning capture |
| 7 | Write `Dia/DiaMailbox/Testing/MailboxTestFixture.h` — test utility capturing `DIA_LOG_WARNING` output and providing typed helpers | Supporting ACs | Planned | haiku | Follows `<Module>/Testing/` pattern |
| 8 | Update `DiaMailbox.vcxproj` — add `Mailbox.h`, `Mailbox.cpp`, `Testing/MailboxTestFixture.h` (vcxproj created by `module-and-build` which runs first) | AC15 (build) | Planned | haiku | |

## Binding Decisions Compliance

| ID | Decision (summary) | Compliance |
|----|--------------------|------------|
| PD-001 | StringCRC for all entity/component IDs | The type-key used to index the descriptor registry is a `StringCRC` derived from the type's compile-time identifier. `Address::routerId` (passed through unchanged on Send/Drain) is `StringCRC` per `address-and-types`. No raw strings in any public path. |
| PD-004 | No STL containers in public APIs | `Send`, `Drain`, and `RegisterType` signatures use only `Address`, `T`, `uint32_t`, `bool`, and the templated `Visitor`. Internally `Dia::Core::Containers::DynamicArrayC` is the only container. No STL (`std::vector`, `std::unordered_map`, etc.) anywhere in `Mailbox.h` or `Mailbox.cpp`. |
| PD-005 | x64 only | `uint32_t` head/tail indices, `uint64_t` address payload, pointer-sized `slotBuffer`. `DiaMailbox.vcxproj` targets x64 exclusively. |
| PD-006 | Visual Studio project files are source of truth | `DiaMailbox.vcxproj` and `.vcxproj.filters` maintained manually. Task 8 calls out the vcxproj addition. |
| PD-007 | C++20 required | `Mailbox.h` compiled under `/std:c++20`. Template member functions, `if constexpr` permitted where appropriate. |
| PD-008 | Directory.Build.props owns OutDir/IntDir/toolchain | `DiaMailbox.vcxproj` must not override `OutDir`, `IntDir`, `PlatformToolset`, `WindowsTargetPlatformVersion`, or `LanguageStandard`. |
| AD-001 | Module system with YAML frontmatter | `dia.mailbox.architecture.module.md` updated (or created in `module-and-build`) to declare `Mailbox.h`/`Mailbox.cpp` as public API and list `DiaCore` as the only dependency. |
| AD-002 | No STL containers in public APIs | Reinforces PD-004. |
| AD-003 | Namespace `Dia::<Module>::` | All code in `Dia::Mailbox::` per SD-MBX-009. |
| SD-MBX-001 | Address is `(StringCRC routerId, uint64_t payload)` — opaque to DiaMailbox | `Send` copies `Address` into each ring slot verbatim. `Drain` passes the stored `Address` to the visitor unchanged. `Mailbox` never inspects or decodes `payload`. |
| SD-MBX-002 | Per-type ring buffers with compile-time capacity (`Register<T, kCap>()`) | `RegisterType<T, kCapacity>()` allocates a fixed buffer of `kCapacity` slots. Capacity is a compile-time template parameter; the buffer never grows. Matches SD-CORE-001 fixed-capacity convention. |
| SD-MBX-003 | Delivery is polled — `Drain<T>(visitor)` — not callback-on-send | `Send` only enqueues. Delivery happens only when `Drain` is called. Caller controls delivery timing. |
| SD-MBX-006 | Default overflow is DropOldest with `DIA_LOG_WARNING`; Assert is opt-in | `RegisterType` defaults to `OverflowPolicy::DropOldest`. Drop warning is emitted once per `Drain` (not per `Send`) via a `dropsThisDrain` counter (AI Review Q6). `Assert` policy fires `DIA_ASSERT` in Debug; returns false in Release. |
| SD-MBX-007 | Single-threaded — no internal locking | `Mailbox` has no mutexes, no atomics, no thread-local storage. Concurrent access is undefined behaviour by design. |
| SD-MBX-008 | Mailbox is non-copyable, non-movable | Copy constructor and copy assignment are `= delete`. Move is not provided (would require remapping all outstanding descriptors). |
| SD-MBX-009 | Namespace is `Dia::Mailbox::` | All public and internal types in `Dia::Mailbox::`. |
| SD-MBX-010 | `Resolve` called by domain consumers, not internally during `Send` | This feature does not implement `Resolve` — that is the `routers` feature. `Send` is O(1) append only; no router interaction. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Type key derivation | How is the compile-time type key for `T` derived without `std::type_index` (which is STL)? | Use a compile-time `StringCRC` applied to `__FUNCTION__` inside a helper template: `template<class T> constexpr uint32_t TypeKey() { return StringCRC(__FUNCSIG__); }` (MSVC). `__FUNCSIG__` includes the full template instantiation string, giving a unique CRC per type. This avoids `std::type_info`/`std::type_index` entirely and is O(0) at runtime — the CRC is folded at compile time. |
| 2 | `slotBuffer` allocation | The descriptor uses a heap-allocated `slotBuffer`. Doesn't that violate PD-004 (no STL) or the spirit of fixed-capacity conventions? | PD-004 prohibits STL *containers* in public APIs; it does not prohibit `new`/`delete` in implementation internals. The fixed capacity is enforced at the call site (compile-time `kCapacity`) — the heap allocation is a single flat byte block, not a growable container. The buffer is allocated once at `RegisterType` time and freed once in the destructor. All access is via raw pointer arithmetic with bounds checked by head/tail indices — no STL involved. |
| 3 | Descriptor registry capacity | `DynamicArrayC<TypedQueueDescriptor*, kMaxTypes>` — what is `kMaxTypes` and is 32 enough? | 32 is the v1 default (matches typical module-per-stage type counts). If a consumer registers more than 32 types, `RegisterType` returns `false` after emitting `DIA_ASSERT`. `kMaxTypes` is a compile-time constant in `Mailbox.h` — easy to raise without API churn. Real consumers (DiaEntity) will size it based on their message type count when they spec out their usage. |
| 4 | `Drain` snapshot-tail pattern | Why snapshot `tail` before the visitor loop instead of looping until `count == 0`? | Looping until empty while allowing `Send` inside the visitor risks infinite loops if a visitor unconditionally re-sends the same type. Snapshotting `tail` before entry means the visitor loop is bounded to `count` at drain-start, regardless of how many new messages arrive during visitation. New sends land after `snapshotTail` in the ring, so they are safely deferred to the next `Drain` call (AC12, AI Review Q9 from system spec). |
| 5 | Ring clear semantics | After `Drain`, the ring is "cleared" — does this mean head = tail = count = 0, or does it advance head past visited entries? | Reset to head = 0, tail = 0, count = 0. This is equivalent to discarding all visited slots. Since `Drain` visits everything up to `snapshotTail`, resetting to zero is correct when `count` at snapshot time equals `count` at drain-end (no sends during drain for the same type). If sends occurred during drain, those slots sit between `snapshotTail` and the current tail — a correct reset must preserve those. Implementation: after the visitor loop, `head = snapshotTail % capacity`, `tail` is unchanged (already points past new sends), `count = count - snapshotCount`. This avoids discarding messages sent during drain. |
| 6 | Log spam — drops counted on Drain not Send | Why count drops on Send but emit only on Drain, rather than emitting on each Send? | Emitting on each `Send` when the ring is full would log once per dropped message — potentially thousands of warnings per second if a queue is undersized, flooding the log sink. The `dropsThisDrain` counter accumulates all drops since the previous `Drain` and emits a single warning at `Drain` time with the total count. This matches the system spec decision (SD-MBX-006, AI Review Q6) and the `DIA_LOG_WARNING` once-per-frame-per-type intent. |
| 7 | Empty `Drain` on unregistered type | Should `Drain<T>` on an unregistered type emit a warning, or silently no-op? | Silently no-op (AC13). Emitting a warning would be noisy for patterns where a consumer calls `Drain<T>` unconditionally each frame even during frames where no sender has registered the type (e.g. during startup, or after a stage where the type was never used). Unregistered-type `Send` does emit a warning (AC3) because a send without a registered queue is almost certainly a caller error; a drain without a registered queue is a benign early-frame pattern. |
| 8 | Large-T value-copy correctness | `T` is value-copied into a raw byte buffer via `copyConstruct`. Does this handle types with non-trivial copy constructors (e.g. structs owning a `DynamicArrayC`)? | Yes — the `copyConstruct` function pointer is captured at `RegisterType<T, kCapacity>()` as a lambda that calls placement-new copy constructor: `[](void* dst, const void* src){ new(dst) T(*static_cast<const T*>(src)); }`. This correctly invokes `T`'s copy constructor regardless of triviality. Similarly, `destruct` calls `static_cast<T*>(ptr)->~T()`. The only requirement is that `T` is copy-constructible — documented as a precondition of `RegisterType`. |
| 9 | Multiple `Mailbox` instances | Can two `Mailbox` objects coexist with the same types registered independently? | Yes — each `Mailbox` owns its own descriptor registry and `slotBuffer` allocations. Type keys are used only within a single `Mailbox` instance for registry lookup; there is no global registry. Two `Mailbox` instances with `RegisterType<Foo, 8>()` each have completely independent ring buffers. |
| 10 | `Address` storage in slots | Each ring slot stores both an `Address` (16 bytes with padding) and a `T`. Does `slotStride` account for `T`'s alignment requirements? | Yes — `slotStride` is computed at `RegisterType` time as `alignedSize(sizeof(Address)) + alignedSize(sizeof(T))` where `alignedSize(n)` rounds up to `alignof(T)` (or 8 bytes, whichever is larger). The `Address` sub-slot within each slot starts at offset 0; the `T` sub-slot starts at `alignedSize(sizeof(Address))`. This guarantees `T` is correctly aligned regardless of size. A `static_assert(alignof(T) <= 16)` guards against exotic over-aligned types at `RegisterType` instantiation time. |
| 11 | `Drain` ring reset and dropsThisDrain | `dropsThisDrain` is reset to zero after the warning is emitted at Drain end. What if `Drain` is called on an empty ring — is `dropsThisDrain` still reset? | No — if the ring is empty at `Drain` entry, the function returns immediately as a no-op before reaching the drop-warning and reset logic. `dropsThisDrain` is preserved across the no-op call. This means drops accumulated from `Send` calls before a valid `Drain` are always reported on the first non-empty `Drain`. If only empty drains follow an overflow cycle, the drops are eventually reported on the next frame where a message is actually drained. |
| 12 | Descriptor lookup cost | Registry lookup is linear scan over `DynamicArrayC<TypedQueueDescriptor*, kMaxTypes>`. Is O(N) acceptable? | Yes for v1 with `kMaxTypes = 32`. At 32 entries a linear scan is ~32 comparisons of `uint32_t` type keys — well under 100 ns. If a consumer registers 100+ types, the lookup cost would motivate a hash table, but that consumer would also be registering an unusually wide message vocabulary. The 32-type cap with `DIA_ASSERT` on excess registration keeps the cost bounded by contract. A hash-based registry can be added later as a non-breaking internal change. |

---

## Status

`Approved` — 2026-05-21. Steps 3 (Binding Decisions) and 4 (AI Review Questions) complete and confirmed.
