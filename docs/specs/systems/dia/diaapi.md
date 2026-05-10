# System Spec: DiaAPI

## Parent Application
@docs/specs/applications/dia.md

## Purpose

DiaAPI is a plugin-based command registration and execution API framework - providing command registration, argument parsing, and execution infrastructure. It functions as a runtime API (static library), not a command-line executable. Other Dia modules register commands programmatically via the framework's public API, which can then be invoked through Python bindings (`dia_api.command_name()`) or directly in C++ (`Dia::API::ExecuteCommand(...)`).

## Responsibilities

- Provide command registration API for other modules to add commands
- Parse command-line-style arguments (supports `--flag`, `--key=value` syntax)
- Execute registered commands with proper error handling and exit codes
- Emit lifecycle events (command registration, execution, errors)
- Generate help text and command discovery
- Expose commands to Python runtime via automatic bindings

## Public Interfaces

### Endpoints / APIs

**Command Registration:**
```cpp
namespace Dia::API {
    // Register a new command
    void RegisterCommand(
        const StringCRC& name,
        const char* description,
        CommandCallback callback
    );
    
    // Callback signature for command handlers
    using CommandCallback = std::function<int(const CommandArgs& args)>;
    
    // Get all registered commands (for help/discovery)
    const CommandRegistry& ListCommands();
}
```

**Command Execution:**
```cpp
namespace Dia::API {
    // Execute a registered command by name
    int ExecuteCommand(
        const StringCRC& commandName,
        const CommandArgs& args
    );
    
    // Main entry point (called from main())
    int RunCLI(int argc, char* argv[]);
}
```

**Command Arguments:**
```cpp
struct CommandArgs {
    DynamicArrayC<const char*> positionalArgs;
    HashTable<StringCRC, const char*> namedArgs;  // --key=value
    HashTable<StringCRC, bool> flags;              // --flag
};
```

### Events Emitted

- `OnCommandRegistered(commandName, description)` - When a new command is added to the registry
- `OnCommandExecuting(commandName, args)` - Before command execution begins
- `OnCommandExecuted(commandName, exitCode, duration)` - After command completes
- `OnCommandError(commandName, errorMessage, exitCode)` - When command fails or throws
- `OnHelpRequested()` - When user requests help (--help or no args)

### Data Contracts

**Exit Codes:**
- `0` - Success
- `1` - General error
- `2` - Invalid arguments
- `3` - Command not found
- `4+` - Command-specific error codes

**CLI Format:**
```bash
DiaAPI <command> [args...]
DiaAPI --help                    # Show all commands
DiaAPI <command> --help          # Show command-specific help
```

## Features

| Feature | Description | Spec | Status |
|---------|-------------|------|--------|
| command-registry | Core command registration and discovery system | [command-registry.md](../../features/dia/diaapi/command-registry.md) | Done |
| cli-parser | Argument parsing (positional, named, flags) | [cli-parser.md](../../features/dia/diaapi/cli-parser.md) | Done |
| help-system | Auto-generated help text from registered commands | [help-system.md](../../features/dia/diaapi/help-system.md) | Done |
| event-system | Lifecycle events for command execution | [event-system.md](../../features/dia/diaapi/event-system.md) | Done |
| python-bindings | Automatic Python exposure of all registered commands via DiaPython | [python-bindings.md](../../features/dia/diaapi/python-bindings.md) | Done |

## Platform Primitives Used

- **DiaCore/Containers** - DynamicArrayC, HashTable for command storage
- **DiaCore/CRC** - StringCRC for command name hashing
- **DiaCore/Core** - Assertions, logging
- **DiaCore/Architecture/Observer** - Event emission (OnCommand* events)

## Dependencies on Other Systems

**Required:**
- **DiaPython** - To expose all registered commands to Python automatically. DiaAPI depends on DiaPython to create Python bindings without requiring pybind11 headers.

**Dependents:**
- Future: Other Dia systems (DiaEditor, asset tools) will depend on DiaAPI to register commands, which automatically become available in C++, CLI, and Python

