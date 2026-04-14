# Feature Spec: Interpreter Lifecycle

## Traceability

| Level | Spec | Link |
|-------|------|------|
| **Feature** | interpreter-lifecycle | (this document) |
| **System** | DiaPython | @docs/specs/systems/dia/diapython.md |
| **Application** | Dia | @docs/specs/applications/dia.md |
| **Platform** | Cluiche | @docs/specs/platform/Cluiche.md |

## Problem Statement

Manages the embedded Python interpreter's initialization and shutdown lifecycle so other Dia systems can safely execute Python code.

## Acceptance Criteria

1. ✅ Python interpreter can be initialized via `Dia::Python::Initialize()`
2. ✅ Python interpreter can be shut down via `Dia::Python::Shutdown()`
3. ✅ `Dia::Python::IsInitialized()` returns correct state
4. ✅ Calling `Initialize()` twice logs warning but doesn't crash
5. ✅ Calling `Shutdown()` when not initialized logs warning but doesn't crash
6. ✅ Attempting to execute Python code when not initialized returns error code
7. ✅ Python sys.path is configured correctly (can import standard library)
8. ✅ OnPythonInitialized event fires after successful initialization
9. ✅ OnPythonShutdown event fires before shutdown

## API Design

### Public Functions

```cpp
namespace Dia::Python {
    // Initialize Python interpreter
    // Parameters:
    //   pythonHome - Path to embedded Python runtime (e.g., "External/Python310/")
    //   modulePath - Path to custom Python modules (e.g., "External/Python/")
    //   captureWarnings - If true, Python warnings are captured and logged (default: false)
    // Returns: true on success, false on failure
    // Pre-condition: Python not already initialized
    // Post-condition: Python is ready for use, sys.path configured, OnPythonInitialized event fired
    bool Initialize(const char* pythonHome, const char* modulePath, bool captureWarnings = false);
    
    // Shutdown Python interpreter
    // Pre-condition: Python is initialized
    // Post-condition: Python is finalized, OnPythonShutdown event fired
    void Shutdown();
    
    // Check if Python is initialized
    // Returns: true if Initialize() was called successfully and Shutdown() has not been called
    bool IsInitialized();
}
```

### Events

```cpp
// Fired after successful Initialize()
void OnPythonInitialized();

// Fired before Shutdown()
void OnPythonShutdown();
```

### Error Handling

- `Initialize()` returns `false` if:
  - Python is already initialized (logs warning)
  - Python interpreter fails to start (logs error with Python error details)
  
- `Shutdown()` behavior:
  - If not initialized: logs warning, no-op
  - If initialized: finalizes Python, clears state

- Other DiaPython functions check `IsInitialized()` and return error code 3 (Python not initialized) if called before `Initialize()`

## Data Models

### Internal State

```cpp
namespace Dia::Python {
    namespace Internal {
        // Singleton state (not exposed in public API)
        struct InterpreterState {
            bool isInitialized = false;
            py::scoped_interpreter* interpreter = nullptr;  // pybind11 RAII handle
        };
        
        static InterpreterState gState;
    }
}
```

## Implementation Tasks

1. **Create DiaPython project structure**
   - `Dia/DiaPython/DiaPython.vcxproj` - Visual Studio project
   - `Dia/DiaPython/DiaPython.h` - Public header
   - `Dia/DiaPython/DiaPython.cpp` - Implementation
   - `Dia/DiaPython/DiaPythonInternal.h` - Internal state (pybind11 details)

2. **Implement Initialize()**
   - Check if already initialized (return false + warning)
   - Create `py::scoped_interpreter` instance
   - Configure Python sys.path (add current directory, External/Python paths)
   - Set `isInitialized = true`
   - Fire `OnPythonInitialized` event
   - Return true on success

3. **Implement Shutdown()**
   - Check if initialized (warning + return if not)
   - Fire `OnPythonShutdown` event
   - Delete `py::scoped_interpreter` instance (RAII cleanup)
   - Set `isInitialized = false`

4. **Implement IsInitialized()**
   - Return `gState.isInitialized`

5. **Add event infrastructure**
   - Use `Dia::Core::ObserverSubject` for event emission
   - Create `PythonLifecycleEvents` observer subject

6. **Add error logging**
   - Use `DiaCore/Core/Logging` for all warnings/errors
   - Capture Python exceptions during initialization and log details

7. **Write unit tests**
   - Test: Initialize() success
   - Test: Initialize() twice (warning, no crash)
   - Test: Shutdown() success
   - Test: Shutdown() when not initialized (warning, no crash)
   - Test: IsInitialized() returns correct state
   - Test: Events fire correctly

