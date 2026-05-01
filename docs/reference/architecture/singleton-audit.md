# Singleton & Static Global Audit

**Date:** 2026-04-21  
**Scope:** Complex logical globals in the Dia engine and Cluiche platform  
**Goal:** Identify risk, inform future DI migration planning

---

## Why This Matters

Complex logical globals create:
- **Hidden coupling** — any code can silently depend on any global, making call graphs opaque
- **Init-order hazards** — C++ static-init ordering is translation-unit defined; globals that depend on each other can fail silently
- **Test isolation failures** — you cannot instantiate a system under test without every global it touches being initialized
- **Threading exposure** — global mutable state is shared by all threads; locks inside singletons protect access but not architectural reasoning

This document catalogues every significant singleton and static complex object in scope, assesses its risk profile, and describes a concrete dependency injection migration path for each.

**Out of scope:** Debug-only singletons (`DebugConsole`, `DebugDraw`, `DebugMemory`) — these are `#ifdef DEBUG` only and pose low production risk. Thread-local RNG in `DiaMaths` — `thread_local` is an acceptable pattern for stateless utilities.

---

## Summary Table

| System | File | Pattern | Risk | DI Migration Effort |
|--------|------|---------|------|---------------------|
| `Logger` | `Dia/DiaCore/Core/Logging/Logger.h:58` | Meyer's static | **High** | Medium |
| `g_pAssertFunc` | `Dia/DiaCore/Core/Assert.cpp:47` | Global function pointer | Medium | Low |
| ~~`GlobalEventDispatcher`~~ | ~~`Dia/DiaCore/Architecture/Events/EventDispatcher.h:228`~~ | ~~Meyer's static~~ | ~~**High**~~ | ~~High~~ | **Removed** |
| `TypeFacade` | `Dia/DiaCore/Type/TypeFacade.h:38` | Free function static | Medium | Medium |
| `ApplicationTypeRegistry` | `Dia/DiaApplication/TypeRegistry/ApplicationTypeRegistry.cpp:12` | Meyer's static | Medium | Low |
| `EditorPluginRegistry` | `Dia/DiaEditor/Plugin/EditorPluginRegistry.h:15` | Meyer's static | Low | Low |
| `AsyncFileLoader` | `Dia/DiaCore/FilePath/AsyncFileLoader.h:128` | Meyer's static | Medium | Medium |
| `JobSystem` | `Dia/DiaCore/Threading/JobSystem.h:115` | Meyer's static | Medium | Medium |
| `PathStore::sPathRootStore` | `Dia/DiaCore/FilePath/PathStore.h:60` | Static data member | ~~Medium~~ Low | Low | **RWLock + Lock() added** |
| `Win32Window::sLastCreated` | `Dia/DiaWindow/Win32Window.h:48` | Static instance pointer | Low | Low |

---

## Category A — Core Systems

These are the highest-priority items because they are used pervasively across the entire engine.

---

### 1. `Logger`

**File:** `Dia/DiaCore/Core/Logging/Logger.h:58`  
**Pattern:** Meyer's singleton (static local inside `GetInstance()`)  
**State:** `LogConfig`, 4 `std::unique_ptr<*LogOutput>`, `Mutex`  
**Callers:** 129 macro invocations (`DIA_LOG_*`) across 17 files — engine, input, Python integration, tests

**Risk Profile**

| Dimension | Assessment |
|-----------|-----------|
| Testability | Poor — any code that logs pulls in the global Logger; you cannot substitute a null/mock logger in unit tests |
| Init-order hazard | Medium — `Logger::GetInstance()` is first-use initialized; but `Assert.cpp` uses `Dia::Core::Log::Output` which also routes through global state, creating a chain |
| Thread safety | Good — all public methods lock `mMutex`; `GetInstance()` is safe under C++11 |

**DI Migration Path**

1. Extract interface: `ILogger` with `Log(level, namespace, format, ...)` and `LogWithLocation(...)`.
2. Introduce `EngineContext` struct (see Strategy section) with an `ILogger* logger` field.
3. Thread `EngineContext&` through `ProcessingUnit(...)` constructor; `Phase` and `Module` access it via `GetContext()`.
4. The `DIA_LOG_*` macros need a context parameter or a fallback — the pragmatic transition is to keep a lightweight compile-time redirect macro that uses the context if one is active, and falls back to the global for legacy call sites.
5. Remove `Logger::GetInstance()` last, once macro redirect is complete.

---

### 2. `g_pAssertFunc`

**File:** `Dia/DiaCore/Core/Assert.cpp:47`  
**Pattern:** Global function pointer initialized at file scope  
**State:** `DIA_ASSERT_FUNC g_pAssertFunc = AssertDefault`  
**Callers:** `DIA_ASSERT` macro — used throughout the entire engine

