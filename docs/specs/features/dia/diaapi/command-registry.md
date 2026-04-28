# Feature Spec: command-registry

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diacli.md | **command-registry** |

**Status:** `Approved`

---

## Problem Statement

Enable any system architecturally north of DiaAPI (which is most systems) to register CLI commands into a central registry so they can be discovered, listed, and executed by the CLI framework.

---

## Solution Overview

The **command-registry** feature provides a global singleton registry where systems register commands with metadata (name, description, category, owner, version, example). The registry stores commands in a HashTable keyed by StringCRC for efficient lookup, emits events when commands are registered, and supports enumeration for help/discovery.

### Key Design Points

1. **Explicit lifecycle** - Registry has Initialize()/Shutdown() functions
2. **Single-threaded** - All registration happens during initialization (no thread safety needed)
3. **Permanent registration** - No UnregisterCommand() (commands are permanent once added)
4. **Rich metadata** - Commands include category, owner, version, and usage examples
5. **Event-driven** - Emits OnCommandRegistered for observability

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | A system can register a command with name (StringCRC), description, callback, and metadata (category, owner, version, example) | Unit test: Register command, verify stored |
| AC2 | RegisterCommand validates that command names are unique (no duplicate registrations) | Unit test: Register same command twice, expect error |
| AC3 | The registry can return a list of all registered commands (for help/discovery) | Unit test: Register 3 commands, ListCommands() returns all 3 |
| AC4 | The registry can look up a command by name (StringCRC) and return its metadata and callback | Unit test: Register command, GetCommand(name) returns correct data |
| AC5 | RegisterCommand emits an OnCommandRegistered event when a new command is added | Unit test: Attach observer, register command, verify event fired |
| AC6 | Commands registered before Initialize() are stored and available after Initialize() | Unit test: Register before init, call Initialize(), verify command available |
| AC7 | The registry uses DiaCore containers (HashTable for storage, DynamicArrayC for listing) | Code review: Verify no STL in public API |
| AC8 | Attempting to register a duplicate command logs an error and returns false | Unit test: Duplicate registration returns false, logs DIA_LOG_ERROR |

---

## Public API

### Data Structures

```cpp
namespace Dia::API {

// Command callback signature
using CommandCallback = std::function<int(const CommandArgs& args)>;

// Command arguments (parsed by cli-parser feature)
struct CommandArgs {
    Dia::Core::Containers::DynamicArrayC<const char*, 32> positionalArgs;
    Dia::Core::Containers::HashTable<Dia::Core::StringCRC, const char*> namedArgs;  // --key=value
    Dia::Core::Containers::HashTable<Dia::Core::StringCRC, bool> flags;              // --flag
};

// Command metadata
struct CommandInfo {
    Dia::Core::StringCRC name;
    const char* description;
    Dia::Core::StringCRC category;  // e.g., StringCRC("build"), StringCRC("asset"), StringCRC("debug")
    const char* owner;              // System that registered it (e.g., "DiaAssets")
    const char* version;            // Version string (e.g., "1.0.0")
    const char* example;            // Usage example (e.g., "compile-asset model.fbx --format=gltf")
    CommandCallback callback;
};

// Command registry (opaque to prevent direct access)
class CommandRegistry {
    CommandRegistry(const CommandRegistry&) = delete;
    CommandRegistry& operator=(const CommandRegistry&) = delete;
};

}
```

### Functions

```cpp
namespace Dia::API {

// Initialize the command registry
void Initialize();

// Shutdown the command registry
void Shutdown();

// Check if registry is initialized
bool IsInitialized();

// Register a new command
bool RegisterCommand(const CommandInfo& info);

// Get command info by name (returns nullptr if not found)
const CommandInfo* GetCommand(const Dia::Core::StringCRC& name);

// List all registered commands
Dia::Core::Containers::DynamicArrayC<const CommandInfo*, 64> ListCommands();

// Get commands by category (returns empty array if category not found)
Dia::Core::Containers::DynamicArrayC<const CommandInfo*, 64> GetCommandsByCategory(const Dia::Core::StringCRC& category);

}
```

### Events

```cpp
namespace Dia::API {

// Fired when a command is registered
// Parameters: commandName (StringCRC), description (const char*)
void OnCommandRegistered(const Dia::Core::StringCRC& name, const char* description);

}
```

---

## Implementation Notes

### Internal State

