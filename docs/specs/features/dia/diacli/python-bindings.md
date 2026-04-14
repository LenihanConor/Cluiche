# Feature Spec: python-bindings

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diacli.md | **python-bindings** |

**Status:** `Approved`

---

## Problem Statement

Automatically expose all registered CLI commands to Python so users can call commands programmatically from Python scripts without writing separate Python bindings for each command.

---

## Solution Overview

The **python-bindings** feature uses DiaPython's public API (CreateModule, AddFunction) to automatically create Python bindings for all commands registered in the command-registry. When DiaCLI initializes, it enumerates all registered commands and creates a `dia_cli` Python module where each command becomes a callable Python function. This enables automation, testing, and scripting without maintaining duplicate bindings.

### Key Design Points

1. **DiaPython integration** - Uses DiaPython public API, never includes pybind11 headers
2. **Automatic binding** - All registered commands automatically callable from Python
3. **Single module** - All commands in `dia_cli` Python module
4. **Argument conversion** - Python kwargs → CommandArgs, Python return → exit code
5. **On-demand initialization** - Python bindings created during DiaCLI::Initialize()

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | Create `dia_cli` Python module during DiaCLI::Initialize() | Unit test: Initialize DiaCLI, import dia_cli in Python, verify success |
| AC2 | Register all CLI commands as Python functions in `dia_cli` module | Unit test: Register 3 commands, verify all 3 callable from Python |
| AC3 | Convert Python positional args to CommandArgs.positionalArgs | Unit test: Call `dia_cli.build("file.txt")` from Python, verify C++ receives positional arg |
| AC4 | Convert Python keyword args to CommandArgs.namedArgs | Unit test: Call `dia_cli.build(format="gltf")` from Python, verify C++ receives named arg |
| AC5 | Convert Python boolean kwargs to CommandArgs.flags | Unit test: Call `dia_cli.build(verbose=True)` from Python, verify C++ receives flag |
| AC6 | Return Python int from command exit code | Unit test: Command returns 0, verify Python call returns 0 |
| AC7 | Never include pybind11 headers in DiaCLI code | Code review: Verify no #include <pybind11/*> in DiaCLI |
| AC8 | Commands registered before Initialize() are exposed to Python | Unit test: Register command, call Initialize(), verify callable from Python |

---

## Public API

### Functions

```cpp
namespace Dia::CLI {

// Initialize Python bindings for all registered commands
// Must be called after DiaPython::Initialize() and command registration
void InitializePythonBindings();

}
```

---

## Implementation Notes

### Binding Creation Algorithm

```
1. Check DiaPython::IsInitialized() - return early if Python not available
2. Create module: DiaPython::CreateModule("dia_cli")
3. Enumerate all commands: ListCommands()
4. For each command:
   a. Create wrapper callback that:
      - Converts PythonArgs → CommandArgs
      - Calls original command callback
      - Returns exit code as PythonObject
   b. Register with DiaPython::AddFunction(module, command.name, wrapper, command.description)
5. Log success
```

### Argument Conversion

**Python → C++ (PythonArgs → CommandArgs):**
```cpp
CommandArgs ConvertPythonArgs(const Dia::Python::PythonArgs& pyArgs) {
    CommandArgs cppArgs;
    
    // Positional args
    for (int i = 0; i < pyArgs.GetCount(); i++) {
        PythonObject arg = pyArgs.GetArg(i);
        if (arg.IsString()) {
            const char* str = DiaPython::ToString(arg);
            cppArgs.positionalArgs.Add(str);
        }
    }
    
    // Named args and flags (from kwargs)
    // DiaPython doesn't expose kwargs directly - assume all args are positional
    // OR: use special syntax like `build("file.txt", "--format=gltf", "--verbose")`
    // Parse strings starting with "--" into named/flags
    
    return cppArgs;
}
```

**C++ → Python (int → PythonObject):**
```cpp
PythonObject exitCode = DiaPython::ToPython(commandReturnValue);
return exitCode;
```

### Wrapper Function Template