**Risk Profile**

| Dimension | Assessment |
|-----------|-----------|
| Testability | Moderate — you can override the function pointer in test setup; not as clean as injection but workable |
| Init-order hazard | Low — it is a plain function pointer assigned at file scope; no constructor logic |
| Thread safety | Poor — the pointer can be read on any thread while being written on another; no synchronization |

**DI Migration Path**

1. The assert function is a process-wide concern (assertions fire in any context). A full DI migration is low value here.
2. Acceptable improvement: replace the raw pointer with `std::atomic<DIA_ASSERT_FUNC>` to make concurrent reads and writes race-free.
3. For test override: expose `SetAssertHandler(DIA_ASSERT_FUNC)` / `ResetAssertHandler()` in a test-support header — this is already achievable today but should be formalized.

---

### 3. `GlobalEventDispatcher`

**File:** `Dia/DiaCore/Architecture/Events/EventDispatcher.h:228`  
**Pattern:** Static-utility class with Meyer's static `EventDispatcher` inside `Get()`  
**State:** `std::unordered_map` of handlers, `EventQueue`, `Mutex`  
**Callers:** Currently only `TestEventDispatcher.cpp` calls `GlobalEventDispatcher::Get()` directly; `EventDispatcher` instances are used locally in tests

**Risk Profile**

| Dimension | Assessment |
|-----------|-----------|
| Testability | Poor — global event bus means any test that dispatches an event can trigger handlers registered by other tests or subsystems |
| Init-order hazard | Medium — `EventDispatcher` has a non-trivial constructor (STL containers); safe under Meyer's semantics but destruction order is undefined relative to other globals |
| Thread safety | Good — `Dispatch()`, `Subscribe()`, `Unsubscribe()` all lock `mMutex` |

**DI Migration Path**

`EventDispatcher` is already designed as an instantiable class (line 47 of the header). `GlobalEventDispatcher` is just a thin wrapper that provides static access.

1. Remove `GlobalEventDispatcher` class.
2. Add `EventDispatcher eventDispatcher` as a value member of `EngineContext`.
3. `ProcessingUnit` already owns a `MessageBus` — the pattern of per-PU event routing is already understood. The global dispatcher should become one (optional) dispatcher at the application root, not a singleton.
4. Callers get access via `context.eventDispatcher` rather than `GlobalEventDispatcher::Get()`.

This is the highest-value migration item: the existing `EventDispatcher` class already supports it with zero interface changes.

---

### 4. `TypeFacade`

**File:** `Dia/DiaCore/Type/TypeFacade.h:38`  
**Pattern:** Free-standing function with Meyer's static (`GetTypeFacade()`)  
**State:** `TypeRegistry`, `TypeTextSerializer`, `TypeJsonSerializer`  
**Callers:** 12 files, primarily type-system internals and serialization tests

**Risk Profile**

| Dimension | Assessment |
|-----------|-----------|
| Testability | Poor — all type registration side-effects land in the same global registry; tests that register types pollute each other |
| Init-order hazard | Low — Meyer's static is safe, but `TypeFacade` contains `TypeRegistry` which likely has per-class `static TypeDefinition*` members registering at static-init time — those depend on `GetTypeFacade()` being alive |
| Thread safety | Unknown — no mutex observed in `TypeFacade.h`; if registration happens concurrently, this is unsafe |

**DI Migration Path**

1. `GetTypeFacade()` is a free function in a header — it will need to become a non-`static` non-free function.
2. Add `TypeFacade typeFacade` to `EngineContext`.
3. The harder problem is macro-based type registration (e.g., `DIA_REGISTER_TYPE`) which calls `GetTypeFacade()` at static init time before any context exists. These macros will need to be converted to deferred registration at engine startup (i.e., `RegisterAllTypes(TypeFacade&)` call in `EngineContext` constructor).
4. This migration is medium effort due to macro refactoring but is well-isolated — the type system is not threaded through `Phase`/`Module` constructors today.

---

## Category B — App Framework

---

### 5. `ApplicationTypeRegistry`

**File:** `Dia/DiaApplication/TypeRegistry/ApplicationTypeRegistry.cpp:12`  
**Pattern:** Meyer's static inside `Instance()`  
**State:** 3 `HashTable` maps (ProcessingUnit/Phase/Module factory pointers), 3 cached `DynamicArrayC` lists  
**Callers:** 10 files — registration macros, manifest loader, introspector, tests

**Risk Profile**

| Dimension | Assessment |
|-----------|-----------|
| Testability | Moderate — tests call `Instance()` directly and can register test types; factories are pointer-stable. Main issue is cross-test type pollution |
| Init-order hazard | Low — Meyer's static, first-use init; registration macros run at static init and call `Instance()` which is safe |
| Thread safety | Not analyzed — the `RegisterXxxType` methods have no mutex; concurrent registration from multiple translation units during static init is technically UB |

