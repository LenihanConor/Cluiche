"""cli_check — compatibility shim.

All logic has moved to dia_cli.cli.check.
This file is retained so that:
  1. Existing test imports of _parse_frontmatter and _scan_includes still work.
  2. The DiaCLI auto-discovery mechanism, which strips the 'cli_' prefix and
     maps this file to command name 'check', finds a valid ``cli`` symbol.

New code should import from dia_cli.cli.check directly.
"""
from dia_cli.cli.check import (  # noqa: F401  re-export for back-compat
    _parse_frontmatter,
    _scan_includes,
    cli,
)
