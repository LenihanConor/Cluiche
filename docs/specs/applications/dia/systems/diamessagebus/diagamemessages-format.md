# Feature Spec: .diagamemessages Format

## Parent System
@docs/specs/applications/dia/systems/diamessagebus/diamessagebus.md

## Problem Statement

Game message types (C++ structs registered with `RegisterType<T>` / `RegisterProducer<T>` / `Subscribe<T>`) are hand-authored today. The schema browser needs a static source of truth for the editor graph, and any manually maintained mirror of C++ code will drift — both the struct definitions *and* the registration call sites. A protobuf-style IDL solves both: the `.diagamemessages` file is the source of truth from which the C++ structs **and their registration wiring** are generated. The developer's only job is to supply handler implementations. Zero struct drift *and* zero graph drift by construction (SD-MBX2-009).

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | A valid `.diagamemessages` file passes `dia validate manifest` with no errors | Run on the sample file |
| AC-2 | Missing required top-level keys (`schema`, `namespace`, `messages`) produce one error per missing key | Run on deliberately broken files |
| AC-3 | A message entry missing `id`, `router`, or `pass` produces an error naming the field and message index | Run on a file with an incomplete entry |
| AC-4 | `router` not in `{"broadcast","entity"}` produces an error | Run on `"router": "invalid"` |
| AC-5 | `pass` not in `{"primary","reaction"}` produces an error | Run on `"pass": "invalid"` |
| AC-6 | A `fields` entry missing `name` or `type` produces an error naming the field index | Run on a file with an incomplete field |
| AC-7 | `dia validate manifest` scans `*.diagamemessages` alongside `*.diaapp`, `*.diagame`, `*.diastage` | Run from repo root; count includes `.diagamemessages` files |
| AC-8 | `dia codegen messages <file>` generates a valid C++ header: `#pragma once`, namespace wrapper, one struct per message with `kTypeId` and typed fields | Run on the sample file; inspect output |
| AC-9 | Generated struct has `static constexpr Dia::Core::StringCRC kTypeId{ "TypeName" };` as its first member | Inspect generated output |
| AC-10 | `dia codegen messages <file> --output <path>` writes to the specified path | Run with explicit output path |
| AC-11 | Generated file has a `// GENERATED` header comment referencing the source `.diagamemessages` file | Inspect generated output |
| AC-12 | Generated file declares a `Handlers` struct with one `std::function` slot per message that has a non-empty `consumers` array; slot signature is `void(const <MessageType>&)` | Inspect generated output |
| AC-13 | Generated file declares `void RegisterMessages(Dia::MessageBus::Bus& bus, const Handlers& handlers)` that emits `RegisterType<T, capacity>(policy)` (capacity as a template argument) for every message, `RegisterProducer<T>(id)` for each producer, and `Subscribe<T>(id, handlers.slot)` for each consumer | Inspect generated output; compiles against `Bus` |
| AC-14 | `RegisterMessages` asserts (Debug) if a `Handlers` slot for a message with consumers is unbound (null `std::function`) | Unit test with an unbound slot |
| AC-15 | Per-message `capacity` (default 64) and `overflow` (`"assert"\|"drop_oldest"`, default `"assert"`) map to the `RegisterType<T>` capacity + `Dia::Mailbox::OverflowPolicy` arguments; an `overflow` value outside that set is a validation error | Inspect generated `RegisterType` call for a message overriding both; run validator on `"overflow":"drop_newest"` |
| AC-16 | Sample file for MessageBus test stage exists at `Cluiche/CluicheTest/Stages/MessageBusTestStage/messagebus_test_messages.diagamemessages` with `NetworkPulseEvent`, `DirectPingEvent`, `PongEvent`, `BurstEvent`; `dia codegen messages` on it produces a compilable header + wiring | `dia validate manifest` passes; generated header compiles |

## Design

### File Format

`.diagamemessages` is a JSON IDL. It is the **source of truth** — C++ structs are generated from it, not the reverse.

