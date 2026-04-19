# Feature Spec: JavaScript Binding

## Parent System
@docs/specs/systems/dia/diauiultralight.md

## Summary

Expose C++ `BoundMethod` callbacks to JavaScript pages via an `app` global object, matching the Awesomium `CreateGlobalJavascriptObject("app")` pattern. Handles both void methods and methods with return values. Converts `JSArgs` ↔ `BoundMethodArgs` for all supported types (bool, number, string).

## Goals

- Create an `app` JavaScript global object in every loaded page
- Register each `BoundMethod` as a property on `app` using `JSCallbackWithRetval`
- Convert incoming `JSArgs` to `BoundMethodArgs` (bool, double, string)
- Invoke the C++ delegate and convert the return value back to `JSValue` where applicable
- Register bindings during `OnDOMReady` (DOM ready, JS engine available)

## Tasks

| # | Task | Implementation |
|---|---|---|
| 1 | Lock JS context | `caller->LockJSContext()` → `SetJSContext(ctx->ctx())` |
| 2 | Get global | `JSGlobalObject()` |
| 3 | Create `app` object | `JSObject appObj; global["app"] = appObj;` |
| 4 | For each pending binding | Set `appObj[name] = JSCallbackWithRetval(lambda)` |
| 5 | Lambda — convert args | Iterate `args`; check `IsBoolean`, `IsNumber`, `IsString`; push to `BoundMethodArgs` |
| 6 | Lambda — call C++ | Call `GetMethodPtr()(diaArgs)` or `GetMethodReturnPtr()(diaArgs)` based on `ReturnValueFlag` |
| 7 | Lambda — convert return | Map `BoundMethodValue::EType` → `JSValue` (bool/int/double/string); return null for void |

## Type Mapping

| JS type | `BoundMethodValue` type | Notes |
|---|---|---|
| Boolean | `kBoolean` | `v.ToBoolean()` |
| Number | `kDouble` | `v.ToNumber()` (JS has no int/double distinction) |
| String | `kString` | `v.ToString()` → `utf8().data()` → `String64` |
| Return bool | JSValue(bool) | |
| Return int | JSValue(int32_t) | |
| Return double | JSValue(double) | |
| Return string | JSValue(const char*) | |

## Binding Decisions Compliance

| Decision | How this feature honours it |
|---|---|
| AD-002 No STL in public APIs | `std::vector<PendingBinding>` is internal to `UISystemImpl`; public `BoundMethodArgs` is Dia type |
| UL-003 OnDOMReady | Bindings registered in `OnDOMReady` callback only |

## AI Review Questions

| # | Question | Answer |
|---|---|---|
| 1 | Why `JSCallbackWithRetval` for void methods too? | Simplest uniform approach; void methods just return `JSValue()` (undefined) |
| 2 | What if `args.size()` exceeds `BoundMethodArgs::kMaxArgs` (8)? | Currently no bounds check; JS callers control argument count; can add assert if needed |
| 3 | What if JS passes a type not handled (array, object)? | Silently skipped; argument not added to `BoundMethodArgs` |
| 4 | Is the JS context lock held during the callback? | Yes — `LockJSContext` holds the lock for its lifetime; callbacks execute within that scope |
| 5 | Are bindings re-registered on page navigation? | Yes — `mPendingBindings` is rebuilt on each `LoadPage` call; `OnDOMReady` fires again |

## Status

`Approved`
