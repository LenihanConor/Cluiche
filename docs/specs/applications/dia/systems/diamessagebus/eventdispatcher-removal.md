# Feature Spec: EventDispatcher Removal

## Parent System
@docs/specs/applications/dia/systems/diamessagebus/diamessagebus.md

## Problem Statement

`Dia::Core::Events::EventDispatcher`, `EventQueue`, and `Delegate` are legacy messaging primitives with STL-heavy internals (`std::unordered_map`, `std::vector`, raw pointer ownership) that violate PD-004 and sit entirely outside the DiaMessageBus routing model. Their only non-test callers in the codebase are `InputSourceManager::UpdateModern` and `LegacyEventConverter`, which exist solely to bridge legacy input events into this dead-end system. Removing them eliminates a stranded parallel messaging layer, cuts DiaInput's STL surface, and opens a clean seam on `InputSourceManager` for the `InputBusAdapter` that the `flush-adapters` feature will fill.

## Acceptance Criteria

| # | Criterion | Verification |
|---|-----------|--------------|
| AC-1 | `Dia/DiaCore/Architecture/Events/EventDispatcher.h`, `EventQueue.h`, and `Delegate.h` are deleted from the repository | `git status` shows all three files removed |
| AC-2 | `Dia/DiaInput/Events/LegacyEventConverter.h` is deleted from the repository | `git status` shows the file removed |
| AC-3 | `InputSourceManager::UpdateModern(Core::Events::EventDispatcher&)` declaration and the EventDispatcher forward declaration are removed from `InputSourceManager.h` | `grep -r "UpdateModern" Dia/DiaInput` returns no results |
| AC-4 | `InputSourceManager::UpdateModern` implementation body is removed from `InputSourceManager.cpp` | `grep -r "UpdateModern" Dia/DiaInput` returns no results |
| AC-5 | `grep -r "EventDispatcher\|EventQueue\|LegacyEventConverter" Dia/ --include="*.h" --include="*.cpp"` returns zero matches across the entire `Dia/` tree | Run post-deletion |
| AC-6 | `grep -r "#include.*Architecture/Events/(EventDispatcher\|EventQueue\|Delegate)" .` over the full repository returns zero matches | Run post-deletion |
| AC-7 | `DiaCore.vcxproj` and `DiaCore.vcxproj.filters` no longer contain entries for `EventDispatcher.h`, `EventQueue.h`, or `Delegate.h` | Inspect both vcxproj files; `dia docs precommit` passes |
| AC-8 | `DiaInput.vcxproj` no longer contains an entry for `LegacyEventConverter.h` | Inspect vcxproj; `dia docs precommit` passes |
| AC-9 | Four dead test files (`TestEventDispatcher.cpp`, `TestEventQueue.cpp`, `TestLegacyEventConverter.cpp`, `TestModernEvents.cpp`) are deleted and removed from `GoogleTests.vcxproj` | `git status` shows all four removed; `dia docs precommit` passes |
| AC-10 | `DiaInput` compiles without errors in `Debug\|x64` after removal — `InputSourceManager::Update(EventData&)` is preserved intact; the `UpdateModern` slot is vacant, ready for `InputBusAdapter` (delivered by `flush-adapters`) | `dia run googletest --filter="Input*"` reports zero compile errors |
| AC-11 | Full GoogleTest suite passes green after all deletions | `dia run googletest` exits with no failures; pass/fail line quoted |
| AC-12 | `ChatPanelBridge.h` and `ChatPanelBridge.cpp` are NOT modified — `ChatPanelBridge::mEventQueue` is `std::queue<ChatEvent>` (a standard library queue unrelated to DiaCore's `EventQueue`) | `git diff` shows no changes to either ChatPanelBridge file |

## Design

### Confirmed Blast Radius

Grepped at spec-authoring time against `Dia/DiaCore` and `Dia/DiaInput`. All affected files listed below; nothing else in the engine or application layer includes the removed headers.

**DiaCore — delete 3 header files (header-only; no matching .cpp):**

- `Dia/DiaCore/Architecture/Events/EventDispatcher.h` — the dispatcher itself: `std::unordered_map<EventTypeID, std::unordered_map<HandlerID, EventHandler>>` internals, raw-pointer `QueueEvent(Event*)` ownership, and a `Mutex` member. STL-heavy, violates PD-004. No non-test callers outside DiaInput.
- `Dia/DiaCore/Architecture/Events/EventQueue.h` — owned internally by `EventDispatcher`. No independent callers.
- `Dia/DiaCore/Architecture/Events/Delegate.h` — included by `EventDispatcher`. No independent callers.

`Dia/DiaCore/Architecture/Events/Event.h` is **not** deleted. It remains as the base event class and is still transitively used by `DiaInput/Events/KeyboardEvents.h`, `MouseEvents.h`, `JoystickEvents.h`, and `GamepadEvents.h`. Retiring those typed structs is a concern for the `flush-adapters` feature, not this one.

**DiaInput — delete 1 header, modify 2 files:**

- Delete: `Dia/DiaInput/Events/LegacyEventConverter.h` — the entire class is a bridge: `ConvertAndDispatch(const EventData&, Core::Events::EventDispatcher&)` converts legacy union events to heap-allocated `Core::Events::Event*` objects and queues them into the dispatcher. With the dispatcher gone, the class has no purpose.
- Modify: `Dia/DiaInput/InputSourceManager.h` — remove the `void UpdateModern(Core::Events::EventDispatcher& dispatcher)` method declaration and the forward declaration `namespace Dia { namespace Core { namespace Events { class EventDispatcher; } } }` from the top of the file.
- Modify: `Dia/DiaInput/InputSourceManager.cpp` — remove the `UpdateModern` implementation. The legacy `Update(EventData& outStream)` path is preserved intact.

**Dead test files — delete 4 files, update GoogleTests.vcxproj:**

- `Cluiche/Tests/GoogleTests/Core/Events/TestEventDispatcher.cpp`
- `Cluiche/Tests/GoogleTests/Core/Events/TestEventQueue.cpp`
- `Cluiche/Tests/GoogleTests/Input/TestLegacyEventConverter.cpp`
- `Cluiche/Tests/GoogleTests/Input/TestModernEvents.cpp`

**vcxproj/filters updates required (3 projects):**

- `DiaCore.vcxproj` + `DiaCore.vcxproj.filters` — remove the 3 `EventDispatcher.h`/`EventQueue.h`/`Delegate.h` `<ClInclude>` entries
- `DiaInput.vcxproj` — remove the `LegacyEventConverter.h` `<ClInclude>` entry
- `GoogleTests.vcxproj` — remove the 4 dead test file `<ClCompile>` entries

### False Positive: ChatPanelBridge

`CluicheEditor/Plugins/DiaChatPlugin/ChatPanelBridge.h` declares `std::queue<ChatEvent> mEventQueue`. This is a standard library `std::queue` — it has no relationship to `Dia::Core::Events::EventQueue`. A grep for `mEventQueue` will match this file; it must not be touched (AC-12).

### Seam for flush-adapters

This feature deliberately does not implement a replacement for `UpdateModern`. It removes the EventDispatcher coupling and leaves `InputSourceManager` with only its legacy `Update(EventData&)` interface. The `flush-adapters` feature delivers `InputBusAdapter` — an `IFlushAdapter` that reads the input `EventData` stream and posts typed bus messages. These two features are decoupled at the compile boundary: `DiaInput` does not need to know about `DiaMessageBus` after this removal, and `InputBusAdapter` (owned by `flush-adapters`) is the adapter that reaches across.

### Ordering Constraints

- **Soft dependency on `core-bus`:** Because `InputSourceManager` retains its `Update(EventData&)` path, `DiaInput` compiles independently of `DiaMessageBus` both before and after this feature. There is no compile-time dependency on `DiaMessageBus` in this feature. The `core-bus` prerequisite arises for the bus-connected code path in `flush-adapters`, not here.
- **Coordinate with `flush-adapters`:** Both features modify `DiaInput.vcxproj` and `InputSourceManager`. Serial ordering is simpler — complete `eventdispatcher-removal` first, then `flush-adapters` adds `InputBusAdapter` into the vacated slot.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Delete `EventDispatcher.h`, `EventQueue.h`, `Delegate.h` from `Dia/DiaCore/Architecture/Events/`; update `DiaCore.vcxproj` and `DiaCore.vcxproj.filters` to remove all three entries | AC-1, AC-7 | Not Started | haiku | Header-only; no .cpp counterparts. Manual vcxproj edit — `dia docs vcxproj-add` does not have a remove path; edit by hand and verify with `dia docs precommit` |
| 2 | Delete `LegacyEventConverter.h` from `Dia/DiaInput/Events/`; update `DiaInput.vcxproj` to remove the `<ClInclude>` entry | AC-2, AC-8 | Not Started | haiku | Header-only |
| 3 | Remove `UpdateModern` declaration and EventDispatcher forward declaration from `InputSourceManager.h`; remove `UpdateModern` implementation from `InputSourceManager.cpp` | AC-3, AC-4 | Not Started | haiku | Preserve `Update(EventData&)` path unchanged |
| 4 | Delete 4 dead test files (`TestEventDispatcher.cpp`, `TestEventQueue.cpp`, `TestLegacyEventConverter.cpp`, `TestModernEvents.cpp`); remove their `<ClCompile>` entries from `GoogleTests.vcxproj` | AC-9 | Not Started | haiku | |
| 5 | Grep-verify zero remaining includes of deleted headers across entire repo | AC-5, AC-6 | Not Started | haiku | `grep -r "EventDispatcher\|EventQueue\|LegacyEventConverter" Dia/` and `grep -r "#include.*Architecture/Events/(EventDispatcher\|EventQueue\|Delegate)" .` |
| 6 | Build `Debug\|x64` and run full GoogleTest suite | AC-10, AC-11 | Not Started | sonnet | `dia run googletest`; quote pass/fail line in task notes |
| 7 | Confirm ChatPanelBridge files are unchanged in `git diff` | AC-12 | Not Started | haiku | One-liner: `git diff -- "*ChatPanelBridge*"` should be empty |

## Status

`Approved`