```json
{
    "schema": "diagamemessages/v1",
    "namespace": "CluicheTest::Messages",
    "includes": [
        "DiaEntity/EntityId.h"
    ],
    "messages": [
        {
            "id": "NetworkPulseEvent",
            "router": "broadcast",
            "pass": "primary",
            "capacity": 128,
            "overflow": "drop_oldest",
            "producers": ["EmitterSystem"],
            "consumers": ["ReceiverSystem"],
            "fields": [
                { "name": "sequenceId", "type": "uint32_t", "notes": "monotonically increasing per emitter" },
                { "name": "emitterId",  "type": "EntityId",  "notes": "which emitter fired" }
            ]
        },
        {
            "id": "PongEvent",
            "router": "broadcast",
            "pass": "reaction",
            "producers": ["ReceiverSystem"],
            "consumers": ["EmitterSystem"],
            "fields": [
                { "name": "responderId", "type": "EntityId", "notes": "receiver that replied" }
            ]
        }
    ]
}
```

### Field reference

| Field | Type | Required | Notes |
|-------|------|----------|-------|
| `schema` | string | yes | Must be `"diagamemessages/v1"` |
| `namespace` | string | yes | C++ namespace for generated structs (e.g. `"Dia::GameplayMessages"`) |
| `includes` | array of string | no | Headers included at top of generated file (e.g. `"DiaEntity/EntityId.h"`) |
| `messages` | array | yes | One entry per message type |
| `messages[].id` | string | yes | C++ struct name, becomes `kTypeId` value |
| `messages[].router` | string | yes | `"broadcast"` or `"entity"` |
| `messages[].pass` | string | yes | `"primary"` or `"reaction"` |
| `messages[].capacity` | int | no | Per-type mailbox capacity for `RegisterType<T>`; default `64` |
| `messages[].overflow` | string | no | `"assert"` \| `"drop_oldest"` → `Dia::Mailbox::OverflowPolicy`; default `"assert"`. (`OverflowPolicy` has only these two — no `drop_newest`.) |
| `messages[].producers` | array of string | yes | StringCRC IDs passed to `RegisterProducer<T>`; use `[]` if undeclared |
| `messages[].consumers` | array of string | yes | StringCRC IDs passed to `Subscribe<T>`; each becomes a `Handlers` slot; use `[]` if undeclared |
| `messages[].fields` | array | no | Payload field definitions |
| `messages[].fields[].name` | string | yes (if fields present) | C++ field name |
| `messages[].fields[].type` | string | yes (if fields present) | C++ type, passed through verbatim to generated code |
| `messages[].fields[].notes` | string | no | Preserved as `//` comment in generated output |

`producers` and `consumers` are required (use `[]` when undeclared) so the validator can flag absent arrays as errors rather than silently treating them as empty.

Field types are **pass-through** — the string is emitted verbatim into the generated C++ struct. No restricted set is enforced at this stage.

### Generated output

`dia codegen messages <file.diagamemessages> [--output <path.h>]` produces three things in one header: **(1)** the message structs, **(2)** a `Handlers` binding struct, and **(3)** a `RegisterMessages` wiring function. The developer supplies only the handler bodies.

```cpp
// GENERATED — do not edit. Source: messagebus_test_messages.diagamemessages
#pragma once
#include <DiaCore/String/StringCRC.h>
#include <DiaMailbox/Mailbox.h>        // OverflowPolicy
#include <DiaMessageBus/Bus.h>         // Bus
#include <DiaEntity/EntityId.h>
#include <functional>

namespace CluicheTest::Messages {

    // ── (1) Message structs ─────────────────────────────────────────────
    struct NetworkPulseEvent {
        static constexpr Dia::Core::StringCRC kTypeId{ "NetworkPulseEvent" };
        uint32_t sequenceId;  // monotonically increasing per emitter
        EntityId emitterId;   // which emitter fired
    };

    struct PongEvent {
        static constexpr Dia::Core::StringCRC kTypeId{ "PongEvent" };
        EntityId responderId;  // receiver that replied
    };

    // ── (2) Handler binding struct ──────────────────────────────────────
    // One slot per message that has consumers. Bind before RegisterMessages.
    struct Handlers {
        std::function<void(const NetworkPulseEvent&)> onNetworkPulseEvent;  // consumer: ReceiverSystem
        std::function<void(const PongEvent&)>         onPongEvent;          // consumer: EmitterSystem
    };

    // ── (3) Registration wiring ─────────────────────────────────────────
    inline void RegisterMessages(Dia::MessageBus::Bus& bus, const Handlers& handlers)
    {
        // NetworkPulseEvent — broadcast, primary, capacity 128, drop_oldest
        bus.RegisterType<NetworkPulseEvent, 128>(Dia::Mailbox::OverflowPolicy::DropOldest);
        bus.RegisterProducer<NetworkPulseEvent>(Dia::Core::StringCRC{ "EmitterSystem" });
        DIA_ASSERT(handlers.onNetworkPulseEvent, "Handlers::onNetworkPulseEvent is unbound");
        bus.Subscribe<NetworkPulseEvent>(Dia::Core::StringCRC{ "ReceiverSystem" }, handlers.onNetworkPulseEvent);

        // PongEvent — broadcast, reaction, capacity 64 (default), assert (default)
        bus.RegisterType<PongEvent, 64>(Dia::Mailbox::OverflowPolicy::Assert);
        bus.RegisterProducer<PongEvent>(Dia::Core::StringCRC{ "ReceiverSystem" });
        DIA_ASSERT(handlers.onPongEvent, "Handlers::onPongEvent is unbound");
        bus.Subscribe<PongEvent>(Dia::Core::StringCRC{ "EmitterSystem" }, handlers.onPongEvent);
    }

}
```

