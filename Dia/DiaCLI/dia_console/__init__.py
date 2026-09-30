"""DiaConsole — UI-independent command/execution/result model over DiaCLI.

This package reflects DiaCLI's Click command tree into typed descriptors and
runs commands via subprocess.  It deliberately contains **no** UI code: see
``docs/specs/applications/dia/systems/diaconsole/diaconsole.md`` (SD-CONSOLE-005).

``dia_console`` depends on ``dia_cli``; never the reverse.
"""
