---
include: conditional
---

# Command Reference

## Spec Commands

- `/spec-platform` - Create or update the platform spec
- `/spec-app` - Create a new application spec
- `/spec-system` - Create a new system spec
- `/spec-feature` - Create a new feature spec (includes interview + AI review)
- `/spec-review` - Review any spec and populate AI review questions
- `/spec-trace` - Trace a feature's full lineage up to platform

## Review Command

- `/review <spec-path>` - Review implementation against its spec with 8 passes
  - Passes: Spec Compliance, Test Exhaustiveness, Architecture, Product, Performance, Thread Safety, Binding Decisions, API Surface
  - Outputs a punch-list table with severity (Critical/Important/Minor), location, and suggestion per finding
  - Verdict: PASS / PASS WITH ISSUES / BLOCKED
  - Options: `--pass=<name>` for single pass, `--severity=<level>` to filter, `--diff=<range>` for custom diff range
  - Use after completing a feature (all plan tasks done) or before merging to master

## Scaffold Commands

- `/new-cluichetest-stage <StageName>` - Scaffold a new CluicheTest test stage from a PascalCase name
  - Creates: `.diastage`, `.diaapp`, `Module.h/.cpp`, vcxproj entries, `cluiche_main.diaapp` wiring, `cluichetest.diagame` import, `assets.catalogue.json` entries, `pipeline.toml` registration
  - Module skeleton placed on MainPU with AutomationModule dependency + one placeholder checkpoint
  - Runs `dia pipeline --target cluichetest` to verify at the end

## Test Commands

- `/gen-tests <target>` - Generate comprehensive tests for a Dia module or component
  - Reads public API headers, checks test-completeness-registry, proposes all applicable test types (unit, stress, golden-value, invariant, determinism, conservation, integration)
  - Creates test files, updates vcxproj, builds, runs, and updates the registry
  - Options: `--type=<type>` to limit test types, `--dry-run` to preview, `--update-registry` to refresh counts only
- `/gtest [options]` - Build and run GoogleTest suite with filtering and failure analysis
