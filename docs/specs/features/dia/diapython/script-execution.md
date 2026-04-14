# Feature Spec: Script Execution

## Traceability

| Level | Spec | Link |
|-------|------|------|
| **Feature** | script-execution | (this document) |
| **System** | DiaPython | @docs/specs/systems/dia/diapython.md |
| **Application** | Dia | @docs/specs/applications/dia.md |
| **Platform** | Cluiche | @docs/specs/platform/Cluiche.md |

## Problem Statement

Enables execution of Python scripts (.py files) and Python code strings so users can automate tasks and extend Dia functionality with Python.

## Acceptance Criteria

1. ✅ Can execute Python script from file path via `ExecuteScript(const char* scriptPath)`
2. ✅ Can execute Python code from string via `ExecuteString(const char* pythonCode)`
3. ✅ Returns exit code 0 on success, non-zero on error
4. ✅ Returns error code 3 if Python not initialized
5. ✅ Returns error code 2 if script file not found
6. ✅ Returns error code 4 if Python syntax error
7. ✅ Returns error code 5 if Python runtime exception
8. ✅ Fires `OnScriptExecuting` event before execution
9. ✅ Fires `OnScriptExecuted` event after execution (with exit code and duration)
10. ✅ Fires `OnPythonError` event if Python exception occurs
11. ✅ Python exceptions are logged with traceback
12. ✅ Script execution respects configured sys.path (External/Python/)
13. ✅ `ExecuteScript` validates file path before execution
14. ✅ Scripts can accept command-line style arguments
15. ✅ stdout/stderr redirection is supported
16. ✅ Both synchronous and asynchronous execution modes supported

## API Design

### Public Functions

```cpp
namespace Dia::Python {
    // Execute Python script file (synchronous)
    // Parameters:
    //   scriptPath - Path to .py file
    //   args - Optional arguments passed to script (array of strings)
    //   argCount - Number of arguments in args array
    // Returns: exit code (0 = success, non-zero = error)
    // Pre-condition: Python is initialized
    int ExecuteScript(const char* scriptPath, const char** args = nullptr, int argCount = 0);
    
    // Execute Python code string (synchronous)
    // Parameters:
    //   pythonCode - Python code to execute
    // Returns: exit code (0 = success, non-zero = error)
    // Pre-condition: Python is initialized
    int ExecuteString(const char* pythonCode);
    
    // Execute Python script file (asynchronous)
    // Parameters:
    //   scriptPath - Path to .py file
    //   args - Optional arguments
    //   argCount - Number of arguments
    //   callback - Called when script completes (on same thread)
    // Returns: immediately with task ID (0 = error starting script)
    // Pre-condition: Python is initialized
    int ExecuteScriptAsync(
        const char* scriptPath, 
        const char** args,
        int argCount,
        ScriptCompletionCallback callback
    );
    
    // Execute Python code string (asynchronous)
    // Parameters:
    //   pythonCode - Python code to execute
    //   callback - Called when execution completes
    // Returns: immediately with task ID (0 = error starting execution)
    int ExecuteStringAsync(
        const char* pythonCode,
        ScriptCompletionCallback callback
    );
    
    // Callback signature for async execution
    using ScriptCompletionCallback = std::function<void(int exitCode, float duration)>;
    
    // Redirect Python stdout/stderr
    void RedirectOutput(
        OutputCallback stdoutCallback,
        OutputCallback stderrCallback
    );
    
    // Restore Python stdout/stderr to default
    void RestoreOutput();
    
    // Cancel a specific async task
    // Returns: true if task was cancelled, false if task not found or already completed
    bool CancelTask(int taskId);
    
    // Cancel all running async tasks
    // Returns: number of tasks cancelled
    int CancelAllTasks();
    
    // Callback signature for output redirection
    using OutputCallback = std::function<void(const char* text)>;
}
```

### Events

```cpp
// Fired before script execution begins
void OnScriptExecuting(const char* scriptPath);

// Fired after script execution completes
void OnScriptExecuted(const char* scriptPath, int exitCode, float duration);

// Fired when Python exception occurs
void OnPythonError(const char* errorType, const char* errorMessage);
```

### Error Codes

```cpp
namespace Dia::Python {
    enum class ScriptError {
        Success = 0,
        GeneralError = 1,
        FileNotFound = 2,
        NotInitialized = 3,
        SyntaxError = 4,
        RuntimeException = 5
    };
}
```

