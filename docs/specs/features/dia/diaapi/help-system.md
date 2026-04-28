# Feature Spec: help-system

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diacli.md | **help-system** |

**Status:** `Approved`

---

## Problem Statement

Generate human-readable help text automatically from registered commands to help users discover available commands and understand their usage without reading source code.

---

## Solution Overview

The **help-system** feature queries the command-registry to enumerate all registered commands and formats them into help text. It detects when the user requests help (--help flag or no command), groups commands by category, and displays name, description, category, owner, version, and usage examples in a clean terminal format.

### Key Design Points

1. **Auto-generated** - Help text generated from CommandInfo metadata, no manual documentation
2. **Category grouping** - Commands grouped by category (build, asset, debug, etc.)
3. **Two modes** - Global help (all commands) and command-specific help (one command details)
4. **Simple formatting** - Plain text output, no colors or fancy formatting (testable)
5. **No external dependencies** - Uses only command-registry data

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | Detect --help flag in CommandArgs and trigger help display | Unit test: Parse args with --help, verify ShowHelp() called |
| AC2 | Show global help when no command is provided or --help flag is present | Unit test: ShowGlobalHelp() lists all registered commands |
| AC3 | Show command-specific help when `DiaAPI <command> --help` is invoked | Unit test: ShowCommandHelp("build") shows only build command details |
| AC4 | Group commands by category in global help | Unit test: Register commands in 3 categories, verify grouped output |
| AC5 | Display command metadata: name, description, category, owner, version, example | Unit test: Verify output contains all fields |
| AC6 | Return exit code 0 after displaying help (success) | Unit test: ShowHelp() returns 0 |
| AC7 | Handle unknown command in help request (DiaAPI unknown --help) | Unit test: Return error "Command not found: unknown" |
| AC8 | Format output as plain text (no ANSI colors, no special characters) | Code review: Verify uses printf/stdout only |

---

## Public API

### Functions

```cpp
namespace Dia::API {

// Show global help (all commands, grouped by category)
int ShowGlobalHelp();

// Show help for specific command
int ShowCommandHelp(const Dia::Core::StringCRC& commandName);

// Check if help was requested in parsed arguments
bool IsHelpRequested(const CommandArgs& args, const Dia::Core::StringCRC& commandName);

}
```

---

## Implementation Notes

### Help Detection Logic

```cpp
bool IsHelpRequested(const CommandArgs& args, const StringCRC& commandName) {
    // Help requested if:
    // 1. Command name is empty/invalid AND --help flag present
    // 2. OR --help flag present for any command
    return args.flags.ContainsKey(StringCRC("help"));
}
```

### Global Help Format

```
DiaAPI - Command-line interface for Dia Engine

Usage: DiaAPI <command> [arguments...]

Available commands (grouped by category):

BUILD:
  compile-asset         Compile an asset to target format [v1.0.0]
    Owner: DiaAssets
    Example: compile-asset model.fbx --format=gltf
  
  validate-asset        Validate asset file integrity [v1.0.0]
    Owner: DiaAssets
    Example: validate-asset model.fbx

ASSET:
  list-assets           List all assets in project [v1.2.0]
    Owner: DiaAssets
    Example: list-assets --filter=*.fbx

Use 'DiaAPI <command> --help' for command-specific help.
```

### Command-Specific Help Format

```
DiaAPI compile-asset - Compile an asset to target format

Category: build
Owner:    DiaAssets
Version:  1.0.0

Usage:
  compile-asset model.fbx --format=gltf

Description:
  Compile an asset to target format
```

### Output Strategy

- Use `printf()` for all output (simple, testable, redirectable)
- Write to stdout (not stderr)
- Return exit code 0 after help (not an error)

---

## Dependencies

### Required Modules
- **command-registry** - ListCommands(), GetCommandsByCategory(), GetCommand()
- **cli-parser** - CommandArgs definition, --help flag detection
- **DiaCore/CRC** - StringCRC
- **DiaCore/Core/Logging** - DIA_LOG_WARNING (for unknown commands)

### Dependent Features
- **DiaAPI main** - Calls IsHelpRequested() and ShowHelp()

---

## Testing Strategy

### Unit Tests (Cluiche/Tests/GoogleTests/CLI/TestHelpSystem.cpp)

1. **Help detection**
   - IsHelpRequested() with --help flag returns true
   - IsHelpRequested() without --help returns false

2. **Global help**
   - Register 3 commands in 2 categories
   - ShowGlobalHelp() outputs all commands grouped
   - Verify output contains all command names and descriptions

3. **Command-specific help**
   - Register command "build"
   - ShowCommandHelp(StringCRC("build")) outputs command details
   - Verify output contains name, description, category, owner, version, example

