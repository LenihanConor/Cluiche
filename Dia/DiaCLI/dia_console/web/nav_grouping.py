"""Category -> friendly nav-group label mapping for DiaConsole.

The mapping is the "resolved, not open" table from console-native-shell.md's
Data Contracts. It is implemented once, here, in Python, and shipped over
``GET /api/commands`` as each command's ``groupLabel`` field (see
``dia_console/web/app.py``) so ``app.js`` only ever buckets nav items by an
already-resolved label instead of re-implementing this table in JavaScript
and risking the two copies drifting apart.
"""
from __future__ import annotations

#: Fallback bucket for any top-level command segment not in the table below.
#: A category never raises or gets dropped for being unmapped -- it lands
#: here instead (closed-enum-plus-fallback pattern, matching ArgumentType's
#: degrade-to-STRING behaviour in dia_console.model).
ADVANCED_GROUP_LABEL = "Advanced"

#: (top-level path segments, friendly label). Segments never repeat across
#: rows. Keyed on a CommandDescriptor's *first* path segment (``path[0]``),
#: not its ``category`` field -- a bare top-level command like ``run`` has
#: ``category == ""`` but ``path[0] == "run"``, which is what this table's
#: left-hand column actually enumerates.
_GROUP_TABLE: tuple[tuple[frozenset[str], str], ...] = (
    (frozenset({"run", "launch", "pipeline", "fix", "diagnose"}), "Run & Debug"),
    (frozenset({"test", "check"}), "Test & Quality"),
    (frozenset({"asset", "reflect"}), "Assets & Data"),
    (frozenset({"scaffold", "docs", "codegen"}), "Create & Docs"),
    (frozenset({"env"}), "Environment"),
)


def group_label_for(top_level_segment: str) -> str:
    """Map a command's first path segment to its nav group label.

    Never raises: any segment outside the fixed table above (``api``,
    ``show``, ``capture``, ``agent``, ``command``, ``cli_validate``,
    ``cli_check``, or anything invented later) falls into
    :data:`ADVANCED_GROUP_LABEL` rather than being dropped or crashing.
    """
    for segments, label in _GROUP_TABLE:
        if top_level_segment in segments:
            return label
    return ADVANCED_GROUP_LABEL