## Data Models

### Internal State

```cpp
namespace Dia::Python {
    namespace Internal {
        // Async execution task
        struct AsyncTask {
            int taskId;
            std::string scriptPath;
            std::string code;
            ScriptCompletionCallback callback;
            bool isRunning;
        };
        
        // Output redirection state
        struct OutputRedirection {
            OutputCallback stdoutCallback = nullptr;
            OutputCallback stderrCallback = nullptr;
            bool isRedirected = false;
        };
        
        static DynamicArrayC<AsyncTask> gAsyncTasks;
        static OutputRedirection gOutputRedirection;
        static int gNextTaskId = 1;
    }
}
```

## Implementation Tasks

1. **Implement ExecuteScript() (synchronous)**
   - Check if Python initialized (return error code 3 if not)
   - Validate file exists using DiaCore/FilePath (return error code 2 if not)
   - Fire `OnScriptExecuting` event
   - Set sys.argv to script path + args (if provided)
   - Use pybind11 `py::eval_file()` to execute script
   - Capture Python exceptions and convert to error codes
   - Log exceptions with traceback
   - Fire `OnScriptExecuted` event with exit code and duration
   - Return exit code

2. **Implement ExecuteString() (synchronous)**
   - Check if Python initialized (return error code 3 if not)
   - Fire `OnScriptExecuting` event (scriptPath = "<string>")
   - Use pybind11 `py::exec()` to execute code
   - Capture Python exceptions and convert to error codes
   - Log exceptions with traceback
   - Fire `OnScriptExecuted` event
   - Return exit code

3. **Implement ExecuteScriptAsync()**
   - Validate script path and Python state
   - Create AsyncTask and assign task ID
   - Launch script execution on separate thread (std::thread)
   - Return task ID immediately
   - On completion, invoke callback on main thread (via event queue)

4. **Implement ExecuteStringAsync()**
   - Similar to ExecuteScriptAsync but for code strings
   - Execute on separate thread
   - Callback on completion

5. **Implement RedirectOutput()**
   - Override Python sys.stdout.write and sys.stderr.write
   - Route output to provided callbacks
   - Store redirection state

6. **Implement RestoreOutput()**
   - Restore Python sys.stdout and sys.stderr to defaults
   - Clear redirection state

7. **Add exception handling**
   - Wrap pybind11 calls in try/catch
   - Convert pybind11::error_already_set to error codes
   - Extract Python traceback and log via DiaCore
   - Fire OnPythonError event with details

8. **Write unit tests**
   - Test: ExecuteScript with valid .py file
   - Test: ExecuteScript with missing file (error code 2)
   - Test: ExecuteScript when not initialized (error code 3)
   - Test: ExecuteString with valid Python code
   - Test: ExecuteString with syntax error (error code 4)
   - Test: ExecuteString with runtime exception (error code 5)
   - Test: Script arguments passed correctly to sys.argv
   - Test: ExecuteScriptAsync completes and calls callback
   - Test: ExecuteStringAsync completes and calls callback
   - Test: RedirectOutput captures stdout
   - Test: RedirectOutput captures stderr
   - Test: RestoreOutput returns to default
   - Test: Events fire correctly (OnScriptExecuting, OnScriptExecuted, OnPythonError)

## Files Affected

### Modified Files
- `Dia/DiaPython/DiaPython.h` - Add ExecuteScript/ExecuteString declarations
- `Dia/DiaPython/DiaPython.cpp` - Implement script execution functions
- `Dia/DiaPython/DiaPythonInternal.h` - Add async task management and output redirection

### New Files
- `Cluiche/Tests/UnitTests/DiaPythonScriptTests.cpp` - Unit tests for script execution
- `Cluiche/Tests/TestScripts/test_hello.py` - Test script: print("Hello, World!")
- `Cluiche/Tests/TestScripts/test_args.py` - Test script: print args
- `Cluiche/Tests/TestScripts/test_error.py` - Test script: raise exception
- `Cluiche/Tests/TestScripts/test_syntax_error.py` - Test script: invalid syntax

## Dependencies

### Dia Systems
- **interpreter-lifecycle** - Python must be initialized before executing scripts
- **DiaCore/Core** - Logging, timing (duration measurement)
- **DiaCore/FilePath** - File path validation
- **DiaCore/Architecture/Observer** - Event emission
- **DiaCore/Containers** - DynamicArrayC for async task queue

