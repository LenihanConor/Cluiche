# Feature Spec: Module API

## Traceability

| Level | Spec | Link |
|-------|------|------|
| **Feature** | module-api | (this document) |
| **System** | DiaPython | @docs/specs/systems/dia/diapython.md |
| **Application** | Dia | @docs/specs/applications/dia.md |
| **Platform** | Cluiche | @docs/specs/platform/Cluiche.md |

## Problem Statement

Provides a clean C++ API for Dia systems (like DiaAPI) to create Python modules and register functions, without exposing pybind11 headers.

## Acceptance Criteria

1. ✅ Can create Python module via `CreateModule(const char* name)`
2. ✅ Can register C++ function to Python module via `AddFunction(module, name, callback, docstring)`
3. ✅ Created modules are importable from Python (e.g., `import dia_api`)
4. ✅ Registered functions are callable from Python
5. ✅ Function callbacks receive `PythonArgs` and return `PythonObject`
6. ✅ Module names are validated (no invalid characters, not empty)
7. ✅ Duplicate function registration logs warning but doesn't crash
8. ✅ Modules can be created before or after Python initialization (cached if before)
9. ✅ No pybind11 headers exposed in public API (opaque handles only)
10. ✅ Functions can have docstrings that show in Python `help()`
11. ✅ Function overloading supported (same name, different signatures)
12. ✅ C++ exceptions in callbacks converted to Python exceptions

## API Design

### Public Types

```cpp
namespace Dia::Python {
    // Opaque handle to Python module (hides pybind11::module_)
    class Module {
        // No public constructor - use CreateModule()
        // Module cannot be destroyed or copied
        Module(const Module&) = delete;
        Module& operator=(const Module&) = delete;
    };
    
    // Opaque wrapper for Python object (hides pybind11::object)
    class PythonObject {
    public:
        PythonObject();  // None/null object
        ~PythonObject();
        
        // Copyable (reference counted, like Python objects)
        PythonObject(const PythonObject& other);
        PythonObject& operator=(const PythonObject& other);
        
        bool IsNone() const;
        bool IsValid() const;
        
        // Internal pybind11 access (not in public header)
    private:
        void* mImpl;  // pybind11::object*
    };
    
    // Opaque wrapper for Python function arguments (hides pybind11::args)
    class PythonArgs {
    public:
        int GetCount() const;
        PythonObject GetArg(int index) const;
        
    private:
        void* mImpl;  // pybind11::args*
    };
}
```

### Public Functions

```cpp
namespace Dia::Python {
    // Create a new Python module
    // Parameters:
    //   name - Module name (e.g., "dia_api")
    // Returns: Module handle, or nullptr if name invalid or module already exists
    // Note: If Python not initialized, module is cached and registered on Initialize()
    Module* CreateModule(const char* name);
    
    // Get an existing Python module
    // Parameters:
    //   name - Module name
    // Returns: Module handle, or nullptr if module doesn't exist
    Module* GetModule(const char* name);
    
    // Python function callback signature
    // Parameters:
    //   args - Function arguments from Python
    // Returns: Return value sent back to Python (or PythonObject() for None)
    // Note: C++ exceptions are caught and converted to Python exceptions
    using PythonCallback = std::function<PythonObject(const PythonArgs& args)>;
    
    // Register a C++ function in a Python module
    // Parameters:
    //   module - Module created by CreateModule()
    //   functionName - Python function name
    //   callback - C++ function to invoke
    //   docstring - Optional documentation string (shown in Python help())
    // Note: Duplicate names log warning; last registration wins
    void AddFunction(
        Module* module,
        const char* functionName,
        PythonCallback callback,
        const char* docstring = nullptr
    );
    
    // Register overloaded function (same name, different signature hint)
    // Parameters:
    //   signatureHint - Human-readable signature for Python help (e.g., "(int, str)")
    void AddFunctionOverload(
        Module* module,
        const char* functionName,
        PythonCallback callback,
        const char* signatureHint,
        const char* docstring = nullptr
    );
}
```

## Data Models

### Internal State

