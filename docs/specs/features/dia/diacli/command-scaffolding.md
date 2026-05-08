# Feature Spec: command-scaffolding

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diacli.md | **command-scaffolding** |

**Status:** `Done`

---

## Problem Statement

Developers extending DiaCLI need an easy way to create new command boilerplate (cli file + implementation file) without manually copying templates or remembering the structure.

---

## Solution Overview

The **command-scaffolding** feature provides `dia command create` to generate new DiaCLI command files from templates, making it self-documenting and easy to extend.

### Key Design Points

1. **Self-service extension** - DiaCLI helps you extend itself
2. **Template-based** - Uses Jinja2 templates for generation
3. **Two-file pattern** - Generates `cli/<name>.py` + `commands/<name>_cmd.py`
4. **Auto-wired** - Generated commands automatically discovered via plugin-discovery
5. **Examples included** - Generated code includes usage examples and docstrings

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `dia command create <name>` generates cli/<name>.py | Manual test: Create command, verify file exists |
| AC2 | Generated command includes Click decorators and docstring | Code review: Check generated file |
| AC3 | Generated command automatically appears in `dia --help` | Manual test: Create command, run `dia --help`, verify listed |
| AC4 | Generated command is immediately executable | Manual test: `dia <name>`, verify runs without errors |
| AC5 | Implementation file created in commands/<name>_cmd.py | Manual test: Verify file created |
| AC6 | Template supports command groups (with subcommands) | Manual test: Generate group, verify subcommand structure |

---

## Public API

### Commands

```bash
# Create simple command
dia command create validate

# Create command group with subcommands
dia command create asset --group

# Create command in subdirectory
dia command create tools/benchmark
```

---

## Dependencies

- **jinja2** (^2.11.2) - Template rendering
- **plugin-discovery** - Generated commands auto-discovered

---

## Status

`Done` - Implemented
