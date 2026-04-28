# Feature Spec: diaapi-bridge

## Traceability

| Level | Parent | This Feature |
|-------|--------|--------------|
| Platform | @docs/specs/platform/Cluiche.md | - |
| Application | @docs/specs/applications/dia.md | - |
| System | @docs/specs/systems/dia/diacli.md | **diaapi-bridge** |

**Status:** `Done`

---

## Problem Statement

Some operations require C++ DiaAPI commands (e.g., engine validation, internal queries). DiaCLI (Python) needs a way to invoke these commands without duplicating logic.

---

## Solution Overview

The **diaapi-bridge** feature provides a Python wrapper that loads the C++ DiaAPI library via Python bindings, discovers registered commands, and executes them from DiaCLI, enabling hybrid Python orchestration + C++ execution workflows.

### Key Design Points

1. **Python bindings bridge** - Use DiaPython to call C++ DiaAPI commands
2. **Command discovery** - Query DiaAPI for registered commands
3. **Argument marshalling** - Convert Python args to C++ CommandArgs format
4. **Optional dependency** - Bridge only loaded if DiaAPI library available
5. **Graceful fallback** - If DiaAPI unavailable, log warning and skip

---

## Acceptance Criteria

| ID | Criterion | Verification Method |
|----|-----------|---------------------|
| AC1 | `dia api <command>` invokes C++ DiaAPI command | Manual test: Register C++ command, invoke via `dia api` |
| AC2 | `dia api list` shows all registered DiaAPI commands | Manual test: Verify lists C++ commands |
| AC3 | Arguments passed correctly from Python to C++ | Unit test: Invoke command with args, verify C++ receives them |
| AC4 | Bridge loads DiaAPI.dll/.so dynamically at runtime | Unit test: Mock library loading |
| AC5 | If DiaAPI not available, bridge logs warning and skips | Unit test: Test without DiaAPI library present |

---

## Public API

### Commands

```bash
# List available DiaAPI commands
dia api list

# Invoke C++ DiaAPI command
dia api validate-assets --path Assets/

# Show help for DiaAPI command
dia api help <command>
```

### Python API

```python
from dia_cli.utils.diaapi_bridge import DiaAPIBridge

bridge = DiaAPIBridge()
if bridge.is_available():
    commands = bridge.list_commands()
    exit_code = bridge.execute("validate-assets", args=["--path", "Assets/"])
```

---

## Dependencies

- **DiaPython** (C++ library) - Python bindings to C++ code
- **DiaAPI** (C++ library) - Command registry and execution

---

## Status

`Done` - Implemented