## Out of Scope

- **GUI interface** - CLI only; graphical tools are future work
- **Interactive prompts** - Commands are non-interactive for scriptability
- **Complex argument parsing** - No subcommands, option chains, or advanced parsing (keep simple)
- **Command aliasing/shortcuts** - Commands use full names only
- **Configuration files** - Commands receive arguments only (no .diaclirc)
- **Parallel execution** - Commands run sequentially

## Decisions

| ID | Decision | Rationale | Scope | Status | Binding |
|----|----------|-----------|-------|--------|---------|
| SD-001 | Commands identified by StringCRC | Efficient lookup; compile-time validation; consistent with platform | All features | Accepted | Yes |
| SD-002 | Exit codes follow Unix conventions (0=success) | Standard for shell scripting and CI/CD; enables automation | All features | Accepted | Yes |
| SD-003 | Commands are stateless (no persistent state between invocations) | Simplifies implementation; predictable behavior; easier testing | All features | Accepted | Yes |
| SD-004 | No interactive prompts (headless by default) | Enables scripting and automation; prevents CI/CD hangs | All features | Accepted | Yes |
| SD-005 | Registered commands stored in global registry (Singleton) | Simple access pattern; matches engine singleton usage | This system | Accepted | Yes |
| SD-006 | DiaAPI depends on DiaPython for automatic Python exposure | Commands registered once are available in C++, CLI, and Python; no pybind11 in DiaAPI headers | All features | Accepted | Yes |

**Status values:** `Proposed` · `Accepted` · `Rejected` · `Superseded`
**Binding:** `Yes` = enforced constraint on all features in this system · `No` = guidance only

## Inherited Binding Decisions

| ID | Source | Decision | Implication for this system |
|----|--------|----------|----------------------------|
| PD-001 | Platform | Use StringCRC for all entity/component IDs | Command names are StringCRC, not raw strings. Enables compile-time validation and efficient lookup. |
| PD-004 | Platform | No STL containers in public APIs | Use DynamicArrayC, HashTable instead of std::vector, std::map in CommandArgs and CommandRegistry. |
| PD-006 | Platform | Visual Studio project files are source of truth | DiaAPI must be a .vcxproj with proper filters. Cannot use CMake or other build systems. |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | Create dia.cli.architecture.module.md with public API, dependencies, responsibilities. |
| AD-002 | Dia App | No STL containers in public APIs | Reinforces PD-004 - use Dia containers throughout. |
| AD-003 | Dia App | Namespace convention: `Dia::<Module>::` | All code in `Dia::API::` namespace. |

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Purpose | Should DiaAPI be an executable or a library? | Library with a thin main.cpp wrapper. Allows other tools to link against it and reuse command registration. |
| 2 | Public Interfaces | Should commands support subcommands (e.g., `DiaAPI asset compile`)? | No - keep simple. Use distinct command names instead: `compile-asset`, `validate-asset`. Subcommands add complexity. |
| 3 | Dependencies | Does DiaAPI need DiaApplicationFlow's Module system? | No - DiaAPI is a build tool, not a runtime application. It doesn't use ProcessingUnits/Phases. |
| 4 | Events | Should events use Observer pattern or callbacks? | Observer pattern - consistent with DiaCore/Architecture/Observer. Allows multiple listeners. |
| 5 | Scope | Should asset compilation be in DiaAPI or separate system? | Start in DiaAPI via registered commands. If complexity grows, refactor to DiaAssetPipeline system later. |
| 6 | Out of Scope | Should we support piping/stdin for CI/CD scenarios? | TBD - defer until concrete use case. Start with argv only. |
| 7 | Dependencies | How does DiaAPI expose commands to Python? | DiaAPI depends on DiaPython and uses its API (CreateModule, AddFunction) to auto-expose all registered commands. DiaAPI never includes pybind11 headers. |

## Status

`Done` - All features implemented