## Files Affected

### New Files
- `Dia/DiaPython/DiaPython.h` - Public API
- `Dia/DiaPython/DiaPython.cpp` - Implementation
- `Dia/DiaPython/DiaPythonInternal.h` - Internal pybind11 wrapper
- `Dia/DiaPython/DiaPython.vcxproj` - Visual Studio project
- `Dia/DiaPython/DiaPython.vcxproj.filters` - VS project filters
- `Dia/DiaPython/Docs/dia.python.architecture.module.md` - Module documentation
- `Cluiche/Tests/UnitTests/DiaPythonTests.cpp` - Unit tests

### Modified Files
- `Cluiche/Cluiche.sln` - Add DiaPython project
- `External/` - Add pybind11 library (if not present)

## Dependencies

### Build-time
- **pybind11** (header-only library) - Place in `External/pybind11/`
- **Python 3.8+ development headers** - Require Python installed on build machine

### Runtime
- **Python 3.8+ runtime** - Must be on system PATH or embedded with executable

### Dia Systems
- **DiaCore/Core** - Logging, assertions
- **DiaCore/Architecture/Observer** - Event emission

## Open Questions

1. **Python version**: Should we target specific Python version (e.g., 3.8, 3.9, 3.10) or latest available?
2. **Python location**: Should we embed Python runtime or require system Python installation?
3. **Module path**: Where should Python look for .py modules? Current directory? External/Python/? Both?
4. **Thread safety**: Should we initialize Python GIL state even though we're single-threaded?

## Binding Decisions Compliance

| ID | Decision Summary | Compliance |
|----|------------------|------------|
| PD-001 | Use StringCRC for entity/component IDs | **Compliant** - No entity/component IDs in this feature. Module names are const char* as they're configuration strings, not runtime identifiers. |
| PD-004 | No STL containers in public APIs | **Compliant** - Public API uses only primitives (bool) and const char*. No containers exposed. |
| PD-006 | Visual Studio project files are source of truth | **Compliant** - DiaPython.vcxproj will be created and maintained manually. |
| AD-001 | Module system with YAML frontmatter documentation | **Compliant** - dia.python.architecture.module.md will be created with YAML frontmatter. |
| AD-002 | No STL containers in public APIs | **Compliant** - Reinforces PD-004. Public API is STL-free. |
| AD-003 | Namespace convention: `Dia::<Module>::` | **Compliant** - All code in `Dia::Python::` namespace. |
| SD-001 | Wrap pybind11, don't expose it | **Compliant** - pybind11 types hidden in DiaPythonInternal.h. Public API has no pybind11 headers. |
| SD-002 | Python interpreter is singleton | **Compliant** - Single global InterpreterState. Initialize() can only be called once. |
| SD-003 | Synchronous execution only | **Compliant** - No async/GIL complexity in this feature. Single-threaded initialization. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Open Questions | Which Python version should we target? | Python 3.10+ (newer features, better performance) |
| 2 | Open Questions | Should we embed Python runtime or require system installation? | Embed Python runtime with application (larger exe, no dependencies) |
| 3 | Open Questions | Where should Python look for .py modules (sys.path)? | `External/Python/` directory only (controlled module location) |
| 4 | Open Questions | Should we initialize GIL state for future multi-threading? | Yes - initialize GIL even though currently single-threaded (future-proofing) |
| 5 | Implementation | Should Initialize() take configuration parameters (Python home, sys.path)? | Yes - `Initialize(const char* pythonHome, const char* modulePath)` for explicit configuration |
| 6 | Error Handling | What should happen if Python fails to initialize (missing runtime, corrupted install)? | Return false, log error, allow application to continue (graceful degradation - Python features disabled) |
| 7 | Testing | Should we test with multiple Python versions or just one? | Test with Python 3.10 only (simplest - single version to maintain) |
| 8 | Dependencies | Should pybind11 be a git submodule or manual download in External/? | Git submodule (automatically versioned, easy to update via `git submodule update`) |

## Decisions

| ID | Decision | Rationale | Status |
|----|----------|-----------|--------|
| FD-001 | Use py::scoped_interpreter for RAII | Automatic cleanup on exception; matches pybind11 best practices | Proposed |
| FD-002 | Initialize() is idempotent (logs warning on re-init) | Prevents crashes; easier debugging; common pattern for lifecycle functions | Proposed |
| FD-003 | Initialize() takes pythonHome and modulePath parameters | Explicit configuration; supports embedded Python; allows custom module paths | Proposed |

## Status

`Approved` - Ready for implementation