4. **Unknown command help**
   - ShowCommandHelp(StringCRC("unknown")) returns error
   - Output contains "Command not found: unknown"

5. **Empty registry**
   - ShowGlobalHelp() with no commands outputs "No commands registered"

6. **Output format**
   - Capture stdout during ShowGlobalHelp()
   - Verify plain text format (no ANSI codes)
   - Verify contains "Usage:", "Available commands:"

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | Use StringCRC for all entity/component IDs | ✅ **Compliant** - Command names are StringCRC in API. |
| PD-004 | Platform | No STL containers in public APIs | ✅ **Compliant** - Uses command-registry APIs which use DiaCore containers. |
| PD-006 | Platform | Visual Studio project files are source of truth | ✅ **Compliant** - Part of DiaAPI.vcxproj. |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | ✅ **Compliant** - Part of DiaAPI module documentation. |
| AD-002 | Dia App | No STL containers in public APIs | ✅ **Compliant** - Reinforces PD-004. |
| AD-003 | Dia App | Namespace convention: `Dia::<Module>::` | ✅ **Compliant** - All code in `Dia::API::` namespace. |
| SD-002 | DiaAPI System | Exit codes follow Unix conventions (0=success) | ✅ **Compliant** - ShowHelp() returns 0 (help is success, not error). |
| SD-004 | DiaAPI System | No interactive prompts (headless by default) | ✅ **Compliant** - Help outputs to stdout, no user interaction. |

---

## AI Review Questions

| # | Section | Question | Answer |
|---|---------|----------|--------|
| 1 | Output | Should help go to stdout or stderr? | ✅ stdout - help is informational, not error |
| 2 | Formatting | Should we add color/bold formatting? | ✅ No - plain text only (simple, testable, scriptable) |
| 3 | Sorting | Should commands be sorted alphabetically within categories? | ✅ Yes - alphabetical by name for consistency |
| 4 | Empty Categories | Should we show category headers with no commands? | ✅ No - only show categories that have commands |
| 5 | Help Text | Should we include argument syntax (e.g., [required] <optional>)? | ✅ No - just show example, commands document their own args |

---

## Open Questions

All resolved:

1. ✅ **Unknown command handling:** ShowCommandHelp() for unknown command returns error code 3 (command not found), logs warning
2. ✅ **Exit code:** ShowHelp() returns 0 (success), caller exits immediately with this code
3. ✅ **Categories without commands:** Skip empty categories in output (don't show header)

---

## Implementation Plan

### Phase 1: Core Help Display (1 day)
- Implement ShowGlobalHelp()
- Query ListCommands(), group by category, format output
- Unit tests for basic help display

### Phase 2: Command-Specific Help (1 day)
- Implement ShowCommandHelp()
- Query GetCommand(), format single command output
- Handle unknown commands
- Unit tests for command help

### Phase 3: Help Detection (0.5 days)
- Implement IsHelpRequested()
- Unit tests for flag detection

### Phase 4: Integration & Polish (0.5 days)
- Integrate with DiaAPI main
- Sorting and formatting polish
- Integration tests

**Total Estimate:** 3 days

---

## Examples

### Example 1: Global Help

```cpp
#include <DiaAPI/Help/HelpSystem.h>

using namespace Dia::API;

int main(int argc, char* argv[]) {
    // DiaAPI --help or DiaAPI with no args
    return ShowGlobalHelp();
}
```

### Example 2: Command-Specific Help

```cpp
#include <DiaAPI/Help/HelpSystem.h>

using namespace Dia::API;
using namespace Dia::Core;

int main(int argc, char* argv[]) {
    // DiaAPI build --help
    StringCRC commandName("build");
    
    if (IsHelpRequested(args, commandName)) {
        return ShowCommandHelp(commandName);
    }
    
    // Execute command...
    return 0;
}
```

### Example 3: Help Detection in Main Loop

```cpp
#include <DiaAPI/Parser/ArgumentParser.h>
#include <DiaAPI/Help/HelpSystem.h>

using namespace Dia::API;
using namespace Dia::Core;

int main(int argc, char* argv[]) {
    ParseResult result = ParseArguments(argc, argv);
    
    if (result.errorCode != 0) {
        return result.errorCode;
    }
    
    // Check for help request
    if (IsHelpRequested(result.args, result.commandName)) {
        if (result.commandName.IsValid()) {
            return ShowCommandHelp(result.commandName);
        } else {
            return ShowGlobalHelp();
        }
    }
    
    // Execute command...
    return ExecuteCommand(result.commandName, result.args);
}
```

---

## Status

`Done` - Implemented
