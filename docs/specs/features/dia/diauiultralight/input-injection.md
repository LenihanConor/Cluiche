# Feature Spec: Input Injection

## Parent System
@docs/specs/systems/dia/diauiultralight.md

## Summary

Forward all five `IUISystem` mouse input methods to the Ultralight `View` via `FireMouseEvent` and `FireScrollEvent`, converting `Dia::Input::EMouseButton` to `ultralight::MouseEvent::Button`.

## Goals

- Implement `InjectMouseMove`, `InjectMouseDown`, `InjectMouseUp`, `InjectMouseClick`, `InjectMouseWheel`
- Correctly map all three Dia mouse buttons (Left, Middle, Right)
- `InjectMouseClick` performs Move + Down + Up as a sequence (matching Awesomium behaviour)
- Guard all methods against null `mView`

## Tasks

| # | Task | Implementation |
|---|---|---|
| 1 | Button mapping | `ToUltralightButton(EMouseButton)` → `kButton_Left / kButton_Middle / kButton_Right`; default `kButton_None` |
| 2 | MouseMove | `kType_MouseMoved`, `button = kButton_None`, `x/y` set |
| 3 | MouseDown | `kType_MouseDown`, mapped button, `x/y` set |
| 4 | MouseUp | `kType_MouseUp`, mapped button, `x/y` set |
| 5 | MouseClick | Calls `InjectMouseMove`, `InjectMouseDown`, `InjectMouseUp` in sequence |
| 6 | MouseWheel | `ScrollEvent { kType_ScrollByPixel, delta_x = scroll_horz, delta_y = scroll_vert }` |
| 7 | Null guard | `if (!mView) return;` at top of each method |

## Button Mapping Table

| `Dia::Input::EMouseButton` | `ultralight::MouseEvent::Button` |
|---|---|
| `kLeft` | `kButton_Left` |
| `kMiddle` | `kButton_Middle` |
| `kRight` | `kButton_Right` |
| *(other)* | `kButton_None` |

## Binding Decisions Compliance

| Decision | How this feature honours it |
|---|---|
| AD-002 No STL in public APIs | Input methods take `int` and `EMouseButton` — all Dia/primitive types |

## AI Review Questions

| # | Question | Answer |
|---|---|---|
| 1 | Why no keyboard injection? | `IUISystem` does not define keyboard methods; Awesomium had the same gap |
| 2 | Does `InjectMouseClick` need the position sent twice? | `InjectMouseMove` updates cursor position; `InjectMouseDown/Up` fire at current position — harmless redundancy |
| 3 | Thread safety? | All five methods are guarded by `mSystemMutex` in the outer `UISystem`; `FireMouseEvent` must be called on the Ultralight thread |
| 4 | What if `mView` is null? | Early return; prevents crash if `InjectMouse*` is called before `Initialize` |

## Status

`Approved`
