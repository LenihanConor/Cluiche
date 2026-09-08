# Feature Spec: Game Input Bridge

## Parent System
@docs/specs/applications/dia/systems/diauiultralight/diauiultralight.md

## Summary

Adds three tightly-coupled capabilities to `DiaUIUltralight` required by in-game Ultralight panels (DiaDebugPanel, DiaChatPlugin): C++→JS function push (`CallJSFunction`), keyboard event injection, and an input-mode routing stack that safely arbitrates keyboard between panel and game.

## Goals

- Implement `IUISystem::CallJSFunction` in `UltralightUISystem` so C++ can push data to the loaded page's JS at runtime
- Add `InjectKeyDown` / `InjectKeyUp` / `InjectCharacterInput` to `IUISystem` and implement in `UltralightUISystem`; CEF and any future backends no-op
- Introduce `Dia::Input::InputRouter` (new, in `DiaInput`) with an `EInputRouting` stack (`PushInputMode` / `PopInputMode` / `GetCurrentInputMode`); owned by `UIModule`, injected into `UltralightUISystem`; JS can push/pop modes via auto-bound `app` methods
- Gate `UIModule` keyboard routing on the current mode so keyboard never leaks to the game while a panel text field is focused

## Acceptance Criteria

1. `UltralightUISystem::CallJSFunction(const char* fnName, const char* argsJson)` evaluates `fnName(argsJson)` in the loaded page's JS context; no-op if no page is loaded.
2. `IUISystem` gains `InjectKeyDown(EKey, EKeyModifiers)`, `InjectKeyUp(EKey, EKeyModifiers)`, and `InjectCharacterInput(uint32_t codepoint)`; all three are pure virtual with a no-op default (so CEF and future backends compile without change).
3. `UltralightUISystem` implements all three via `FireKeyEvent`; `Dia::Input::EKey` is mapped to Ultralight virtual key codes via a static mapping table.
4. `Dia::Input::InputRouter` (new in `DiaInput`) provides `EInputRouting` (`kGameOnly`, `kUIOnly`, `kGameAndUI`) and `PushInputMode` / `PopInputMode` / `GetCurrentInputMode`; owned by `UIModule`, injected into `UltralightUISystem` via `IUISystem::SetInputRouter`.
5. `UltralightUISystem` auto-registers `app.PushInputMode(modeStr)` and `app.PopInputMode()` JS bound-methods in `OnDOMReady`; these delegate to the injected `InputRouter`; mode strings are `"game_only"`, `"ui_only"`, `"game_and_ui"`.
6. `UIModule::DoUpdate` skips forwarding keyboard events to the game whenever `GetCurrentInputMode()` is `kUIOnly`.
7. `GetCurrentInputMode() == kGameAndUI` forwards keyboard to both panel and game (panels with sliders/toggles that coexist with camera controls).
8. The input mode stack is thread-safe: `GetCurrentInputMode()` is readable from any PU; mutations (`Push`/`Pop`) are guarded.
9. All four capabilities verified end-to-end in `UIUltralightTestStageModule`: (a) JS function is called from C++ and observed in a JS counter; (b) a key event is injected and handled by JS; (c) mode transitions are exercised; (d) `UIModule` keyboard suppression is confirmed.

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Implement `CallJSFunction` in `UltralightUISystem` | GoogleTest + stage | Draft | sonnet | Evaluate `fnName(argsJson)` via `JSEvaluateScript` inside a locked JS context; null-guard on `mView`; document that argsJson must be valid JS (caller responsible for escaping) |
| 2 | Add keyboard API to `IUISystem` + no-op defaults | Compile check | Draft | haiku | `InjectKeyDown(EKey, EKeyModifiers)`, `InjectKeyUp(EKey, EKeyModifiers)`, `InjectCharacterInput(uint32_t)`; default empty body so existing backends don't break |
| 3 | Implement keyboard injection in `UltralightUISystem` | GoogleTest | Draft | sonnet | Map `EKey` → Ultralight virtual key codes (static lookup table); `FireKeyEvent(kType_KeyDown/Up/Char)` on `mView`; null-guard |
| 4 | `InputRouter` + `EInputRouting` in `DiaInput` | GoogleTest | Draft | haiku | New `Dia/DiaInput/InputRouter.h`; `DynamicArrayC<EInputRouting,8>` stack; `std::mutex` guards; returns `kGameOnly` when empty; add to `DiaInput.vcxproj` |
| 5 | `SetInputRouter` on `IUISystem`; wire in `UltralightUISystem`; auto-bind JS methods in `OnDOMReady` | GoogleTest | Draft | sonnet | `UltralightUISystem` stores `InputRouter*`; registers `app.PushInputMode` / `app.PopInputMode`; mode string → enum; unknown string = no-op; both bindings registered only when `mInputRouter != nullptr` |
| 6 | `UIModule` owns `InputRouter`; injects into `IUISystem`; gates keyboard routing on mode | Stage test | Draft | sonnet | Construct `InputRouter` in `UIModule::DoStart`; call `SetInputRouter`; in `DoUpdate`: skip game keyboard forward when `kUIOnly`; skip UI inject when `kGameOnly`; forward to both on `kGameAndUI` |
| 7 | End-to-end test in `UIUltralightTestStage` | Stage checkpoints | Draft | sonnet | 4 checkpoints: `call_js_observed`, `key_event_handled`, `mode_stack_transitions`, `keyboard_suppressed_in_ui_only` |

## Public Interface Changes

### New: `Dia/DiaInput/InputRouter.h`