```cpp
namespace Dia::Python {
    namespace Internal {
        // Module wrapper (hides pybind11::module_)
        struct ModuleImpl {
            std::string name;
            py::module_* pybindModule = nullptr;  // null if Python not initialized
            bool isPendingRegistration = false;
            DynamicArrayC<FunctionRegistration> pendingFunctions;
        };
        
        // Function registration (for deferred registration)
        struct FunctionRegistration {
            std::string name;
            PythonCallback callback;
            std::string docstring;
            std::string signatureHint;  // Empty for non-overloaded
        };
        
        // Global module registry
        static HashTable<StringCRC, ModuleImpl*> gModules;
    }
}
```

## Implementation Tasks

1. **Implement Module class**
   - Opaque handle (pointer to ModuleImpl)
   - No copy/assignment
   - Internal storage of pybind11::module_

2. **Implement PythonObject class**
   - Opaque wrapper around pybind11::object
   - Constructor/destructor manage lifetime
   - IsNone(), IsValid() helper methods
   - Internal access for type-conversion feature

3. **Implement PythonArgs class**
   - Opaque wrapper around pybind11::args
   - GetCount(), GetArg(index) accessors
   - Internal conversion to/from pybind11::args

4. **Implement CreateModule()**
   - Validate module name (alphanumeric + underscore, not empty)
   - Check if module already exists (return nullptr if duplicate)
   - If Python initialized:
     - Create pybind11::module_ immediately
     - Register in global module registry
   - If Python not initialized:
     - Create ModuleImpl with isPendingRegistration = true
     - Register in global module registry
     - Will be created when Initialize() is called

5. **Implement AddFunction()**
   - Validate module pointer (not null)
   - Validate function name (not empty)
   - If module's pybind11 handle exists:
     - Wrap callback to catch C++ exceptions and convert to Python
     - Use pybind11 module.def() to register function
     - Store docstring if provided
   - If module pending registration:
     - Add FunctionRegistration to pendingFunctions list
     - Will be registered when module is created

6. **Implement AddFunctionOverload()**
   - Similar to AddFunction but stores signatureHint
   - Multiple functions with same name allowed
   - Python sees multiple signatures in help()

7. **Update Initialize() to register pending modules**
   - After Python interpreter starts
   - For each module with isPendingRegistration:
     - Create pybind11::module_
     - Register all pendingFunctions
     - Clear isPendingRegistration flag

8. **Implement C++ exception handling in callback wrapper**
   - Wrap PythonCallback invocation in try/catch
   - Catch std::exception and convert to Python RuntimeError
   - Catch unknown exceptions (...) and convert to generic Python exception
   - Log exception details via DiaCore

9. **Write unit tests**
   - Test: CreateModule with valid name
   - Test: CreateModule with invalid name (empty, special chars)
   - Test: CreateModule duplicate name (returns nullptr)
   - Test: CreateModule before Initialize (deferred registration)
   - Test: AddFunction and call from Python
   - Test: AddFunction with docstring (verify help() shows it)
   - Test: AddFunction duplicate name (warning logged, last wins)
   - Test: AddFunctionOverload with multiple signatures
   - Test: C++ exception in callback converts to Python exception
   - Test: PythonArgs GetCount/GetArg work correctly
   - Test: PythonObject IsNone/IsValid work correctly

## Files Affected

### New Files
- `Dia/DiaPython/DiaPythonModule.h` - Module class definition (opaque)
- `Dia/DiaPython/DiaPythonModule.cpp` - Module implementation
- `Dia/DiaPython/DiaPythonObject.h` - PythonObject/PythonArgs classes
- `Dia/DiaPython/DiaPythonObject.cpp` - PythonObject/PythonArgs implementation
- `Cluiche/Tests/UnitTests/DiaPythonModuleTests.cpp` - Unit tests

### Modified Files
- `Dia/DiaPython/DiaPython.h` - Add CreateModule, AddFunction, Module/PythonObject/PythonArgs forward declarations
- `Dia/DiaPython/DiaPython.cpp` - Add CreateModule, AddFunction implementations
- `Dia/DiaPython/DiaPythonInternal.h` - Add ModuleImpl, FunctionRegistration, module registry
- `Dia/DiaPython/DiaPython.cpp` (Initialize) - Add pending module registration logic

## Dependencies

### Dia Systems
- **interpreter-lifecycle** - Modules can be registered before/after Initialize()
- **DiaCore/Core** - Logging for warnings/errors
- **DiaCore/Containers** - HashTable for module registry, DynamicArrayC for pending functions
- **DiaCore/CRC** - StringCRC for module name hashing
- **DiaCore/Strings** - String256 for name validation