Default output path: `<same directory as source file>/<source-stem>.h`  
(e.g. `messagebus_test_messages.diagamemessages` → `messagebus_test_messages.h`)

`DiaCore/String/StringCRC.h`, `DiaMailbox/Mailbox.h`, `DiaMessageBus/Bus.h`, and `<functional>` are always included — required for `kTypeId`, `OverflowPolicy`, `Bus`, and the `Handlers` slots respectively.

**Why generate the wiring, not just structs (SD-MBX2-009):** if `RegisterProducer`/`Subscribe` call sites stayed hand-written, the producer/consumer graph in the `.diagamemessages` file would be a manual mirror of the code and could drift. Generating the wiring makes the file the single source of truth for the *connection graph*, not just the payloads. The developer cannot register a producer/consumer that isn't declared, and cannot declare one without it being wired.

**Why a `Handlers` struct of `std::function` (SD-MBX2-010):** handlers must be instance methods (they capture `this` to touch system state) — that rules out free-function pointers baked into generated code, and per the no-singletons / no-service-locator rule the generated wiring can't reach into a global to find the system. The `Handlers` struct is the seam: the developer binds each slot to a lambda/`std::bind` over their system instance, then hands it to `RegisterMessages`. Unbound slots for messages that declare consumers assert at registration (AC-14).

### File location

`.diagamemessages` files live alongside the C++ source that uses the generated header — not in `Assets/`, not deployed with the game. The editor and `dia` tools read from the source tree.

### Validator implementation

`cli_validate.py` gains `_validate_diagamemessages` and adds `"*.diagamemessages"` to the extension scan list. Validates structure per the field reference above.

### Codegen implementation

New `dia codegen messages` command in DiaCLI. Reads the JSON, renders the header template — structs, `Handlers` struct, and `RegisterMessages` wiring — writes to the output path. The `overflow` string maps to `Dia::Mailbox::OverflowPolicy` (`assert`→`Assert`, `drop_oldest`→`DropOldest`).

### Generated headers are committed (Q3/Q4)

Generated `.h` files are **committed to the repo** alongside their `.diagamemessages` source (both carry the `// GENERATED` banner). No custom MSBuild step — the header is just another project file, so there is no build-order dependency and the compiler and schema browser see the same artifact. Codegen is run **manually** (`dia codegen messages`) when the doc changes; you commit doc + header together.

To catch "edited the doc, forgot to regen," `dia docs precommit` gains a **staleness check**: it regenerates each `.diagamemessages` to a temp file and fails if the result differs from the committed header (same pattern as vcxproj-sync). This is a precommit concern, not a codegen AC — tracked in the module-and-build / precommit work, noted here for context.

## Tasks

| # | Task | Notes |
|---|------|-------|
| 1 | Add `_validate_diagamemessages` to `cli_validate.py`; add `*.diagamemessages` to scan list; validate `capacity`/`overflow` when present | Covers AC-1 through AC-7 |
| 2 | Add `dia codegen messages` command; render structs + `Handlers` struct + `RegisterMessages` wiring from IDL | Covers AC-8 through AC-15 |
| 3 | Write sample file `Cluiche/CluicheTest/Stages/MessageBusTestStage/messagebus_test_messages.diagamemessages`; run codegen to produce the stage's message header + wiring | Covers AC-16 |

## Status

`Approved`