The routing stack lives in `DiaInput` so any system (UI panel, cutscene, pause menu) can claim keyboard without coupling to `IUISystem`.

```cpp
namespace Dia::Input {

    enum class EInputRouting { kGameOnly, kUIOnly, kGameAndUI };

    // Arbitrates keyboard routing between game and in-game UI panels.
    // Owned by UIModule; injected into UltralightUISystem via SetInputRouter().
    // Stack default (empty) = kGameOnly.
    class InputRouter {
    public:
        void          PushInputMode(EInputRouting mode);
        void          PopInputMode();
        EInputRouting GetCurrentInputMode() const; // thread-safe read

    private:
        Dia::Core::Containers::DynamicArrayC<EInputRouting, 8> mStack;
        mutable std::mutex mMutex;
    };

} // namespace Dia::Input
```

### `IUISystem.h` additions

```cpp
// ── Keyboard injection ──────────────────────────────────────────
// Default no-op: CEF handles keyboard natively; no ImGui IUISystem impl exists.
virtual void InjectKeyDown(Dia::Input::EKey key, int modifiers = 0) {}
virtual void InjectKeyUp(Dia::Input::EKey key, int modifiers = 0)   {}
virtual void InjectCharacterInput(uint32_t codepoint)               {}

// ── Input router wiring ─────────────────────────────────────────
// UIModule calls this after constructing both objects.
// UltralightUISystem uses it for JS auto-bindings (app.PushInputMode / PopInputMode).
virtual void SetInputRouter(Dia::Input::InputRouter* router) {}
```

`CallJSFunction` is already declared on `IUISystem` as a virtual no-op (Task 1 is pure implementation).

### New: `Dia/DiaInput/EKeyModifiers.h`

```cpp
namespace Dia::Input {
    enum EKeyModifiers : int {
        kModNone    = 0,
        kModShift   = 1 << 0,
        kModControl = 1 << 1,
        kModAlt     = 1 << 2,
        kModSystem  = 1 << 3,
    };
}
```

### JS auto-bindings (UltralightUISystem registers in OnDOMReady via injected InputRouter)

| JS call | C++ effect |
|---------|-----------|
| `app.PushInputMode("ui_only")` | `mInputRouter->PushInputMode(kUIOnly)` |
| `app.PushInputMode("game_only")` | `mInputRouter->PushInputMode(kGameOnly)` |
| `app.PushInputMode("game_and_ui")` | `mInputRouter->PushInputMode(kGameAndUI)` |
| `app.PopInputMode()` | `mInputRouter->PopInputMode()` |

Panel HTML wires these to `onfocus`/`onblur` on text inputs:
```html
<input onfocus="app.PushInputMode('ui_only')"
       onblur="app.PopInputMode()">
```

If no `InputRouter` is set (i.e. `SetInputRouter` was never called), the JS bindings are registered as no-ops.

## Files Touched

| File | Change |
|------|--------|
| `Dia/DiaInput/InputRouter.h` | New — `EInputRouting` enum + `InputRouter` class |
| `Dia/DiaInput/EKeyModifiers.h` | New — `EKeyModifiers` flags |
| `Dia/DiaInput/DiaInput.vcxproj` | Add new headers |
| `Dia/DiaUI/IUISystem.h` | Add `InjectKey*` (no-op defaults), `SetInputRouter` |
| `Dia/DiaUIUltralight/UltralightUISystem.h/.cpp` | Implement `CallJSFunction`, `InjectKey*`, `SetInputRouter` + `OnDOMReady` JS bindings |
| `Cluiche/CluicheGameBaseline/Modules/UIModule.h/.cpp` | Own `InputRouter`; call `SetInputRouter`; gate keyboard forwarding on `GetCurrentInputMode()` |
| `Cluiche/CluicheTest/Modules/TestStages/UIUltralightTestStageModule.cpp` | 4 new test checkpoints |
| `Cluiche/CluicheTest/Modules/TestStages/UIUltralight/UIUltralightTestPage.cpp` | JS handlers for test checkpoints |

## Binding Decisions

| Decision | How this feature complies |
|----------|--------------------------|
| PD-004 / AD-002 No STL in public APIs | `InjectKey*` takes `EKey` (Dia enum) and `int`; `InjectCharacterInput` takes `uint32_t`; `EInputRouting` is a scoped enum. Mode stack methods are non-virtual with internal mutex. `JSHandler` using `std::function` is a pre-existing IUISystem violation, not introduced here. |
| PD-002 PU/Module architecture | `InputRouter` is owned by `UIModule`; `UIModule` reads `GetCurrentInputMode()` on its own PU thread. Mutations from JS arrive on the Ultralight update thread — guarded by `std::mutex` in `InputRouter` (Task 4). |
| PD-007 C++20 | Scoped enum (`enum class EInputRouting`) and `[[nodiscard]]` on `GetCurrentInputMode` are C++20-compatible. |

## Open Design Questions

~~1. **EKey → Ultralight virtual key code mapping completeness** — Resolved: unmapped keys map to `0` (silent no-op in Release; `DIA_ASSERT` in Debug so gaps surface during development).~~

~~2. **Mode stack placement** — Resolved: `DiaInput` layer. `InputRouter` lives in `Dia/DiaInput/`; `UIModule` owns the instance and injects it into `UltralightUISystem` via `SetInputRouter`.~~

~~3. **`CallJSFunction` JS context thread** — Resolved: option (b). `VisualDebuggerModule` writes JSON state to a stream; `UIModule` reads it on its update thread and calls `CallJSFunction`. Matches existing `SimToRender` stream architecture.~~

## Status

**Status:** `Done`
