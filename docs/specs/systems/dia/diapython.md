# System Spec: DiaPython

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaPython is a Python embedding framework that provides infrastructure for integrating Python scripting into the Dia engine. It wraps pybind11 with a clean C++ API, allowing other Dia systems (like DiaCLI) to expose themselves to Python without direct pybind11 knowledge. The system manages the Python interpreter lifecycle, script execution, and provides type conversion utilities between C++ and Python.

**Key principle:** DiaPython is agnostic infrastructure - it doesn't know about CLI commands, graphics, or any specific Dia subsystem. It just provides the Python embedding foundation that other systems build upon.

**Design decision:** Script execution is **synchronous only**. Asynchronous execution was removed for simplicity and to avoid GIL (Global Interpreter Lock) threading complexity. This matches DiaCLI's synchronous command model.

## Responsibilities

- Manage Python interpreter lifecycle (initialize, shutdown, state checking)
- Execute Python scripts (.py files) and Python code strings
- Wrap pybind11 with clean C++ API (hide pybind11 headers from other systems)
- Provide Python module creation and function registration API
- C++ ↔ Python type conversion utilities (hide pybind11::object)
- Python exception handling and error propagation to C++
- Python path and module management

## Public Interfaces

### Endpoints / APIs

**Python Interpreter Lifecycle:**
```cpp
namespace Dia::Python {
    // Initialize Python interpreter
    // Parameters:
    //   pythonHome - Path to embedded Python runtime (e.g., "External/Python310/")
    //   modulePath - Path to custom Python modules (e.g., "External/Python/")
    //   captureWarnings - If true, Python warnings are captured and logged (default: false)
    // Returns: true on success, false on failure (logs error)
    bool Initialize(const char* pythonHome, const char* modulePath, bool captureWarnings = false);
    
    // Shutdown Python interpreter
    void Shutdown();
    
    // Check if Python is initialized
    bool IsInitialized();
}
```

**Script Execution:**
```cpp
namespace Dia::Python {
    // Execute Python script file
    // Returns: exit code (0 = success, non-zero = error)
    int ExecuteScript(const char* scriptPath);
    
    // Execute Python code string
    // Returns: exit code (0 = success, non-zero = error)
    int ExecuteString(const char* pythonCode);
}
```

**Module Creation (hides pybind11):**
```cpp
namespace Dia::Python {
    // Opaque handle to Python module (hides pybind11::module)
    class Module;
    
    // Create a new Python module
    Module* CreateModule(const char* name);
}
```

**Function Registration (hides pybind11::cpp_function):**
```cpp
namespace Dia::Python {
    // Python function callback signature
    using PythonCallback = std::function<PythonObject(const PythonArgs&)>;
    
    // Register a C++ function in a Python module
    void AddFunction(
        Module* module,
        const char* functionName,
        PythonCallback callback,
        const char* docstring = nullptr
    );
}
```

**Type Conversion (hides pybind11::object):**
```cpp
namespace Dia::Python {
    // Opaque wrappers (hide pybind11 types)
    class PythonObject;  // Wraps pybind11::object
    class PythonArgs;    // Wraps pybind11::args
    
    // C++ → Python conversion
    PythonObject ToPython(int value);
    PythonObject ToPython(float value);
    PythonObject ToPython(const char* str);
    PythonObject ToPython(bool value);
    
    // Python → C++ conversion
    int ToInt(const PythonObject& obj);
    float ToFloat(const PythonObject& obj);
    const char* ToString(const PythonObject& obj);
    bool ToBool(const PythonObject& obj);
    
    // PythonArgs utilities
    int GetArgCount(const PythonArgs& args);
    PythonObject GetArg(const PythonArgs& args, int index);
}
```

### Events Emitted

- `OnPythonInitialized()` - After Python interpreter starts successfully
- `OnPythonShutdown()` - Before Python interpreter shuts down
- `OnScriptExecuting(scriptPath)` - Before script execution begins
- `OnScriptExecuted(scriptPath, exitCode, duration)` - After script completes
- `OnPythonError(errorType, errorMessage)` - When Python raises exception or error occurs

### Data Contracts

**Exit Codes:**
- `0` - Success
- `1` - General Python error
- `2` - Script file not found
- `3` - Python not initialized
- `4` - Python syntax error
- `5` - Python runtime exception

