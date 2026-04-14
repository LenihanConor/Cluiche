# Feature Spec: Error Handling

## Traceability

| Level | Spec | Link |
|-------|------|------|
| **Feature** | error-handling | (this document) |
| **System** | DiaPython | @docs/specs/systems/dia/diapython.md |
| **Application** | Dia | @docs/specs/applications/dia.md |
| **Platform** | Cluiche | @docs/specs/platform/Cluiche.md |

## Problem Statement

Provides consistent Python exception handling and error propagation across all DiaPython features, ensuring errors are logged, events are fired, and C++ code handles Python failures gracefully.

## Acceptance Criteria

1. ✅ All Python exceptions are caught and converted to C++ error codes
2. ✅ Python tracebacks are captured and logged with file/line info
3. ✅ C++ exceptions in Python callbacks are converted to Python exceptions
4. ✅ `OnPythonError` event fires for all Python errors
5. ✅ Error messages include Python type name and context
6. ✅ Error codes match documented ScriptError enum
7. ✅ Initialization failures log detailed error info
8. ✅ Type conversion errors log source type and target type
9. ✅ Script execution errors include script path in logs
10. ✅ All error paths tested with unit tests

## API Design

### Error Codes

```cpp
namespace Dia::Python {
    enum class ErrorCode {
        Success = 0,
        GeneralError = 1,
        FileNotFound = 2,
        NotInitialized = 3,
        SyntaxError = 4,
        RuntimeException = 5,
        TypeError = 6,
        InitializationFailed = 7
    };
}
```

### Events

```cpp
namespace Dia::Python {
    // Fired when any Python error occurs
    // Parameters:
    //   errorType - Python exception type name (e.g., "ValueError", "SyntaxError")
    //   errorMessage - Full error message with traceback
    //   context - Where the error occurred (e.g., "ExecuteScript", "ToInt")
    void OnPythonError(const char* errorType, const char* errorMessage, const char* context);
}
```

### Error Utilities

```cpp
namespace Dia::Python {
    namespace Internal {
        // Error context for debugging
        struct ErrorContext {
            const char* operation;      // "ExecuteScript", "ToInt", etc.
            const char* scriptPath;     // Script file path (if applicable)
            int lineNumber;             // Script line number (if available)
        };
        
        // Convert pybind11 exception to error code
        ErrorCode ConvertException(const py::error_already_set& ex, const ErrorContext& context);
        
        // Extract Python traceback as string
        const char* ExtractTraceback(const py::error_already_set& ex);
        
        // Log error and fire event
        void ReportError(ErrorCode code, const char* errorType, const char* message, const ErrorContext& context);
        
        // Wrap callback to catch C++ exceptions and convert to Python
        template<typename Func>
        auto WrapCallback(Func&& func, const char* functionName) -> decltype(auto);
    }
}
```

## Data Models

### Internal State

```cpp
namespace Dia::Python {
    namespace Internal {
        // Last error info (for debugging)
        struct LastError {
            ErrorCode code;
            std::string errorType;
            std::string message;
            std::string context;
            std::string traceback;
        };
        
        static LastError gLastError;
    }
}
```

## Implementation Tasks

1. **Implement ConvertException()**
   - Check exception type: `py::error_already_set`
   - Extract Python exception type name
   - Map to ErrorCode enum
   - Return appropriate error code

2. **Implement ExtractTraceback()**
   - Use pybind11 traceback API
   - Format as string with file:line info
   - Cache result for logging

3. **Implement ReportError()**
   - Log error via DiaCore with severity based on error code
   - Store in gLastError for debugging
   - Fire OnPythonError event

4. **Implement WrapCallback() template**
   - Wrap function in try/catch
   - Catch std::exception → convert to Python RuntimeError
   - Catch unknown (...) → generic Python exception
   - Include function name in error message

5. **Update Initialize() error handling**
   - Wrap Py_Initialize() call
   - Catch initialization failures
   - Log detailed error (missing Python DLLs, path issues)
   - Return false with error event

6. **Update ExecuteScript/ExecuteString() error handling**
   - Wrap py::eval_file/py::exec in try/catch
   - Use ErrorContext with script path
   - Convert exceptions via ConvertException()
   - Report via ReportError()

7. **Update type conversion error handling**
   - Wrap py::cast in try/catch
   - Log source type → target type mismatch
   - Return default value
   - Report via ReportError()

8. **Update AddFunction() callback wrapping**
   - Use WrapCallback template
   - Include module.function name in context
   - Ensure C++ exceptions visible in Python traceback

9. **Write unit tests**
   - Test: Python SyntaxError converted to ErrorCode::SyntaxError
   - Test: Python RuntimeError converted to ErrorCode::RuntimeException
   - Test: Python FileNotFoundError converted to ErrorCode::FileNotFound
   - Test: Traceback extraction includes file/line
   - Test: OnPythonError event fired with correct params
   - Test: C++ exception in callback → Python exception
   - Test: ToInt with invalid type logs TypeError
   - Test: ExecuteScript with missing file logs FileNotFound
   - Test: Initialize failure logs InitializationFailed
   - Test: gLastError populated correctly