**DI Migration Path**

The `ApplicationTypeRegistry` is a pure type-catalogue — it has no mutable runtime state after startup. It is the easiest singleton to remove:

1. Instantiate `ApplicationTypeRegistry` as a value member of `EngineContext` (or an `ApplicationContext` that lives inside the application entry point).
2. Change registration macros from `ApplicationTypeRegistry::Instance().Register...` to accept a registry reference: `void RegisterAllTypes(ApplicationTypeRegistry& registry)` called once at startup.
3. `ApplicationManifestLoader` and `ApplicationIntrospector` receive the registry by reference/pointer in their constructors.

Low effort, high testability gain.

---

### 6. `EditorPluginRegistry`

**File:** `Dia/DiaEditor/Plugin/EditorPluginRegistry.h:15`  
**Pattern:** Meyer's static inside `Instance()`  
**State:** `DynamicArrayC<PluginEntry, 32>` — up to 32 plugin entries  
**Callers:** Editor plugin registration sites (not yet widely used)

**Risk Profile**

| Dimension | Assessment |
|-----------|-----------|
| Testability | Low risk — editor plugins are not active in game or unit test builds |
| Init-order hazard | Low — only populated at editor startup |
| Thread safety | Acceptable — editor is single-threaded |

**DI Migration Path**

Same pattern as `ApplicationTypeRegistry` — move to an `EditorContext` struct that owns the registry. Low priority given editor-only scope.

---

## Category C — Subsystems

---

### 7. `AsyncFileLoader`

**File:** `Dia/DiaCore/FilePath/AsyncFileLoader.h:128`  
**Pattern:** Meyer's static — `GetInstance()` is private; all public API is static  
**State:** `ThreadPool*`, `std::atomic<bool> mShutdown`, callback queue + `Mutex`  
**Callers:** 10 files; primarily `MainKernelModule.cpp` and file path utilities

**Risk Profile**

| Dimension | Assessment |
|-----------|-----------|
| Testability | Poor — async file operations in tests are tied to the same thread pool as production code |
| Init-order hazard | Medium — owns a `ThreadPool*` (heap-allocated in `Initialize()`); destruction order relative to other globals using it is unguarded |
| Thread safety | Good — callback queue is mutex-protected; `mShutdown` is atomic |

**DI Migration Path**

1. `AsyncFileLoader` is already designed as an instantiable class (constructor is private only to enforce singleton; can be made public).
2. Add `AsyncFileLoader fileLoader` to `EngineContext`; call `fileLoader.Initialize(numThreads)` in `EngineContext` constructor.
3. All current static call sites (`AsyncFileLoader::LoadAsync(...)`) become `context.fileLoader.LoadAsync(...)` — a mechanical rename.
4. `ProcessingUnit`/`Module` access file loading via `context.fileLoader` passed at construction.

---

### 8. `JobSystem`

**File:** `Dia/DiaCore/Threading/JobSystem.h:115`  
**Pattern:** Meyer's static — same shape as `AsyncFileLoader`  
**State:** `ThreadPool*`  
**Callers:** `JobSystem.h` itself (via `ParallelFor`), tests

**Risk Profile**

| Dimension | Assessment |
|-----------|-----------|
| Testability | Poor — `ParallelFor` uses `JobSystem::CreateJob` directly, coupling it to the global |
| Init-order hazard | Medium — `ThreadPool` is heap-allocated; must be initialized before use |
| Thread safety | Good — `ThreadPool` handles its own thread safety |

**DI Migration Path**