```cpp
namespace Dia::API::Internal {

struct RegistryState {
    bool isInitialized = false;
    Dia::Core::Containers::HashTable<Dia::Core::StringCRC, CommandInfo*> commands;  // Heap-allocated CommandInfo
    Dia::Core::Containers::DynamicArrayC<CommandInfo*, 64> pendingRegistrations;    // Pre-init registrations
};

extern RegistryState gRegistryState;

}
```

### Registration Flow

1. **Before Initialize():**
   - RegisterCommand() stores CommandInfo in `pendingRegistrations` array
   - Returns true, does not emit event yet

2. **During Initialize():**
   - Move all `pendingRegistrations` into `commands` HashTable
   - Emit OnCommandRegistered for each pending command
   - Clear `pendingRegistrations`

3. **After Initialize():**
   - RegisterCommand() directly adds to `commands` HashTable
   - Emits OnCommandRegistered immediately

### Memory Management

- CommandInfo structs are heap-allocated and owned by the registry
- Strings (description, category, owner, version, example) are NOT copied - caller must ensure lifetime
- Registry never frees CommandInfo (permanent registration)
- Shutdown() clears the HashTable but does not delete CommandInfo (acceptable memory leak on shutdown)

---

## Dependencies

### Required Modules
- **DiaCore/Containers** - HashTable, DynamicArrayC
- **DiaCore/CRC** - StringCRC
- **DiaCore/Core/Logging** - DIA_LOG_ERROR, DIA_LOG_INFO
- **DiaCore/Architecture/Observer** - Observer pattern for events (deferred to event-system feature)

### Dependent Features
- **cli-parser** - Parses argv into CommandArgs
- **help-system** - Uses ListCommands() and GetCommandsByCategory()
- **python-bindings** - Enumerates commands to expose to Python

---

## Testing Strategy

### Unit Tests (Cluiche/Tests/GoogleTests/CLI/TestCommandRegistry.cpp)

1. **Initialize/Shutdown lifecycle**
   - Initialize() sets isInitialized = true
   - Shutdown() clears registry
   - IsInitialized() returns correct state

2. **Command registration**
   - RegisterCommand() with valid info returns true
   - RegisterCommand() with duplicate name returns false
   - GetCommand() returns registered command
   - GetCommand() with unknown name returns nullptr

3. **Pre-init registration**
   - Register command before Initialize()
   - Call Initialize()
   - Verify command is available via GetCommand()

4. **Command listing**
   - Register 3 commands in different categories
   - ListCommands() returns all 3
   - GetCommandsByCategory("build") returns only build commands

5. **Event emission**
   - Attach observer to OnCommandRegistered
   - Register command
   - Verify event fired with correct name/description

6. **Duplicate detection**
   - Register command "build"
   - Register command "build" again
   - Second registration returns false
   - DIA_LOG_ERROR emitted

7. **Metadata storage**
   - Register command with all metadata fields
   - GetCommand() returns CommandInfo with correct category/owner/version/example

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | Use StringCRC for all entity/component IDs | ✅ **Compliant** - Command names are StringCRC. Enables compile-time validation and efficient lookup in HashTable. |
| PD-004 | Platform | No STL containers in public APIs | ✅ **Compliant** - CommandArgs uses DynamicArrayC and HashTable. ListCommands() returns DynamicArrayC, not std::vector. |
| PD-006 | Platform | Visual Studio project files are source of truth | ✅ **Compliant** - Will create DiaAPI.vcxproj with proper filters. No CMake. |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | ✅ **Compliant** - Will create dia.cli.architecture.module.md for DiaAPI module. |
| AD-002 | Dia App | No STL containers in public APIs | ✅ **Compliant** - Reinforces PD-004. All public APIs use Dia containers. |
| AD-003 | Dia App | Namespace convention: `Dia::<Module>::` | ✅ **Compliant** - All code in `Dia::API::` namespace. Internal state in `Dia::API::Internal::`. |
| SD-001 | DiaAPI System | Commands identified by StringCRC | ✅ **Compliant** - CommandInfo.name is StringCRC. HashTable keys are StringCRC. |
| SD-003 | DiaAPI System | Commands are stateless (no persistent state between invocations) | ✅ **Compliant** - Registry stores CommandInfo but command callbacks are stateless functions. |
| SD-005 | DiaAPI System | Registered commands stored in global registry (Singleton) | ✅ **Compliant** - Single global `gRegistryState` with Initialize()/Shutdown() lifecycle. |

