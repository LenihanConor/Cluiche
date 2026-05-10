# Feature Spec: Registration

## Parent System
@docs/specs/systems/dia/diaapplicationflow.md

## Traceability

| Level | Spec | Link |
|-------|------|------|
| Platform | Cluiche | @docs/specs/platform/Cluiche.md |
| Application | Dia | @docs/specs/applications/dia.md |
| System | DiaApplicationFlow | @docs/specs/systems/dia/diaapplicationflow.md |
| Feature | Registration | (this file) |

## Problem Statement

The framework must instantiate modules by type ID from manifest config. Modules need a one-liner registration mechanism that maps a StringCRC type ID to a C++ class, replacing the current ~40-line boilerplate per module. ModuleRef<T> needs a single consistent access pattern.

## Acceptance Criteria

1. `DIA_MODULE(ClassName)` macro registers a module class with its `kTypeId`
2. Each module class declares `static constexpr StringCRC kTypeId{"ModuleName"}`
3. `TypeRegistry` stores factory functions keyed by StringCRC
4. Framework instantiates modules from manifest `type_id` field via TypeRegistry lookup
5. `ModuleRef<T>` is the sole inter-module access pattern
6. ModuleRef resolves lazily (first access) by querying the owning PU's module list
7. ModuleRef requires owning module pointer and optional instance ID override
8. If target module is not found or not Active, ModuleRef returns nullptr / evaluates false
9. No other module access patterns exist (no GetModule<T>, no FindModule by CRC in user code)
10. TypeRegistry is populated before Application::Start() — registration order does not matter

## Public API

```cpp
namespace Dia::ApplicationFlow {

    // Type registry — maps StringCRC → factory function
    class TypeRegistry {
    public:
        using FactoryFn = Module* (*)(const StringCRC& instanceId);

        void Register(const StringCRC& typeId, FactoryFn factory);
        Module* Create(const StringCRC& typeId, const StringCRC& instanceId) const;
        bool Contains(const StringCRC& typeId) const;
    };

    // Static registration helper — constructed at static init time
    template<typename T>
    struct ModuleRegistration {
        explicit ModuleRegistration(const StringCRC& typeId);
    };

    // One-liner macro
    #define DIA_MODULE(ClassName) \
        static Dia::ApplicationFlow::ModuleRegistration<ClassName> \
            s_reg_##ClassName{ClassName::kTypeId}

    // Typed module reference — sole access pattern
    template<typename T>
    class ModuleRef {
    public:
        ModuleRef(Module* owner, const char* instanceId = T::kTypeId.GetString());

        T* Get();
        T* operator->();
        const T* operator->() const;
        explicit operator bool() const;

    private:
        Module* mOwner;
        StringCRC mTargetId;
        T* mCached = nullptr;
    };
}
```

## Registration Flow

```
1. Static init: DIA_MODULE(MyModule) constructs ModuleRegistration<MyModule>
2. ModuleRegistration constructor calls TypeRegistry::Register(kTypeId, &factory)
   where factory = [](const StringCRC& id) -> Module* { return new MyModule(id); }
3. Application construction receives TypeRegistry reference
4. Application::Start() reads manifest, calls TypeRegistry::Create() for each module entry
5. Created modules are assigned to their ProcessingUnit
```

## ModuleRef Resolution

```
1. ModuleRef<T> constructed in module's constructor or member initializer
2. First call to Get()/operator->():
   a. Query owner->GetProcessingUnit()->FindModule(mTargetId)
   b. static_cast<T*>(result) — type safety guaranteed by registration
   c. Cache pointer in mCached
3. Subsequent calls return mCached directly
4. If target module is Inactive/Failed/not found: returns nullptr
5. ModuleRef is intra-PU only — cross-PU access uses streams
```

## Module Declaration Pattern

```cpp
// MyModule.h
class MyModule : public Dia::ApplicationFlow::Module {
public:
    static constexpr Dia::Core::StringCRC kTypeId{"MyModule"};

    explicit MyModule(const StringCRC& instanceId);

protected:
    StartResult DoStart() override;
    void DoUpdate(float deltaTime) override;
    StopResult DoStop() override;

private:
    ModuleRef<TimeServerModule> mTimeServer{this};
    ModuleRef<InputStreamModule> mInput{this};
};

// MyModule.cpp
DIA_MODULE(MyModule);

MyModule::MyModule(const StringCRC& instanceId)
    : Module(instanceId) {}
```

