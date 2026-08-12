# Feature Spec: DiaXxxVisualDebugger Contract

**Parent System:** @docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
**Status:** Done

## Summary

Establishes the formal 15-AC contract that all current and future `DiaXxxVisualDebugger` modules must satisfy. Enforced by `dia check debugger-contract`, which statically validates module isolation, interface completeness, ImGui-free code, JSON minimum structure, command handler presence, and test file existence.

## Acceptance Criteria

**Module isolation rules:**

1. Each visual debugger system = one `DiaXxxVisualDebugger` module (one `.vcxproj`, one module YAML).
2. `DiaXxx` (the owned system) has **zero** `#include` or link dependency on `DiaXxxVisualDebugger`.
3. `DiaXxxVisualDebugger` may depend on: `DiaXxx` + `DiaVisualDebugger` + `DiaCore` + draw infrastructure only.

**Interface completeness:**

4. Implements `IDebugDomain` — all pure virtual methods.
5. `GetDescription()` string is ≤80 characters.

**World-space draw rules:**

6. World-space draw colours from `DebugColourPalette` only — no hardcoded `ColourRGBA` literals in `Draw()` implementations. Panel accent tints returned by `GetAccentColour()` use `DebugGroupAccents` named constants (defined in the group accent table in the system spec; published as `Dia/DiaVisualDebugger/Domain/DebugGroupAccents.h`); these are named constants, not inline literals, and are exempt from the `DebugColourPalette`-only rule.
7. All world-space sizes, radii, and lengths multiplied by `IDebugContext::GetDebugScale()`. Verified by the scale-sensitivity test shape (AC 15); not statically checkable by `dia check`.
8. No `ImGui::GetBackgroundDrawList()` or any other ImGui call in a visual debugger module.

**Cross-PU data access:**

9. Read-only access to sim-owned data is via `const T&` (accepted convention) or `DebugSnapshot<T>` if double-buffering is required. No mutable shared state.

**JSON state:**

10. `GetJSONState()` emits at minimum `{ "drawers": [{name, enabled}], "stats": {} }`. Domains may include additional domain-specific fields (e.g. score tables, state lists, plan cursors, rule fire reports) consumed by domain-specific view code in `debug-panel.html`. The per-domain extended schema is defined in each domain's migration spec and is the normative contract between C++ and the panel's JS.

**Command handling:**

11. `OnCommand("toggle", {drawer: name})` — enables/disables the named drawer.
12. `OnCommand("setScale", {key: name, value: float})` — updates a named scale parameter.

`OnCommand` arrives on the Render PU (panel JS → JS bridge → C++) while drawers execute on the Sim PU. Implementations must route mutations through an `std::atomic` flag or a small lock-free command queue drained by `VisualDebuggerModule::DoUpdate` on the Sim PU — not a raw member write across PU boundaries.

**Tests:**

13. `Tests/GoogleTests/DiaXxxVisualDebugger/TestXxxVisualDebugger.cpp` exists.
14. Tests use `RecordingDebugVisitor` or mock `IDebugDraw`.
15. Mandatory test shapes: enable/disable gate for each drawer, each drawer type emits correct primitive type, scale sensitivity (changing `GetDebugScale()` changes output measurements), `GetJSONState()` round-trip, `OnCommand("toggle", ...)` round-trip.

## `dia check debugger-contract`

Statically validates ACs 1–3 (module isolation), 4 (IDebugDomain inheritance), 8 (ImGui-free), 10 (GetJSONState declared), 11–12 (OnCommand declared), 13–14 (test file existence).

AC 5 (description ≤80 chars), AC 6 (palette), AC 7 (scale), AC 15 (test shapes) are verified by mandatory test shapes — not static analysis.

Modules currently not yet migrated to `IDebugDomain` are reported as **PENDING** (AC 4), not as broken failures. All other checks must pass. Exit code is non-zero if any module has a failing or pending check.