```cpp
namespace Dia::CLI::Internal {

PythonObject WrapCommandForPython(
    const CommandInfo* cmdInfo,
    const Dia::Python::PythonArgs& pyArgs)
{
    // Convert Python args to CommandArgs
    CommandArgs cppArgs = ConvertPythonArgs(pyArgs);
    
    // Execute command callback
    int exitCode = cmdInfo->callback(cppArgs);
    
    // Convert exit code to Python
    return DiaPython::ToPython(exitCode);
}

}
```

### Python Argument Syntax

Since DiaPython doesn't natively support Python kwargs, use string-based argument passing:

**Python call:**
```python
import dia_cli

# Positional args
dia_cli.compile_asset("model.fbx")

# Named args as strings
dia_cli.compile_asset("model.fbx", "--format=gltf", "--verbose")

# Or use helper function
dia_cli.compile_asset("model.fbx", format="gltf", verbose=True)  # If we add kwargs support
```

**Simplest solution:** Parse argv-style strings from Python args:
```python
dia_cli.compile_asset("model.fbx", "--format=gltf", "--verbose")
```

Use cli-parser to parse these strings into CommandArgs.

---

## Dependencies

### Required Modules
- **DiaPython** - CreateModule, AddFunction, PythonObject, PythonArgs, ToPython, ToString
- **command-registry** - ListCommands(), CommandInfo, CommandArgs
- **cli-parser** - ParseArguments (to parse argv-style strings from Python)
- **DiaCore/CRC** - StringCRC

### Dependent Features
- **command-registry** - Provides commands to bind
- **DiaPython** - Must be initialized before InitializePythonBindings()

---

## Testing Strategy

### Unit Tests (Cluiche/Tests/GoogleTests/CLI/TestPythonBindings.cpp)

1. **Module creation**
   - Call InitializePythonBindings()
   - Execute Python: `import dia_cli`
   - Verify no exception

2. **Command binding**
   - Register command "test-cmd"
   - Call InitializePythonBindings()
   - Execute Python: `dia_cli.test_cmd()`
   - Verify command callback called

3. **Positional args**
   - Register command that expects 2 positional args
   - Execute Python: `dia_cli.test_cmd("arg1", "arg2")`
   - Verify C++ receives ["arg1", "arg2"]

4. **Named args (argv-style)**
   - Register command
   - Execute Python: `dia_cli.test_cmd("file.txt", "--key=value")`
   - Verify C++ receives positionalArgs=["file.txt"], namedArgs["key"]="value"

5. **Flags (argv-style)**
   - Execute Python: `dia_cli.test_cmd("--verbose")`
   - Verify C++ receives flags["verbose"]=true

6. **Return value**
   - Command callback returns 42
   - Execute Python: `result = dia_cli.test_cmd()`
   - Verify result == 42

7. **Multiple commands**
   - Register 3 commands
   - Call InitializePythonBindings()
   - Verify all 3 callable from Python

8. **Pre-init registration**
   - Register command before DiaCLI::Initialize()
   - Call Initialize(), InitializePythonBindings()
   - Verify command callable from Python

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | Use StringCRC for all entity/component IDs | ✅ **Compliant** - Command names are StringCRC. |
| PD-004 | Platform | No STL containers in public APIs | ✅ **Compliant** - Uses CommandArgs which uses DiaCore containers. |
| PD-006 | Platform | Visual Studio project files are source of truth | ✅ **Compliant** - Part of DiaCLI.vcxproj. |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | ✅ **Compliant** - Part of DiaCLI module documentation. |
| AD-002 | Dia App | No STL containers in public APIs | ✅ **Compliant** - Reinforces PD-004. |
| AD-003 | Dia App | Namespace convention: `Dia::<Module>::` | ✅ **Compliant** - All code in `Dia::CLI::` namespace. |
| SD-006 | DiaCLI System | DiaCLI depends on DiaPython for automatic Python exposure | ✅ **Compliant** - Uses DiaPython public API (CreateModule, AddFunction). Never includes pybind11. |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | API Design | Should InitializePythonBindings() be called automatically in Initialize()? | ✅ No - separate function. Allows DiaCLI to work without Python. Caller decides when to initialize bindings. |
| 2 | Argument Syntax | How should Python pass named args and flags? | ✅ Argv-style strings: `dia_cli.cmd("file.txt", "--key=value", "--flag")`. Simple, reuses cli-parser. |
| 3 | Error Handling | What if command throws exception? | ✅ Catch in wrapper, log error, return error code to Python. No Python exceptions. |
| 4 | Initialization Order | What if InitializePythonBindings() called before DiaPython::Initialize()? | ✅ Check DiaPython::IsInitialized(), log warning and return early if false. |
| 5 | Command Registration | What if commands registered after InitializePythonBindings()? | ✅ Those commands won't be exposed. Document that InitializePythonBindings() must be called after all registrations. |

