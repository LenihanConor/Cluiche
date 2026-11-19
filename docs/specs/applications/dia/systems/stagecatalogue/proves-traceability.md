# Feature Spec: `proves` Traceability

**Parent:** @docs/specs/applications/dia/systems/stagecatalogue/stagecatalogue.md (Feature 3)
**Status:** Approved (2026-11-18)

## Summary

A stage lists the specs it proves in a `proves` array of repo-relative spec paths. A spec opts in to "a stage must prove me" with a `**Testbed:** required` header line. `dia check stage-proves` then reports:

- broken `proves` links,
- unproven opt-in specs that are Approved, In Progress or Done.

The generated stage registry gains a "proves" column. The runtime ignores the field.

## Goals

- Answers "which systems have no testbed?" and "what does this stage prove?" from files, with no game running.
- Makes "every world layer's testbed proves it" (the CoW testbed philosophy) checkable.
- Avoids noise: systems proved by GoogleTests alone (DiaMaths, DiaCore) are never flagged.

## Acceptance Criteria

### Format

1. `.diastage` gains optional `proves`: an array of unique, repo-relative, forward-slash paths to `.md` files under `docs/specs/`, at most 16 entries.
2. `dia validate manifest` checks the shape:
   - array of strings, `docs/specs/` prefix, `.md` suffix, no duplicates;
   - a path that does not exist is a **warning** here, so manifest validation doesn't depend on the docs tree; it is an error in AC 5.
3. `DiaStageManifest` gains `proves` (`DynamicArrayC<String256, 16>`), and `JsonDiaStageSerializer` round-trips it. `ManifestComposerV2` and the runtime catalogue ignore it.

### Opt-in marker

4. A spec opts in with a header line `**Testbed:** required` in the block before its first `## ` heading.
   - Matching is case-insensitive on `required`.
   - Any other value (or no line) means not required.
   - The check documents the marker in its `--help`.

### Check (`dia check stage-proves`)

5. `dia check stage-proves [--game <name>] [--strict]`:
   1. Finds every `.diagame` under `Cluiche/Assets/`, or the named one.
   2. Loads its stages with the Feature 1 Python loader.
   3. Resolves each `proves` path. A missing file is a **broken link**.
6. It reads spec status from the `## Status` section's first backticked word, falling back to a `**Status:**` header line. An opt-in spec whose status is Approved, In Progress or Done, and that no stage in any game names, is **unproven**. Draft specs are not flagged.
7. **Output:**
   - A console summary (counts, then one line per finding).
   - `Cluiche/out/check/stage-proves.md`, with sections Broken links, Unproven specs, and Coverage (spec → stages that prove it).
   - `--json` prints the same data.
8. **Exit codes:**
   - 1 on any broken link;
   - 0 otherwise, even with unproven specs;
   - with `--strict`, unproven specs also exit 1.
9. `dia docs precommit` runs the check without `--strict`, and broken links fail precommit.

### Registry and adoption

10. `dia docs stage-registry` (Feature 1) gains a "Proves" column with links.
11. **Adoption:**
    - The S-1, S-2.5, S-3 and S-3.5 testbed system specs, and the CoW world-layer system specs they prove (S-1 worldgen, A4, A1, A3), get `**Testbed:** required`.
    - `S1WorldGenerationTestbedStage` gets `proves` for S-1.
    - `F2CoreRuntime` gets `proves` for the F-2 spec.
    - The first check run lists the expected unproven specs, which have no stage yet. That output is the baseline and is recorded in the plan.

## Files Touched

- **DiaGame:** `DiaGameManifest.h`, `JsonDiaStageSerializer.*`.
- **DiaCLI:**
  - `cli/cli_validate.py`
  - `cli/check.py`, plus a new `commands/check/stage_proves.py`
  - `commands/docs/stage_registry_cmd.py`
  - `commands/docs/precommit_cmd.py`
  - tests
- **CoW:** two `.diastage` files.
- **Specs:** `**Testbed:** required` lines in the listed CoW specs.

## Binding Decisions

| Decision | How this feature complies |
|---|---|
| SC-002: named fields only | `proves` is a named field with one named consumer, `dia check stage-proves` |
| SC-008: spec file paths; `**Testbed:** required` opt-in | AC 1, 4-6 |
| PD-010: `.diastage` is tooling metadata | Only tools read `proves`; the runtime ignores it (AC 3) |
| PD-009: generated output under `Cluiche/out/` | Report written to `Cluiche/out/check/` (AC 7) |

## Test Plan

- **GoogleTests:** serializer round trip with `proves`, and the composer ignores it.
- **pytest** (fixture repo tree with specs and stages):
  - shape validation and the missing-path warning;
  - a broken link gives exit 1;
  - unproven opt-in Approved spec: listed, exit 0, exit 1 with `--strict`;
  - Draft opt-in spec not flagged;
  - non-opt-in spec never flagged;
  - status read from the `## Status` section, and the header fallback;
  - case-insensitive marker;
  - report and `--json` content;
  - registry column;
  - precommit integration.
- **Manual:** `dia check stage-proves` on the repo matches the baseline recorded in the plan.

## Open Design Questions

1. **Feature specs.** Only system specs are expected to opt in. A stage may still name a feature spec in `proves`; it counts as coverage but never triggers "unproven".
   - Revisit if per-feature testbeds become common.
2. **Moved specs.** Renaming or moving a spec breaks `proves` links. Precommit (AC 9) catches this, but the fix is manual.
   - Optional later: `dia check stage-proves --fix-moves`, using git rename detection.
