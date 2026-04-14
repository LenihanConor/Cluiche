# Feature Spec: cli-parser

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diacli.md | **cli-parser** |

**Status:** `Approved`

---

## Problem Statement

Parse raw command-line arguments (argv) into a structured CommandArgs object with positional arguments, named arguments (--key=value), and flags (--flag) for command execution.

---

## Solution Overview

The **cli-parser** feature provides an argument parser that converts `int argc, char* argv[]` from main() into a CommandArgs structure. It extracts the command name from argv[1], then parses remaining arguments into positional args, named args (--key=value), and boolean flags (--flag). The parser handles quoted strings, validates argument format, and returns structured data ready for command execution.

### Key Design Points

1. **Format-only validation** - Parser validates syntax (--key=value format), not semantics (commands validate values)
2. **Unix conventions** - Support `--` to end flag parsing, double-dash prefix for named args
3. **Short flag support** - `-v` equivalent to `--verbose` via alias mapping
4. **No STL** - Uses DiaCore containers throughout
5. **Command name extraction** - First argument after executable is command name (StringCRC)

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | Parse positional arguments from argv (e.g., `DiaCLI build foo.txt bar.txt` → positionalArgs = ["foo.txt", "bar.txt"]) | Unit test: Parse with positional args, verify CommandArgs.positionalArgs |
| AC2 | Parse named arguments with `--key=value` format (e.g., `--format=gltf` → namedArgs["format"] = "gltf") | Unit test: Parse with named args, verify CommandArgs.namedArgs |
| AC3 | Parse boolean flags with `--flag` format (e.g., `--verbose` → flags["verbose"] = true) | Unit test: Parse with flags, verify CommandArgs.flags |
| AC4 | Support short-form flags with single dash (e.g., `-v` equivalent to `--verbose`) | Unit test: Parse `-v`, verify flags["verbose"] = true |
| AC5 | The first argument after "DiaCLI" is the command name, remaining arguments are parsed into CommandArgs | Unit test: Parse `DiaCLI build --flag`, verify commandName="build" |
| AC6 | Handle quoted arguments with spaces (e.g., `"path with spaces"` is one argument) | Unit test: Parse quoted string, verify single positional arg |
| AC7 | Return error for malformed arguments (e.g., `--key` without value, `---invalid`) | Unit test: Parse malformed args, verify error code 2 |
| AC8 | Populate CommandArgs using DiaCore containers (DynamicArrayC, HashTable) | Code review: Verify no STL in implementation |
| AC9 | Unknown/unrecognized flags are added to CommandArgs (no validation at parser level - command validates) | Unit test: Parse `--unknown-flag`, verify stored in CommandArgs |
| AC10 | Support `--` to mark end of flags (everything after is positional) | Unit test: Parse `build --flag -- --not-a-flag`, verify `--not-a-flag` is positional |
| AC11 | Handle empty values for named args (e.g., `--key=""`) | Unit test: Parse `--key=""`, verify namedArgs["key"] = "" |

---

## Public API

### Data Structures

```cpp
namespace Dia::CLI {

// Command arguments (defined in CommandRegistry.h)
struct CommandArgs {
    Dia::Core::Containers::DynamicArrayC<const char*, 32> positionalArgs;
    Dia::Core::Containers::HashTable<Dia::Core::StringCRC, const char*> namedArgs;  // --key=value
    Dia::Core::Containers::HashTable<Dia::Core::StringCRC, bool> flags;              // --flag
};

// Parse result
struct ParseResult {
    Dia::Core::StringCRC commandName;  // Extracted from argv[1]
    CommandArgs args;                   // Parsed arguments
    int errorCode;                      // 0 = success, 2 = invalid arguments
    const char* errorMessage;           // Human-readable error (nullptr on success)
};

}
```

### Functions

```cpp
namespace Dia::CLI {

// Parse command-line arguments
// Returns ParseResult with commandName, args, and error status
ParseResult ParseArguments(int argc, char* argv[]);

}
```

### Short Flag Aliases

```cpp
namespace Dia::CLI {

// Register short flag alias (e.g., -v → --verbose)
void RegisterShortFlag(const char* shortFlag, const char* longFlag);

// Built-in aliases:
// -h → --help
// -v → --verbose

}
```

---

## Implementation Notes

### Parsing Algorithm

```
1. If argc < 2: return error "No command specified"
2. Extract commandName = argv[1]
3. For i = 2 to argc-1:
   a. If arg == "--": mark endOfFlags = true, continue
   b. If endOfFlags: add to positionalArgs
   c. Else if arg starts with "--":
      - If contains "=": split into key/value, add to namedArgs
      - Else: add to flags with value = true
   d. Else if arg starts with "-" (single dash):
      - Look up alias in short flag map
      - If found: add full name to flags
      - Else: return error "Unknown short flag"
   e. Else: add to positionalArgs
4. Return ParseResult with success
```