**Error Handling:**
- Python exceptions are caught and converted to C++ error codes
- Python tracebacks are logged via DiaCore logging
- C++ exceptions thrown during callbacks are converted to Python exceptions

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| interpreter-lifecycle | Python interpreter initialization, shutdown, state management | [interpreter-lifecycle.md](../../features/dia/diapython/interpreter-lifecycle.md) | Done |
| script-execution | Execute .py files and Python code strings (synchronous only) | [script-execution.md](../../features/dia/diapython/script-execution.md) | Done |
| module-api | Clean C++ API for creating Python modules (wraps pybind11) | [module-api.md](../../features/dia/diapython/module-api.md) | Done |
| type-conversion | C++ ↔ Python type conversion utilities | [type-conversion.md](../../features/dia/diapython/type-conversion.md) | Done |
| error-handling | Python exception handling and propagation | [error-handling.md](../../features/dia/diapython/error-handling.md) | Done |

## Platform Primitives Used

- **DiaCore/Core** - Logging, assertions
- **DiaCore/Containers** - DynamicArrayC, HashTable for internal storage
- **DiaCore/Strings** - String64, String256 for path/name handling
- **DiaCore/Type** - TypeInstance for advanced type conversion (future)
- **DiaCore/FilePath** - Script path validation
- **DiaCore/Architecture/Observer** - Event emission (OnPython* events)

## Dependencies on Other Systems

**Dia Systems:**
- None initially - DiaPython is a foundational system
- Future: Other Dia systems will depend on DiaPython to expose themselves to Python

**External Dependencies:**
- **pybind11** - C++/Python binding library (header-only, git submodule in External/pybind11/)
- **Python 3.10+ runtime** - Embedded CPython interpreter (bundled in External/Python310/)

## Out of Scope

- **Specific bindings** - DiaPython does not expose DiaCLI, DiaGraphics, or any specific system (those systems use DiaPython to expose themselves)
- **Python package management** - No pip, virtualenv, or package installation (embedded Python only, modules in External/Python/)
- **Python debugging** - No integrated debugger (use external Python debuggers)
- **Async Python** - No asyncio or async/await support (synchronous only)
- **Python C API direct usage** - All Python interaction through pybind11 wrapper
- **Multi-threaded Python** - Python code executes on single thread (GIL initialized for future-proofing, but no concurrent Python execution)

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | Wrap pybind11, don't expose it | Other systems shouldn't need pybind11 headers; easier to swap binding library later | All DiaPython users | Accepted | Yes |
| SD-002 | Python interpreter is singleton | Only one Python interpreter per process; simpler lifecycle management | This system | Accepted | Yes |
| SD-003 | Python interpreter is single-threaded (GIL), but script execution can be sync or async | Async execution runs on worker threads, but GIL ensures only one Python thread executes at a time; allows long scripts without blocking | All features | Accepted | Yes |
| SD-004 | Opaque handle types (Module, PythonObject) | Hide pybind11 types from public API; enforce abstraction | Public API | Accepted | Yes |
| SD-005 | Python exceptions converted to exit codes | C++ callers don't need to handle Python exceptions directly | Script execution API | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | Use StringCRC for all entity/component IDs | Module names use StringCRC internally for efficient lookup. Public API still accepts const char* for convenience. |
| PD-004 | Platform | No STL containers in public APIs | Use DynamicArrayC, HashTable instead of std::vector, std::map in any public API structs. Internal implementation can use STL. |
| PD-006 | Platform | Visual Studio project files are source of truth | DiaPython must be a .vcxproj with proper filters. Cannot use CMake or other build systems. |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | Create dia.python.architecture.module.md with public API, dependencies, responsibilities. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004 - use Dia containers throughout public API. |
| AD-003 | Dia App | Namespace convention: `Dia::<Module>::` | All code in `Dia::Python::` namespace. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Purpose | Should DiaPython support multiple Python interpreters (sub-interpreters)? | No - singleton interpreter is simpler. Re-evaluate if multi-threading Python becomes necessary. |
| 2 | Public Interfaces | Should type conversion support DiaCore TypeInstance for automatic serialization? | Future enhancement - start with primitive types (int, float, string, bool). Add TypeInstance support in phase 2. |
| 3 | Dependencies | Which Python version should we target? | Python 3.10+ (newer features, better performance). Embedded runtime bundled with application. |
| 4 | Out of Scope | Should we support NumPy or other Python libraries? | Not required initially. If external libraries are needed, users can import them from Python side. DiaPython just provides the infrastructure. |
| 5 | Scope | Should DiaPython provide automatic binding generation (like SWIG)? | No - manual binding via DiaPython API gives more control. Auto-generation adds complexity and build-time dependencies. |
| 6 | Architecture | How do other systems (like DiaCLI) register their modules? | They call `CreateModule()` and `AddFunction()` during their initialization. DiaPython doesn't manage a registry - each system exposes itself. |

## Status

`Draft` - System spec defined, implementation not started
