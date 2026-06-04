---
include: conditional
---

# Command Reference

## Spec Commands

- `/spec-platform` - Create or update the platform spec
- `/spec-app` - Create a new application spec
- `/spec-system` - Create a new system spec
- `/spec-feature` - Create a new feature spec (includes interview + open design questions)
- `/spec-review` - Review any spec and refresh decisions + open design questions
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

## Backlog Command

- `/backlog` - Show the current project backlog in four sections: Ready to Build, Ready to Spec, Blocked, What's Next

## Implementation Commands

- `/implement <spec-path>` - Orchestrate full implementation from approved spec to working code
  - Creates plan, dispatches subagents per task, verifies, commits each task
  - Resumes from existing plan if one exists (idempotent re-invocation)
  - See `.claude/skills/implement.md` for full protocol

## Quality Commands

- `/fixup` - Auto-diagnose and fix common mechanical build/test errors
  - Pattern-matches: missing includes, wrong signatures, typos, vcxproj gaps, namespace issues
  - Max 3 passes before escalating to `/debug`
  - Agent-invocable: fires automatically after subagent build failures
- `/pre-commit` - Run structural validation checks before committing
  - Checks: module deps, manifest validity, module docs, vcxproj sync, forbidden patterns
  - Options: `--scope <path>` to limit to specific directory
  - Offers auto-fix for mechanical failures (vcxproj, deps)

## Test Commands

- `/gen-tests <target>` - Generate comprehensive tests for a Dia module or component
  - Reads public API headers, checks test-completeness-registry, proposes all applicable test types (unit, stress, golden-value, invariant, determinism, conservation, integration)
  - Creates test files, updates vcxproj, builds, runs, and updates the registry
  - Options: `--type=<type>` to limit test types, `--dry-run` to preview, `--update-registry` to refresh counts only
- `/gtest [options]` - Build and run GoogleTest suite with filtering and failure analysis