---

## Open Questions

All resolved:

1. ✅ **Kwargs support:** Use argv-style strings (--key=value) instead of Python kwargs. Simpler, reuses cli-parser, no DiaPython kwargs needed.
2. ✅ **Module name:** Use "dia_cli" (lowercase, snake_case) per Python conventions. Not "DiaCLI" or "dia.cli".
3. ✅ **Docstrings:** Pass CommandInfo.description to DiaPython::AddFunction() so Python help() shows command descriptions.

---

## Implementation Plan

### Phase 1: Module Creation (1 day)
- Implement InitializePythonBindings()
- Create dia_cli module with DiaPython::CreateModule()
- Check DiaPython::IsInitialized()
- Unit tests for module creation

### Phase 2: Command Binding (2 days)
- Enumerate commands with ListCommands()
- Create wrapper function for each command
- Register with DiaPython::AddFunction()
- Unit tests for basic command binding

### Phase 3: Argument Conversion (2 days)
- Implement ConvertPythonArgs()
- Parse argv-style strings with cli-parser
- Convert to CommandArgs
- Unit tests for positional, named, flags

### Phase 4: Integration & Testing (1 day)
- Integration tests with DiaPython
- Test error handling (exceptions, invalid args)
- Test multiple commands
- Documentation

**Total Estimate:** 6 days

---

## Examples

### Example 1: Initialize Python Bindings

```cpp
#include <DiaCLI/DiaCLI.h>
#include <DiaPython/DiaPython.h>

int main(int argc, char* argv[]) {
    // Initialize Python
    Dia::Python::Initialize("External/Python311-Win32/", "External/Python/", false);
    
    // Initialize DiaCLI
    Dia::CLI::Initialize();
    
    // Register commands (or they can be registered before Initialize)
    RegisterAllCommands();
    
    // Create Python bindings for all registered commands
    Dia::CLI::InitializePythonBindings();
    
    // Now Python can call: import dia_cli; dia_cli.build("file.txt")
    
    Dia::CLI::Shutdown();
    Dia::Python::Shutdown();
    return 0;
}
```

### Example 2: Python Script Using CLI Commands

```python
# my_build_script.py
import dia_cli

# Call build command
result = dia_cli.compile_asset("models/character.fbx", "--format=gltf", "--verbose")
if result != 0:
    print(f"Build failed with code {result}")
    exit(1)

# Call validation
result = dia_cli.validate_asset("models/character.fbx")
if result != 0:
    print("Validation failed")
    exit(1)

print("Build successful!")
```

### Example 3: Custom Command with Python Binding

```cpp
#include <DiaCLI/CommandRegistry/CommandRegistry.h>

using namespace Dia::CLI;
using namespace Dia::Core;

int MyCustomCommand(const CommandArgs& args) {
    printf("MyCustomCommand called with %d args\n", args.positionalArgs.Size());
    
    if (args.flags.ContainsKey(StringCRC("verbose"))) {
        printf("Verbose mode enabled\n");
    }
    
    return 0;  // Success
}

void RegisterMyCommands() {
    CommandInfo info;
    info.name = StringCRC("my-command");
    info.description = "My custom command for testing";
    info.category = StringCRC("test");
    info.owner = "MySystem";
    info.version = "1.0.0";
    info.example = "my-command file.txt --verbose";
    info.callback = MyCustomCommand;
    
    RegisterCommand(info);
}

// After InitializePythonBindings(), Python can call:
// import dia_cli
// dia_cli.my_command("file.txt", "--verbose")
```

---

## Status

`Approved` - Ready for implementation
