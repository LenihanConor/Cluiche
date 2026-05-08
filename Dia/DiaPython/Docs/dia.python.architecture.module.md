---
schema: dia.module.v1
module_id: dia.python
name: DiaPython
owner_team: Core
layer: platform
status: active
maturity: dev

path: Dia/DiaPython
language: cpp
parent_module_id: dia.core

summary: >
  Python embedding framework that wraps pybind11 with a clean C++ API for scripting and automation.

intent: >
  DiaPython enables Python scripting in the Dia engine without exposing pybind11 details to client code.
  It provides interpreter lifecycle management, type conversion, module creation, and script execution.
  Designed as agnostic infrastructure that other systems (DiaCLI, etc.) build upon.

responsibilities:
  - Initialize and manage the Python interpreter (singleton, single-threaded GIL)
  - Convert between C++ and Python types (int, float, bool, string)
  - Register C++ functions as Python modules
  - Execute Python scripts and code strings (synchronous only)
  - Handle Python exceptions and convert to error codes
  - Redirect Python stdout/stderr for output capture
  - Provide opaque handles (PythonObject, Module) that hide pybind11

non_responsibilities:
  - Does not know about CLI commands, graphics, or specific Dia subsystems
  - Does not manage Python package installation (uses bundled Python runtime)
  - Does not provide multi-threaded Python execution (GIL enforces single-threaded)
  - Does not handle file I/O beyond script execution (client code handles files)

dependent_modules: []

public_api:
  headers:
    - Dia/DiaPython/DiaPython.h
    - Dia/DiaPython/Lifecycle/Lifecycle.h
    - Dia/DiaPython/TypeConversion/PythonObject.h
    - Dia/DiaPython/TypeConversion/Conversion.h
    - Dia/DiaPython/ErrorHandling/Error.h
    - Dia/DiaPython/Module/Module.h
    - Dia/DiaPython/ScriptExecution/Script.h
  namespaces:
    - Dia::Python
  entry_points:
    - Initialize
    - Shutdown
    - IsInitialized
    - ToPython
    - ToInt
    - ToFloat
    - ToBool
    - ToString
    - CreateModule
    - GetModule
    - AddFunction
    - ExecuteScript
    - ExecuteString
    - RedirectOutput
    - RestoreOutput

dependencies:
  required:
    - dia.core.containers
    - dia.core.logging
    - dia.core.threading
    - dia.core.crc
    - pybind11 (external)
    - Python 3.11 runtime (external)
  forbidden: []
---

# DiaPython Module

Python embedding framework for the Dia engine.

## Overview

DiaPython wraps pybind11 to provide a clean, C++-native API for Python scripting. It enables:
- **Python scripting** in Dia applications
- **Module registration** so C++ code can expose functions to Python
- **Type conversion** between C++ and Python with automatic coercion
- **Script execution** (sync/async) with error handling
- **Output redirection** for capturing Python stdout/stderr

## System Spec

See: `docs/specs/systems/dia/diapython.md`

## Feature Specs

Implemented features:
1. **interpreter-lifecycle** - `docs/specs/features/dia/diapython/interpreter-lifecycle.md`
2. **type-conversion** - `docs/specs/features/dia/diapython/type-conversion.md`
3. **error-handling** - `docs/specs/features/dia/diapython/error-handling.md`
4. **module-api** - `docs/specs/features/dia/diapython/module-api.md`
5. **script-execution** - `docs/specs/features/dia/diapython/script-execution.md`

## Binding Decisions

| ID | Decision | Status |
|----|----------|--------|
| SD-001 | Wrap pybind11, don't expose it | ✅ Compliant - All public headers use opaque handles (PythonObject, Module, PythonArgs). pybind11 types only in .cpp and DiaPythonInternal.h |
| SD-002 | Python interpreter is singleton | ✅ Compliant - Single global `InterpreterState gState`. Initialize() is idempotent. |
| SD-003 | Python interpreter is single-threaded (GIL) | ✅ Compliant - GIL ensures thread safety. Script execution is synchronous only. |
| SD-004 | Opaque handle types (Module, PythonObject, PythonArgs) | ✅ Compliant - All handles use void* mImpl or reinterpret_cast. Internal pybind11 types hidden. |
| SD-005 | Python exceptions converted to exit codes | ✅ Compliant - ConvertException() maps py::error_already_set to ErrorCode enum. No pybind11 exceptions in public API. |