### Short Flag Alias Map

```cpp
namespace Dia::CLI::Internal {

struct ParserState {
    Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Dia::Core::StringCRC> shortFlagAliases;
    // Key: StringCRC("-v"), Value: StringCRC("verbose")
};

extern ParserState gParserState;

}
```

### String Handling

- **Quoted strings:** Shell handles quote parsing (argv already unquoted by OS)
- **String lifetime:** Parser does NOT copy strings - all `const char*` point into argv (caller must keep argv alive)
- **String storage:** CommandArgs stores pointers to argv elements, not copies

### Error Handling

**Error Codes:**
- `0` - Success
- `2` - Invalid arguments (malformed syntax)

**Error Messages:**
- "No command specified" - argc < 2
- "Invalid named argument format: --key (expected --key=value)" - Missing `=`
- "Invalid flag format: ---flag (too many dashes)" - More than 2 dashes
- "Unknown short flag: -x" - Short flag not in alias map

---

## Dependencies

### Required Modules
- **DiaCore/Containers** - DynamicArrayC, HashTable
- **DiaCore/CRC** - StringCRC
- **DiaCore/Core/Logging** - DIA_LOG_ERROR, DIA_LOG_WARNING
- **CommandRegistry** - CommandArgs definition (shared type)

### Dependent Features
- **command-registry** - Provides CommandArgs type definition
- **help-system** - Uses parser to detect `--help` flag
- **DiaCLI main** - Calls ParseArguments() as entry point

---

## Testing Strategy

### Unit Tests (Cluiche/Tests/GoogleTests/CLI/TestArgumentParser.cpp)

1. **Basic parsing**
   - Parse `DiaCLI build` → commandName = "build", no args
   - Parse `DiaCLI build file.txt` → positional = ["file.txt"]
   - Parse `DiaCLI build file1.txt file2.txt` → positional = ["file1.txt", "file2.txt"]

2. **Named arguments**
   - Parse `DiaCLI build --format=gltf` → namedArgs["format"] = "gltf"
   - Parse `DiaCLI build --key=value --other=123` → two named args
   - Parse `DiaCLI build --empty=""` → namedArgs["empty"] = ""

3. **Flags**
   - Parse `DiaCLI build --verbose` → flags["verbose"] = true
   - Parse `DiaCLI build --flag1 --flag2` → two flags
   - Parse `DiaCLI build -v` → flags["verbose"] = true (via alias)

4. **Mixed arguments**
   - Parse `DiaCLI build file.txt --format=gltf --verbose` → positional + named + flag
   - Parse `DiaCLI build --verbose file.txt` → flag before positional (order preserved)

5. **End-of-flags marker**
   - Parse `DiaCLI build --flag -- --not-a-flag` → positional = ["--not-a-flag"]
   - Parse `DiaCLI build -- --help` → positional = ["--help"] (not a flag)

6. **Error cases**
   - Parse `DiaCLI` (no command) → error code 2
   - Parse `DiaCLI build --key` (no value) → error code 2
   - Parse `DiaCLI build ---invalid` (triple dash) → error code 2
   - Parse `DiaCLI build -x` (unknown short flag) → error code 2

7. **Short flag aliases**
   - RegisterShortFlag("-h", "help")
   - Parse `DiaCLI build -h` → flags["help"] = true
   - Parse `DiaCLI build -v -h` → flags["verbose"] and flags["help"]

8. **String lifetime**
   - Parse arguments, destroy argv, verify CommandArgs still points to original strings (this will fail - documents expectation)
   - Note: Caller must keep argv alive for CommandArgs lifetime

---

## Binding Decisions Compliance

| Decision | Source | Summary | Compliance |
|----------|--------|---------|------------|
| PD-001 | Platform | Use StringCRC for all entity/component IDs | ✅ **Compliant** - Command names are StringCRC. HashTable keys for namedArgs and flags use StringCRC. |
| PD-004 | Platform | No STL containers in public APIs | ✅ **Compliant** - ParseResult and CommandArgs use DynamicArrayC and HashTable, not std::vector/std::map. |
| PD-006 | Platform | Visual Studio project files are source of truth | ✅ **Compliant** - Parser code added to DiaCLI.vcxproj. |
| AD-001 | Dia App | Module system with YAML frontmatter documentation | ✅ **Compliant** - Part of DiaCLI module documentation. |
| AD-002 | Dia App | No STL containers in public APIs | ✅ **Compliant** - Reinforces PD-004. |
| AD-003 | Dia App | Namespace convention: `Dia::<Module>::` | ✅ **Compliant** - All code in `Dia::CLI::` namespace. |
| SD-001 | DiaCLI System | Commands identified by StringCRC | ✅ **Compliant** - Command name extracted from argv[1] and converted to StringCRC. |
| SD-002 | DiaCLI System | Exit codes follow Unix conventions (0=success) | ✅ **Compliant** - ParseResult.errorCode uses 0 for success, 2 for invalid arguments. |
| SD-004 | DiaCLI System | No interactive prompts (headless by default) | ✅ **Compliant** - Parser is pure function, no user interaction. |