## Files Affected

### New Files
- `Dia/DiaPython/DiaPythonError.h` - Error utilities and ErrorContext
- `Dia/DiaPython/DiaPythonError.cpp` - Error handling implementation
- `Cluiche/Tests/UnitTests/DiaPythonErrorTests.cpp` - Unit tests

### Modified Files
- `Dia/DiaPython/DiaPython.cpp` - Update Initialize() error handling
- `Dia/DiaPython/DiaPythonScript.cpp` - Update ExecuteScript/ExecuteString() error handling
- `Dia/DiaPython/DiaPythonConversion.cpp` - Update type conversion error handling
- `Dia/DiaPython/DiaPythonModule.cpp` - Update AddFunction() callback wrapping
- `Dia/DiaPython/DiaPythonInternal.h` - Add LastError state

## Dependencies

### Dia Systems
- **interpreter-lifecycle** - Error handling for Initialize/Shutdown
- **script-execution** - Error handling for script execution
- **type-conversion** - Error handling for type conversions
- **module-api** - Callback wrapping for C++ exceptions
- **DiaCore/Core** - Logging infrastructure
- **DiaCore/Architecture/Observer** - OnPythonError event emission

### External
- **pybind11** - `py::error_already_set`, traceback API, exception conversion

## Open Questions

None - implementation can proceed with existing patterns.

## Binding Decisions Compliance

| ID | Decision Summary | Compliance |
|----|------------------|------------|
| PD-001 | Use StringCRC for entity/component IDs | **Compliant** - No entity/component IDs. Error types and contexts are runtime strings for debugging. |
| PD-004 | No STL containers in public APIs | **Compliant** - Public API only has OnPythonError event. Internal std::string usage for error storage. |
| PD-006 | Visual Studio project files are source of truth | **Compliant** - Implementation in existing DiaPython.vcxproj. |
| AD-001 | Module system with YAML frontmatter documentation | **Compliant** - Updates to existing dia.python.architecture.module.md. |
| AD-002 | No STL containers in public APIs | **Compliant** - Reinforces PD-004. |
| AD-003 | Namespace convention: `Dia::<Module>::` | **Compliant** - All code in `Dia::Python::` namespace. |
| SD-001 | Wrap pybind11, don't expose it | **Compliant** - pybind11::error_already_set handled internally, not exposed in public headers. |
| SD-002 | Python interpreter is singleton | **Compliant** - Single gLastError state for the global interpreter. |
| SD-005 | Python exceptions converted to exit codes | **Compliant** - This feature implements the conversion mechanism defined in SD-005. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Error Codes | Should we distinguish between more Python exception types (ValueError, KeyError, etc.)? | No - map to RuntimeException; log specific type in error message (simpler enum, detailed logs) |
| 2 | Error Context | Should ErrorContext include timestamp? | No - DiaCore logging adds timestamps automatically (avoid duplication) |
| 3 | Implementation | Should gLastError be thread-local for multi-threaded scenarios? | No - single-threaded Python interpreter (GIL). If multi-threading added later, revisit. |
| 4 | API Design | Should there be GetLastError() public API? | No - error info available via events and logs (simpler API surface) |
| 5 | Testing | Should we test with intentionally broken Python code? | Yes - test scripts with syntax errors, runtime errors, missing imports (validates real-world failure paths) |
| 6 | Performance | Should we limit traceback size (very deep stacks)? | Yes - truncate at 50 frames (prevents log spam, still useful for debugging) |
| 7 | Error Recovery | Should there be ClearLastError() to reset state? | No - automatically overwritten on next error (simpler, stateless behavior) |
| 8 | Logging | What log level for different error types? | Syntax/Runtime = Error; Type conversion = Warning; Init failure = Critical (matches severity) |

## Decisions

| ID | Decision | Rationale | Status |
|----|----------|-----------|--------|
| FD-001 | Error codes match documented ScriptError enum | Consistency across features; users reference single source of truth | Proposed |
| FD-002 | Python tracebacks captured and logged | Essential for debugging Python errors; shows call stack | Proposed |
| FD-003 | C++ exceptions in callbacks converted to Python | Prevents crashes; Python sees natural exceptions | Proposed |
| FD-004 | OnPythonError event includes context (operation name) | Helps identify which DiaPython feature failed | Proposed |
| FD-005 | gLastError stored for debugging (not public API) | Useful in debugger; not needed in public API (events + logs sufficient) | Proposed |
| FD-006 | WrapCallback template for automatic exception conversion | Reduces boilerplate; consistent error handling across all callbacks | Proposed |

## Status
**Done** - Implemented and tested. Core functionality working.

`Approved` - Ready for implementation