## Public API

### Lifecycle Management

```cpp
namespace Dia::Python {
    // Initialize Python interpreter
    // Parameters:
    //   pythonHome - Path to Python runtime (e.g., "External/Python311/")
    //   modulePath - Path to custom modules (e.g., "External/Python/")
    //   captureWarnings - Enable Python warning capture
    // Returns: true on success, false on failure
    bool Initialize(const char* pythonHome, const char* modulePath, bool captureWarnings = false);
    
    // Shutdown Python interpreter
    void Shutdown();
    
    // Check if Python is initialized
    bool IsInitialized();
}
```

**Example:**
```cpp
#include <DiaPython/DiaPython.h>

bool result = Dia::Python::Initialize("External/Python311/", "External/Python/", false);
if (!result) {
    // Handle initialization failure
}

// ... use Python ...

Dia::Python::Shutdown();
```

### Type Conversion

```cpp
namespace Dia::Python {
    // C++ → Python conversion
    PythonObject ToPython(int value);
    PythonObject ToPython(float value);
    PythonObject ToPython(bool value);
    PythonObject ToPython(const char* str);
    
    // Python → C++ conversion
    int ToInt(const PythonObject& obj);
    float ToFloat(const PythonObject& obj);
    bool ToBool(const PythonObject& obj);
    const char* ToString(const PythonObject& obj);
    
    // PythonObject wrapper
    class PythonObject {
    public:
        PythonObject();  // Creates None
        bool IsNone() const;
        bool IsValid() const;
        bool IsInt() const;
        bool IsFloat() const;
        bool IsBool() const;
        bool IsString() const;
    };
    
    // Function arguments wrapper
    class PythonArgs {
    public:
        int GetCount() const;
        PythonObject GetArg(int index) const;
    };
}
```

**Example:**
```cpp
using namespace Dia::Python;

// C++ to Python
PythonObject intObj = ToPython(42);
PythonObject floatObj = ToPython(3.14f);
PythonObject strObj = ToPython("Hello");

// Python to C++
int value = ToInt(intObj);
float pi = ToFloat(floatObj);
const char* text = ToString(strObj);

// Type queries
if (intObj.IsInt()) {
    // Handle integer
}
```

### Module API

```cpp
namespace Dia::Python {
    // Opaque module handle
    class Module {
        Module(const Module&) = delete;  // Non-copyable
    };
    
    // Function callback signature
    using PythonCallback = std::function<PythonObject(const PythonArgs& args)>;
    
    // Create Python module
    Module* CreateModule(const char* name);
    
    // Get existing module
    Module* GetModule(const char* name);
    
    // Register C++ function
    void AddFunction(
        Module* module,
        const char* functionName,
        PythonCallback callback,
        const char* docstring = nullptr
    );
    
    // Register overloaded function
    void AddFunctionOverload(
        Module* module,
        const char* functionName,
        PythonCallback callback,
        const char* signatureHint,
        const char* docstring = nullptr
    );
}
```

**Example:**
```cpp
using namespace Dia::Python;

// Define C++ function
PythonObject Add(const PythonArgs& args) {
    int a = ToInt(args.GetArg(0));
    int b = ToInt(args.GetArg(1));
    return ToPython(a + b);
}

// Create module
Module* mathModule = CreateModule("my_math");

// Register function
AddFunction(mathModule, "add", Add, "Add two numbers");

// Later, retrieve module
Module* retrieved = GetModule("my_math");
```