---

## AI Review Questions

| # | Section | Question | Suggested Default | Answer |
|---|---------|----------|-------------------|--------|
| 1 | API Design | Should we provide both ParseResult return and output-param styles? | Yes - offer both, let callers choose preference | ✅ ParseResult return only (no output params) |
| 2 | Short Flags | Should RegisterShortFlag() be public or internal? | Public - allows systems to register custom short flags | ✅ Public API |
| 3 | Error Handling | Should malformed args be logged as ERROR or WARNING? | WARNING - syntax errors are user mistakes, not system failures | ✅ WARNING level |
| 4 | Validation | Should we validate command name format here or in command-registry? | Command-registry - parser just extracts, registry validates | ✅ Parser extracts, registry validates |
| 5 | Memory | Should we copy argv strings or assume caller keeps them alive? | Assume caller lifetime - simpler, matches standard argc/argv conventions | ✅ Assume caller lifetime |
| 6 | Edge Cases | What if argv[1] starts with "--" (e.g., `DiaCLI --help`)? | Special case: if argv[1] is "--help", treat as help request with no command | ✅ Special case for --help |
| 7 | Named Args | Should we support both `--key=value` and `--key value` (space-separated)? | Only `--key=value` - simpler parsing, no ambiguity with flags | ✅ Only --key=value format |

---

## Open Questions

1. ✅ **Help flag special handling:** Treat `--help` as normal flag in CommandArgs.flags. help-system feature checks for it and handles help display.

2. ✅ **Command name validation:** Parser validates argv[1]: if "--help" then special case (empty command + help flag), else if starts with "-" then error, else extract as command name.

3. ✅ **Short flag initialization:** Built-in short flags (-h → help, -v → verbose) registered during DiaCLI::Initialize(). Systems can register custom short flags after initialization.

---

## Implementation Plan

### Phase 1: Core Parser (2 days)
- Implement ParseArguments() with basic parsing
- Handle positional, named (--key=value), and flags (--flag)
- Return ParseResult with error codes
- Unit tests for basic cases

### Phase 2: Short Flags (1 day)
- Implement short flag alias map
- RegisterShortFlag() function
- Built-in aliases (-h, -v)
- Unit tests for short flag resolution

### Phase 3: Edge Cases (1 day)
- End-of-flags marker (`--`)
- Empty values (`--key=""`)
- Error validation (malformed args)
- Unit tests for edge cases

### Phase 4: Integration (1 day)
- Integrate with command-registry (use CommandArgs type)
- Add to DiaCLI main entry point
- Integration tests with full flow

**Total Estimate:** 5 days

---

## Examples

### Example 1: Basic Argument Parsing

```cpp
#include <DiaCLI/Parser/ArgumentParser.h>

using namespace Dia::CLI;

int main(int argc, char* argv[]) {
    // Parse: DiaCLI build model.fbx --format=gltf --verbose
    ParseResult result = ParseArguments(argc, argv);
    
    if (result.errorCode != 0) {
        printf("Error: %s\n", result.errorMessage);
        return result.errorCode;
    }
    
    printf("Command: %s\n", result.commandName.GetDebugName());
    printf("Positional args: %d\n", result.args.positionalArgs.Size());
    printf("Format: %s\n", result.args.namedArgs.TryGetItem(StringCRC("format")));
    printf("Verbose: %s\n", result.args.flags.ContainsKey(StringCRC("verbose")) ? "yes" : "no");
    
    return 0;
}
```

### Example 2: Register Custom Short Flag

```cpp
#include <DiaCLI/Parser/ArgumentParser.h>

using namespace Dia::CLI;

void InitializeParser() {
    // Register custom short flags
    RegisterShortFlag("-f", "force");
    RegisterShortFlag("-q", "quiet");
    RegisterShortFlag("-d", "debug");
}

// Now: DiaCLI build -f → flags["force"] = true
```


---

## Status

`Approved` - Ready for implementation