### External
- **pybind11** - py::module_, py::object, py::args, exception conversion

## Open Questions

1. Should module names follow Python naming conventions (lowercase_with_underscores)?
2. Should there be a maximum number of modules?
3. Should we support nested modules (e.g., "dia.cli")?
4. How should we handle function signature mismatches (wrong arg count in Python)?
5. Should PythonObject/PythonArgs be copyable or move-only?

## Binding Decisions Compliance

| ID | Decision Summary | Compliance |
|----|------------------|------------|
| PD-001 | Use StringCRC for entity/component IDs | **Compliant** - Module names hashed to StringCRC for internal lookup. Public API still accepts const char* for convenience. |
| PD-004 | No STL containers in public APIs | **Compliant** - Public API uses only Module*, const char*, PythonCallback, PythonObject, PythonArgs. No STL types exposed. std::function is C++11 standard, not STL container. |
| PD-006 | Visual Studio project files are source of truth | **Compliant** - Implementation in existing DiaPython.vcxproj. |
| AD-001 | Module system with YAML frontmatter documentation | **Compliant** - Updates to existing dia.python.architecture.module.md. |
| AD-002 | No STL containers in public APIs | **Compliant** - Reinforces PD-004. Internal use of std::string and std::function is implementation detail. |
| AD-003 | Namespace convention: `Dia::<Module>::` | **Compliant** - All code in `Dia::Python::` namespace. |
| SD-001 | Wrap pybind11, don't expose it | **Compliant** - Module, PythonObject, PythonArgs are opaque handles. pybind11 types only in .cpp files and DiaPythonInternal.h. |
| SD-002 | Python interpreter is singleton | **Compliant** - Single global module registry. All modules use same Python interpreter. |
| SD-004 | Opaque handle types (Module, PythonObject) | **Compliant** - Module, PythonObject, PythonArgs all opaque. Internal pybind11 types hidden via void* or forward declarations. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Open Questions | Should module names follow Python naming conventions (lowercase_with_underscores)? | Enforce Python conventions (lowercase_with_underscores only - validated in CreateModule) |
| 2 | Open Questions | Should there be a maximum number of modules? | Yes, hard limit of 32 modules (prevents resource exhaustion) |
| 3 | Open Questions | Should we support nested modules (e.g., "dia.cli")? | Yes - support dot notation for nested modules (better organization) |
| 4 | Open Questions | How should we handle function signature mismatches (wrong arg count in Python)? | Python raises TypeError automatically (pybind11 default behavior - natural Python errors) |
| 5 | Open Questions | Should PythonObject/PythonArgs be copyable or move-only? | Copyable (like Python objects - reference counted, lightweight copies) |
| 6 | Implementation | Should AddFunction validate callback is not null? | Yes - assert in debug, return error in release (fail-fast on invalid input) |
| 7 | API Design | Should there be GetModule(name) to retrieve existing module? | Yes - allows other systems to add functions to existing modules (flexible architecture) |
| 8 | Error Handling | What happens if AddFunction called on null module pointer? | Assert/crash in debug, log error in release (fail-fast on programmer error) |
| 9 | Testing | Should we test with DiaAPI as integration test (real use case)? | Yes - create integration test with DiaAPI registering commands (validates real-world usage) |
| 10 | Dependencies | Does this feature depend on type-conversion feature for PythonObject conversion? | Yes - module-api depends on type-conversion for PythonObject/PythonArgs to work properly (implement type-conversion first) |

## Decisions

| ID | Decision | Rationale | Status |
|----|----------|-----------|--------|
| FD-001 | Modules are global and cannot be destroyed | Simpler lifecycle; matches Python module behavior; avoids dangling references | Proposed |
| FD-002 | Modules can be created before Python initialization | Allows natural initialization order (system creates module in constructor, Python initialized later) | Proposed |
| FD-003 | C++ exceptions in callbacks converted to Python exceptions | Natural Python behavior; doesn't crash; errors visible in Python traceback | Proposed |
| FD-004 | Overloaded functions supported via AddFunctionOverload | Enables natural C++ patterns; Python sees multiple signatures in help() | Proposed |
| FD-005 | Module, PythonObject, PythonArgs are opaque handles | Enforces SD-001; hides pybind11 from users; allows implementation changes | Proposed |

## Status
**Done** - Implemented and tested. Core functionality working.

`Done` - Implemented
