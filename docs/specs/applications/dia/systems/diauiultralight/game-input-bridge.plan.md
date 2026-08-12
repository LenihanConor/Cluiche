**Spec:** @docs/specs/applications/dia/systems/diauiultralight/game-input-bridge.md
**Status:** Done

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| 1 | Implement `CallJSFunction` in `UltralightUISystem` | Stage | Done | sonnet | `LockJSContext` + `EvaluateScript("fnName(argsJson)")` with null-guard on `mView` |
| 2 | Add keyboard API to `IUISystem` + no-op defaults | Compile | Done | haiku | `InjectKeyDown`, `InjectKeyUp`, `InjectCharacterInput` with empty default bodies |
| 3 | Implement keyboard injection in `UltralightUISystem` | Stage | Done | sonnet | `EKey`→GK_* mapping via range-based if-else (avoids duplicate Unknown/B case); `FireKeyEvent kType_RawKeyDown/KeyUp/Char` |
| 4 | `InputRouter` + `EInputRouting` in `DiaInput` | Stage | Done | haiku | Header-only `InputRouter.h` + `EKeyModifiers.h`; `DynamicArrayC<EInputRouting,8>` + `std::mutex`; added to `DiaInput.vcxproj` |
| 5 | `SetInputRouter` on `IUISystem`; wire in `UltralightUISystem`; auto-bind JS methods in `OnDOMReady` | Stage | Done | sonnet | `UISystemImpl` stores `InputRouter*`; `app.PushInputMode`/`PopInputMode` auto-bound; `::ultralight::String` intermediate needed for `JSValue::ToString()` (JSString has no `.utf8()`) |
| 6 | `UIModule` owns `InputRouter`; injects into `IUISystem`; gates keyboard routing on mode | Stage | Done | sonnet | `mInputRouter` member; `SetInputRouter(&mInputRouter)` in `DoStart`; `DoUpdate` skips game forward on `kUIOnly`, skips UI inject on `kGameOnly` |
| 7 | End-to-end test in `UIUltralightTestStage` | Stage checkpoints | Done | sonnet | 10 total checkpoints (was 6); 4 bridge: `call_js_observed`, `key_event_handled`, `mode_stack_transitions`, `keyboard_suppressed_in_ui_only`; all pass |