**Python usage:**
```python
import my_math
result = my_math.add(2, 3)  # Returns 5
help(my_math.add)  # Shows: "Add two numbers"
```

### Script Execution

**Note:** Script execution is **synchronous only**. Asynchronous execution (ExecuteScriptAsync, ExecuteStringAsync, CancelTask, CancelAllTasks) was removed to simplify the implementation and avoid GIL threading complexity. This matches DiaCLI's synchronous command model.

```cpp
namespace Dia::Python {
    // Synchronous execution
    int ExecuteScript(const char* scriptPath, const char** args = nullptr, int argCount = 0);
    int ExecuteString(const char* pythonCode);
    
    // Output redirection
    using OutputCallback = std::function<void(const char* text)>;
    
    void RedirectOutput(
        OutputCallback stdoutCallback,
        OutputCallback stderrCallback
    );
    
    void RestoreOutput();
}
```

**Example - Script Execution:**
```cpp
using namespace Dia::Python;

// Execute script file
int exitCode = ExecuteScript("scripts/test.py");
if (exitCode != 0) {
    // Handle error
}

// Execute code string
exitCode = ExecuteString("print('Hello from Python')");

// Execute with arguments
const char* args[] = { "arg1", "arg2" };
exitCode = ExecuteScript("scripts/process.py", args, 2);
```

**Example - Output Redirection:**
```cpp
using namespace Dia::Python;

std::string capturedOutput;

RedirectOutput(
    [&capturedOutput](const char* text) {
        capturedOutput += text;
    },
    nullptr  // Don't redirect stderr
);

ExecuteString("print('Captured')");

RestoreOutput();

// capturedOutput now contains "Captured"
```

### Error Handling

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

**Example:**
```cpp
using namespace Dia::Python;

int exitCode = ExecuteString("def foo(");  // Syntax error

switch (static_cast<ErrorCode>(exitCode)) {
    case ErrorCode::Success:
        // Script succeeded
        break;
    case ErrorCode::SyntaxError:
        // Python syntax error
        break;
    case ErrorCode::RuntimeException:
        // Python runtime exception
        break;
    case ErrorCode::NotInitialized:
        // Python not initialized
        break;
    default:
        // Other error
        break;
}
```

## Events (TODO: Phase 7)

Events will be implemented using DiaCore Observer pattern:

```cpp
// Fired before script execution
void OnScriptExecuting(const char* scriptPath);

// Fired after script execution
void OnScriptExecuted(const char* scriptPath, int exitCode, float duration);

// Fired when Python error occurs
void OnPythonError(const char* errorType, const char* errorMessage, const char* context);

// Fired when Python initializes
void OnPythonInitialized();

// Fired when Python shuts down
void OnPythonShutdown();
```

## Complete Usage Example

```cpp
#include <DiaPython/DiaPython.h>

using namespace Dia::Python;

// Define a C++ function callable from Python
PythonObject Greet(const PythonArgs& args) {
    if (args.GetCount() == 0) {
        return ToPython("Hello, World!");
    }
    
    const char* name = ToString(args.GetArg(0));
    std::string greeting = std::string("Hello, ") + name + "!";
    return ToPython(greeting.c_str());
}

int main() {
    // 1. Initialize Python
    if (!Initialize("External/Python311/", "External/Python/", false)) {
        printf("Failed to initialize Python\n");
        return 1;
    }
    
    // 2. Create a module and register function
    Module* greetModule = CreateModule("greetings");
    AddFunction(greetModule, "greet", Greet, "Greet someone");
    
    // 3. Execute Python code that uses the module
    int exitCode = ExecuteString(
        "import greetings\n"
        "print(greetings.greet())\n"
        "print(greetings.greet('Alice'))\n"
    );
    
    if (exitCode != 0) {
        printf("Script failed with exit code: %d\n", exitCode);
    }
    
    // 4. Execute script file
    exitCode = ExecuteScript("scripts/my_script.py");
    
    // 5. Cleanup
    Shutdown();
    
    return 0;
}
```