## Files Touched

| File | Action |
|------|--------|
| `Dia/DiaApplicationFlow/TypeRegistry.h` | New (replaces v1 ApplicationTypeRegistry.h) |
| `Dia/DiaApplicationFlow/TypeRegistry.cpp` | New |
| `Dia/DiaApplicationFlow/ModuleRef.h` | Rewrite (v2 API — lazy resolution, owner-based) |
| `Dia/DiaApplicationFlow/RegistrationMacros.h` | Rewrite (one-liner DIA_MODULE) |
| `Dia/DiaApplicationFlow/TypeRegistry/ApplicationTypeRegistry.h` | Delete (v1) |
| `Dia/DiaApplicationFlow/TypeRegistry/ApplicationTypeRegistry.cpp` | Delete (v1) |
| `Dia/DiaApplicationFlow/TypeRegistry/RegistrationMacros.h` | Delete (v1) |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj` | Update |
| `Dia/DiaApplicationFlow/DiaApplicationFlow.vcxproj.filters` | Update |

## Dependencies

- **Module Lifecycle** (this system) — Module base class
- **DiaCore/CRC/StringCRC** — type and instance IDs
- **DiaCore/Containers/HashTableC** — TypeRegistry internal storage

## Binding Decisions Compliance

| ID | Source | Decision | Compliance |
|----|--------|----------|------------|
| PD-001 | Platform | StringCRC for all IDs | Compliant — typeId and instanceId are StringCRC |
| PD-004 | Platform | No STL in public APIs | Compliant — HashTableC internally, no std:: exposed |
| PD-007 | Platform | C++20 required | Compliant — constexpr StringCRC, concepts could guard ModuleRegistration (T must derive Module) |
| AD-003 | Dia App | Namespace Dia::\<Module\>:: | Compliant — Dia::ApplicationFlow:: |
| SD-001 | DiaAppFlow | Config is sole source of truth | Compliant — manifest's type_id field drives instantiation |
| SD-003 | DiaAppFlow | ModuleRef<T> is sole access pattern | Compliant — this feature defines and enforces it |
| SD-008 | DiaAppFlow | One-liner DIA_MODULE macro | Compliant — this feature implements it |
| SD-015 | DiaAppFlow | No shared modules across PUs | Compliant — ModuleRef resolves within owning PU only |
| SD-017 | DiaAppFlow | Clean break | Compliant — v1 TypeRegistry deleted, fresh implementation |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Static init | Is static initialization order a problem (TypeRegistry must exist before registrations)? | TypeRegistry is a simple container. Use a singleton-on-first-use pattern (Meyers singleton) for the global instance. Registration constructors call into it safely. |
| 2 | ModuleRef | Should ModuleRef assert or silently return nullptr when target not found? | Silently return nullptr. Module may query before target is started. User checks with `if (mRef)` before use. DIA_ASSERT on dereference of null (operator->). |
| 3 | ModuleRef | Should ModuleRef invalidate cache when target module stops? | No. ModuleRef caches once. If target stops, caller gets a pointer to a Stopped module — state check is caller's responsibility via `GetState()`. In practice, stopped modules are only accessed during transitions which are framework-managed. |
| 4 | Macro | Should DIA_MODULE work in a .cpp file only, or also in headers? | .cpp only. Static variable in a .cpp avoids ODR violations. Document this requirement. |
| 5 | TypeRegistry | Should TypeRegistry be global (singleton) or passed around? | Passed to Application constructor. Tests can provide isolated registries. The global singleton is a convenience that populates from DIA_MODULE macros — Application constructor takes a reference to it. |
| 6 | Concepts | Should we use a C++20 concept to constrain T in ModuleRegistration? | Yes. `concept DerivedModule = std::derived_from<T, Module> && requires { T::kTypeId; }`. Catches errors at registration, not runtime. |

## Open Questions

None.

## Status

`Approved` — 2026-05-09