### External
- **pybind11** - `py::eval_file()`, `py::exec()`, exception handling
- **std::thread** - For async execution

## Open Questions

1. Should async execution use a thread pool or spawn new threads per task?
2. Should there be a limit on concurrent async script executions?
3. How should async tasks be cancelled (if at all)?
4. Should RedirectOutput be per-script or global?
5. Should we capture Python warnings in addition to errors?

## Binding Decisions Compliance

| ID | Decision Summary | Compliance |
|----|------------------|------------|
| PD-001 | Use StringCRC for entity/component IDs | **Compliant** - No entity/component IDs in this feature. Script paths and code are const char* (configuration/input data). |
| PD-004 | No STL containers in public APIs | **Compliant** - Public API uses only primitives (int, const char*, const char**, callbacks). DynamicArrayC used internally for async tasks. |
| PD-006 | Visual Studio project files are source of truth | **Compliant** - No build system changes; implementation in existing DiaPython.vcxproj. |
| AD-001 | Module system with YAML frontmatter documentation | **Compliant** - Updates to existing dia.python.architecture.module.md to include script execution API. |
| AD-002 | No STL containers in public APIs | **Compliant** - Reinforces PD-004. Public API is STL-free. std::thread used internally only. |
| AD-003 | Namespace convention: `Dia::<Module>::` | **Compliant** - All code in `Dia::Python::` namespace. |
| SD-001 | Wrap pybind11, don't expose it | **Compliant** - pybind11 types (py::eval_file, py::exec) used only in .cpp file, never in public headers. |
| SD-002 | Python interpreter is singleton | **Compliant** - Script execution uses the single global interpreter initialized by interpreter-lifecycle. |
| SD-003 | Python interpreter is single-threaded (GIL), but script execution can be sync or async | **Compliant** - Async execution runs scripts on worker threads. GIL ensures only one Python thread executes at a time, maintaining thread safety. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Open Questions | Should async execution use a thread pool or spawn new threads? | Spawn new thread per task (unlimited concurrency, higher overhead but simpler) |
| 2 | Open Questions | Should there be a limit on concurrent async script executions? | Yes, hard limit of 16 concurrent scripts (prevents thread explosion) |
| 3 | Open Questions | How should async tasks be cancelled (if at all)? | Both `CancelTask(taskId)` and `CancelAllTasks()` - full cancellation support |
| 4 | Open Questions | Should RedirectOutput be per-script or global? | Global - affects all script executions (simpler implementation, consistent behavior) |
| 5 | Open Questions | Should we capture Python warnings in addition to errors? | Configurable at Initialize - add `captureWarnings` parameter to Initialize() |
| 6 | Implementation | How should async tasks notify main thread (event queue, polling, callbacks)? | Callback posted to main thread - worker thread posts callback to main thread (safest, aligns with FD-005) |
| 7 | API Design | Should ExecuteScript return more than just exit code (e.g., return value from script)? | Exit code only (simple - scripts use print() or sys.exit(code) to communicate) |
| 8 | Error Handling | Should we distinguish between different types of exceptions (ValueError, TypeError, etc.)? | Log exception type in error message but return same error code 5 for all runtime exceptions (middle ground) |
| 9 | Testing | Should we test with Python scripts that import modules from External/Python/? | No - only test standalone scripts without imports (simpler, faster tests) |
| 10 | Dependencies | Does async execution conflict with SD-003 (synchronous only)? Need to update SD-003? | SD-003 already updated to allow async script execution while maintaining single-threaded Python interpreter (GIL) |

## Decisions

| ID | Decision | Rationale | Status |
|----|----------|-----------|--------|
| FD-001 | Provide both sync and async execution APIs | Different use cases: sync for immediate results, async for long-running scripts; gives flexibility | Proposed |
| FD-002 | Script arguments passed as const char** array | Simple, matches C argc/argv pattern; easy to convert to Python sys.argv | Proposed |
| FD-003 | Output redirection is opt-in via RedirectOutput() | Default behavior preserves Python stdout/stderr; redirection when needed (e.g., capturing build tool output) | Proposed |
| FD-004 | No timeout mechanism for infinite loops | User responsibility; adding timeout adds complexity and thread cancellation issues | Proposed |
| FD-005 | Async callbacks invoked on main thread | Safer than callback on worker thread; avoids threading issues in callback code | Proposed |

## Status

`Approved` - Ready for implementation