1. Make `JobSystem` instantiable (remove Meyer's static; constructor becomes public).
2. Add `JobSystem jobSystem` to `EngineContext`.
3. `ParallelFor` template function needs to accept a `JobSystem&` parameter or become a method — the latter is cleaner.
4. Medium effort due to `ParallelFor`'s widespread use as a free template function.

---

### 9. `PathStore::sPathRootStore`

**File:** `Dia/DiaCore/FilePath/PathStore.h:60`  
**Pattern:** Static data member (`static PathStoreMap sPathRootStore`)  
**State:** `std::map<Path::Alias, Path::String>` — path alias registry  
**Callers:** `FilePath.cpp`, `PathStoreConfig.h`, `PathStore.cpp`, `MainKernelModule.cpp`

**Risk Profile**

| Dimension | Assessment |
|-----------|-----------|
| Testability | Poor — tests that register path aliases mutate the global map, causing cross-test pollution |
| Init-order hazard | Low — it is a `std::map` static data member, default-constructed |
| Thread safety | Poor — no mutex on reads or writes; concurrent `RegisterToStore` and `ResolvePathToCString` is a data race |

**DI Migration Path**

1. `PathStore` is a static utility class — convert it to a value type: `class PathStore { PathStoreMap mStore; ... }`.
2. Add `PathStore pathStore` to `EngineContext`.
3. All callers that currently use `PathStore::RegisterToStore(...)` / `PathStore::ResolvePathToCString(...)` become `context.pathStore.RegisterToStore(...)`.
4. `FilePath` objects that currently call `PathStore` statically will need to receive a `PathStore*` or resolve at creation time. This is the main effort — `FilePath` is used widely.

---

### 10. `Win32Window::sLastCreated`

**File:** `Dia/DiaWindow/Win32Window.h:48`, `Win32Window.cpp:13`  
**Pattern:** Static class member pointer — set to `this` in constructor, used by `WndProc`  
**State:** `static Win32Window* sLastCreated` — raw pointer to last-created window  

**Risk Profile**

| Dimension | Assessment |
|-----------|-----------|
| Testability | Low risk — window creation is not unit-tested |
| Init-order hazard | None — pointer is null-initialized |
| Thread safety | Low risk — window lifecycle and message pump run on the same thread (Win32 requirement) |

**DI Migration Path**

This is a platform-specific callback routing hack: Win32's `WndProc` is a C-style callback and cannot carry `this`. Two clean alternatives:

1. **Register via `SetWindowLongPtr`:** Store the `Win32Window*` in the Win32 window's user data field at creation (`SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)this)`), then retrieve it in `WndProc` with `GetWindowLongPtr`. This eliminates the static entirely.
2. **Keep static, restrict to one window:** If the engine only ever creates one window (which is the current reality), document `sLastCreated` as an implementation detail of the single-window assumption and add an assert on second construction.

Option 1 is the correct long-term fix. Low effort.

---

## DI Migration Strategy

The recommended migration approach uses a single `EngineContext` struct as the composition root for all engine services.

```cpp
// Dia/DiaApplication/EngineContext.h (proposed)
struct EngineContext
{
    Logging::Logger           logger;
    Events::EventDispatcher   eventDispatcher;
    Types::TypeFacade         typeFacade;
    AsyncFileLoader           fileLoader;
    JobSystem                 jobSystem;
    PathStore                 pathStore;

    void Initialize();  // Calls Initialize() on subsystems that need it
    void Shutdown();    // Calls Shutdown() in reverse order (LIFO)
};
```

**Propagation through the application hierarchy:**

`ProcessingUnit` already accepts a `uniqueId` and optional config in its constructor. Adding `EngineContext&` is the natural next parameter. `Phase` and `Module` receive context via their owning `ProcessingUnit`'s `GetContext()` accessor.

```
main()
  └─ EngineContext ctx;           // Owns all services
       ctx.Initialize();
       └─ ProcessingUnit pu(id, ctx);   // Takes context ref
            └─ Phase::OnStart(ctx)
            └─ Module::Update(ctx)
```

**Registration macros:** `ApplicationTypeRegistry` and `EditorPluginRegistry` currently self-register via macros at static-init time. The cleanest transition is a two-phase approach:
1. Macros register into a lightweight `pending registrations` list (simple array, no global state beyond a POD list).
2. `EngineContext::Initialize()` drains the list into the real registry.

This keeps the ergonomic macro syntax while eliminating the singleton.

---

## Priority & Effort Matrix

```
                  Migration Effort
               Low         Medium       High
            ┌───────────┬───────────┬───────────┐
       High │ PathStore │  Logger   │EventDisp. │
  Risk      │           │           │           │
            ├───────────┼───────────┼───────────┤
     Medium │AppTypeReg │TypeFacade │           │
            │AsyncFile  │ JobSystem │           │
            │g_pAssert  │           │           │
            ├───────────┼───────────┼───────────┤
        Low │Win32Last  │           │           │
            │EditorPlug │           │           │
            └───────────┴───────────┴───────────┘
```

**Recommended migration order (quick wins first):**

1. `Win32Window::sLastCreated` — Use `SetWindowLongPtr`; self-contained, no API changes elsewhere.
2. `ApplicationTypeRegistry` — Introduce `EngineContext`, move registry in; update registration macros.
3. `PathStore` — Convert to value type; thread through `EngineContext`.
4. `g_pAssertFunc` — Make atomic; expose test override API.
5. `AsyncFileLoader` / `JobSystem` — Make instantiable; add to `EngineContext`.
6. `Logger` — Introduce `ILogger`, update `DIA_LOG_*` macros.
7. `GlobalEventDispatcher` — Remove static wrapper; `EventDispatcher` instance in `EngineContext`.
8. `TypeFacade` — Migrate type-registration macros to deferred pattern; add to `EngineContext`.

Items 1–3 can be done independently. Items 6–8 are best done together as they are interlinked through the logging/assert chain.
