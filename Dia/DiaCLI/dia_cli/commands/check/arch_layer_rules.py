"""arch_layer_rules — encode the numbered-layer ordering rules and check violations."""
from __future__ import annotations

from typing import Optional


# ---------------------------------------------------------------------------
# Layer parsing helpers
# ---------------------------------------------------------------------------

def parse_layer(layer: str) -> Optional[tuple]:
    """Parse a layer string into a structured key for ordering.

    Returns a tuple suitable for ordering comparisons, or None if unrecognised.

    Layer strings (from module YAML):
      foundation/core          → (1, 0, "core",        "")
      foundation/maths         → (1, 1, "maths",       "")
      foundation/services      → (1, 1, "services",    "")
      foundation/platform      → (1, 2, "platform",    "")
      foundation/application   → (1, 2, "application", "")
      assets/core              → (2, 0, "assets",      "core")
      assets/tools             → (2, 1, "assets",      "tools")
      domain/visual/core       → (3, 0, "visual",      "core")
      domain/visual/tools      → (3, 1, "visual",      "tools")
      domain/physics/core      → (3, 0, "physics",     "core")
      domain/physics/tools     → (3, 1, "physics",     "tools")
      domain/animation/core    → (3, 0, "animation",   "core")
      domain/animation/tools   → (3, 1, "animation",   "tools")
    """
    layer = layer.strip()

    if layer.startswith("foundation/"):
        group = layer[len("foundation/"):]
        level_map = {
            "core": (1, 0),
            "maths": (1, 1),
            "services": (1, 1),
            "platform": (1, 2),
            "application": (1, 2),
        }
        if group in level_map:
            major, sub = level_map[group]
            return (major, sub, group, "")
        return None

    if layer.startswith("assets/"):
        tier = layer[len("assets/"):]
        if tier == "core":
            return (2, 0, "assets", "core")
        if tier == "tools":
            return (2, 1, "assets", "tools")
        return None

    if layer.startswith("domain/"):
        parts = layer[len("domain/"):].split("/")
        if len(parts) == 2:
            domain, tier = parts
            if tier == "core":
                return (3, 0, domain, "core")
            if tier == "tools":
                return (3, 1, domain, "tools")
        return None

    return None


# ---------------------------------------------------------------------------
# Violation check
# ---------------------------------------------------------------------------

def check_layer_violation(
    from_layer: str,
    to_layer: str,
) -> Optional[str]:
    """Return a rule-violation description if from_layer → to_layer is forbidden.

    Returns None if the dependency is permitted.
    """
    from_parsed = parse_layer(from_layer)
    to_parsed = parse_layer(to_layer)

    if from_parsed is None or to_parsed is None:
        return None  # unknown layer — skip (warned elsewhere)

    from_major, from_sub, from_group, from_tier = from_parsed
    to_major, to_sub, to_group, to_tier = to_parsed

    # Rule 1: no upward deps — level N cannot depend on level N+1 or above
    if to_major > from_major:
        return (
            f"{from_layer} (level {from_major}) may not depend on "
            f"{to_layer} (level {to_major})"
        )

    # Rules apply within the same level
    if to_major == from_major:

        # Foundation (level 1): sub-level ordering
        if from_major == 1:
            # Rule 2: sub-levels are ordered: 1.0 < 1.1 < 1.2
            if to_sub > from_sub:
                return (
                    f"foundation/{from_group} (sub-level {from_sub}) may not depend on "
                    f"foundation/{to_group} (sub-level {to_sub})"
                )
            # Rule 3: same sub-level, different group → forbidden
            if to_sub == from_sub and from_group != to_group:
                return (
                    f"foundation/{from_group} (1.{from_sub}) may not depend on "
                    f"foundation/{to_group} (1.{to_sub}) — same sub-level, different group"
                )

        # Assets (level 2): tools may not depend on each other via level
        if from_major == 2:
            if to_sub > from_sub:
                return (
                    f"assets/{from_tier} (sub-level {from_sub}) may not depend on "
                    f"assets/{to_tier} (sub-level {to_sub})"
                )

        # Domain (level 3): cross-domain forbidden at core tier
        if from_major == 3:
            # Sub-level ordering within domain (tools can depend on core)
            if to_sub > from_sub:
                return (
                    f"domain/{from_group}/{from_tier} (sub-level {from_sub}) may not depend on "
                    f"domain/{to_group}/{to_tier} (sub-level {to_sub})"
                )
            # Cross-domain: both are core OR include a cross-domain core ref
            if from_group != to_group:
                return (
                    f"domain/{from_group}/{from_tier} may not depend on "
                    f"domain/{to_group}/{to_tier} — cross-domain dependency forbidden"
                )

    return None