---

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | API Design | Should RegisterCommand() accept CommandInfo by value or const ref? | const ref - avoids copy, caller can stack-allocate | ✅ const ref |
| 2 | Memory | Should CommandInfo strings be copied or assume caller lifetime? | Assume caller lifetime - simpler, less allocation. Caller must use string literals or static storage. | ✅ Assume caller lifetime |
| 3 | Events | Should OnCommandRegistered pass full CommandInfo or just name/description? | Just name/description - minimal event payload, listeners can call GetCommand() for details | ✅ name + description only |
| 4 | API Design | Should GetCommandsByCategory() return by value or const ref? | By value - DynamicArrayC is lightweight (just pointers), safe to return by value | ✅ By value |
| 5 | Error Handling | Should RegisterCommand() throw or return bool on duplicate? | Return bool - consistent with C++ error handling, no exceptions | ✅ Return bool |
| 6 | Initialization | Should Initialize() be idempotent (safe to call twice)? | Yes - log warning but don't crash. Matches DiaPython pattern. | ✅ Idempotent |
| 7 | Categories | Should category be StringCRC or const char*? | const char* - categories are user-facing strings for help text, not internal IDs | ✅ StringCRC - hashed for fast comparison and grouping |

---

## Open Questions

1. ✅ **Event system integration:** Implement basic event firing now (call OnCommandRegistered function), `event-system` feature adds observer registration infrastructure later.

2. ✅ **Command validation:** RegisterCommand() validates that description/category/owner are non-null and non-empty. Returns false if validation fails.

3. ✅ **Command name validation:** Enforce command name format: lowercase letters, numbers, and hyphens only (pattern: `[a-z0-9-]+`). RegisterCommand() returns false if format is invalid.

---

## Implementation Plan

### Phase 1: Core Registry (1-2 days)
- Create DiaAPI.vcxproj project
- Implement CommandRegistry.h/cpp with Initialize/Shutdown/IsInitialized
- Implement RegisterCommand() with duplicate detection
- Implement GetCommand() lookup
- Unit tests for lifecycle and basic registration

### Phase 2: Command Listing (1 day)
- Implement ListCommands()
- Implement GetCommandsByCategory()
- Unit tests for enumeration

### Phase 3: Pre-Init Registration (1 day)
- Add pendingRegistrations array
- Modify Initialize() to process pending commands
- Unit tests for pre-init registration flow

### Phase 4: Events & Polish (1 day)
- Implement OnCommandRegistered event emission
- Add command name validation
- Add metadata validation
- Unit tests for events and validation

**Total Estimate:** 4-5 days

---

## Examples

### Example 1: Register a Build Command

```cpp
#include <DiaAPI/CommandRegistry/CommandRegistry.h>

using namespace Dia::API;
using namespace Dia::Core;

// Command implementation
int BuildAssetCommand(const CommandArgs& args) {
    // ... build logic ...
    return 0;  // Success
}

// Registration (typically in system initialization)
void RegisterBuildCommands() {
    CommandInfo info;
    info.name = StringCRC("compile-asset");
    info.description = "Compile an asset to target format";
    info.category = StringCRC("build");
    info.owner = "DiaAssets";
    info.version = "1.0.0";
    info.example = "compile-asset model.fbx --format=gltf";
    info.callback = BuildAssetCommand;
    
    bool success = RegisterCommand(info);
    if (!success) {
        DIA_LOG_ERROR("DiaAssets", "Failed to register compile-asset command");
    }
}
```

### Example 2: List All Commands

```cpp
#include <DiaAPI/CommandRegistry/CommandRegistry.h>

using namespace Dia::API;

void PrintAllCommands() {
    auto commands = ListCommands();
    
    printf("Available commands:\n");
    for (unsigned int i = 0; i < commands.Size(); i++) {
        const CommandInfo* cmd = commands[i];
        printf("  %s - %s [%s]\n", 
            cmd->name.GetDebugName(), 
            cmd->description, 
            cmd->category.GetDebugName());
    }
}
```

### Example 3: Execute a Command

```cpp
#include <DiaAPI/CommandRegistry/CommandRegistry.h>

using namespace Dia::API;
using namespace Dia::Core;

int ExecuteCommandByName(const char* name, const CommandArgs& args) {
    StringCRC nameCRC(name);
    const CommandInfo* cmd = GetCommand(nameCRC);
    
    if (!cmd) {
        DIA_LOG_ERROR("DiaAPI", "Command not found: %s", name);
        return 3;  // Command not found
    }
    
    return cmd->callback(args);
}
```

---

## Status

`Done` - Implemented