## Architecture Notes

### Thread Safety

- **Python GIL**: All Python code executes with the Global Interpreter Lock, ensuring thread safety
- **Single-threaded execution**: Script execution is synchronous (no async threading)
- **Module registry**: Not thread-safe - create modules before multi-threaded execution (if called from multiple threads)

### Memory Management

- **PythonObject**: Reference-counted via pybind11. Copy constructor increments refcount.
- **Modules**: Never destroyed. Created once and live for entire application lifetime.
- **Strings**: ToString() returns pointer to internal cache. Valid until PythonObject destroyed.

### Performance

- **Initialization**: Python interpreter creation takes ~50-100ms
- **Type conversion**: Minimal overhead (direct pybind11 casts)
- **Module lookup**: O(1) via std::unordered_map hash table
- **Script execution**: Performance depends on Python code complexity

### Limitations

- **No async execution**: Removed for simplicity - all scripts execute synchronously
- **No timeout**: Scripts can run indefinitely (by design)
- **Single interpreter**: Only one Python interpreter per process (SD-002)
- **No nested modules**: Module names like "dia.cli" supported, but not true nesting
- **Basic output redirection**: Current implementation is simplified

## File Structure

```
Dia/DiaPython/
├── DiaPython.h                     # Main header (includes all features)
├── DiaPythonInternal.h             # Internal state (includes pybind11)
├── Lifecycle/
│   ├── Lifecycle.h                 # Initialize, Shutdown, IsInitialized
│   └── Lifecycle.cpp
├── TypeConversion/
│   ├── PythonObject.h              # PythonObject, PythonArgs classes
│   ├── PythonObject.cpp
│   ├── Conversion.h                # ToPython, ToInt, etc.
│   └── Conversion.cpp
├── Module/
│   ├── Module.h                    # CreateModule, AddFunction
│   └── Module.cpp
├── ErrorHandling/
│   ├── Error.h                     # ErrorCode enum, error utilities
│   └── Error.cpp
├── ScriptExecution/
│   ├── Script.h                    # ExecuteScript, ExecuteString, async
│   └── Script.cpp
└── Docs/
    └── dia.python.architecture.module.md  # This file
```

## Dependencies

### External
- **pybind11** (v2.11+) - Header-only C++/Python binding library
- **Python 3.11** - Embedded runtime in `External/Python311/`

### Internal (DiaCore)
- **Containers**: DynamicArrayC, HashTable (for module registry, async tasks)
- **Logging**: Logger, DIA_LOG_* macros
- **Threading**: Mutex (for async task list)
- **CRC**: StringCRC (for module name hashing)

## Testing

Unit tests in `Cluiche/Tests/GoogleTests/Python/`:
- `TestLifecycle.cpp` - 13 tests for interpreter lifecycle
- `TestTypeConversion.cpp` - 30+ tests for type conversion
- `TestErrorHandling.cpp` - 18 tests for error handling
- `TestModule.cpp` - 25+ tests for module API
- `TestScriptExecution.cpp` - 30+ tests for script execution

Test scripts in `Cluiche/Tests/TestScripts/Python/`:
- `test_hello.py` - Simple print test
- `test_args.py` - Argument testing
- `test_error.py` - Runtime exception
- `test_syntax_error.py` - Syntax error

## Future Enhancements (Not in Current Scope)

- **Observer pattern events** - OnScriptExecuting, OnScriptExecuted, OnPythonError
- **Main thread callbacks** - Post async callbacks to event queue
- **Improved output redirection** - Line-buffered, separate stdout/stderr
- **Module unloading** - Ability to unload modules (currently permanent)
- **Python package management** - pip integration
- **Multi-interpreter support** - Multiple isolated Python interpreters (pybind11 limitation)

## References

- System spec: `docs/specs/systems/dia/diapython.md`
- Feature specs: `docs/specs/features/dia/diapython/*.md`
- pybind11 docs: https://pybind11.readthedocs.io/
