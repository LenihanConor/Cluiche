# Implementation Plan: CoW App Skeleton

**Spec:** @docs/specs/applications/cow/systems/f1-harness/cow-app-skeleton.md
**Status:** Not Started

## Implementation Patterns

- Mirror CluicheTest; its files are the reference: `Cluiche/CluicheTest/CluicheTest.vcxproj`, `Main.cpp`, `Cluiche/Assets/CluicheTest/cluichetest.diagame`, `global/misc/ApplicationFlow/cluiche_main.diaapp`, `Stages/<Stage>/`, and the `[targets.cluichetest]` block in `pipeline.toml`.
- Trim in small steps. After each trim run `dia validate manifest`, then launch. One change, one check.
- Use the baseline `MainStateProducerModule`, not `TestMainStateProducerModule`.
- Python changes in DiaCLI follow the existing pytest layout under `Dia/DiaCLI/tests/`; write the failing test first.
- Project files: `dia docs vcxproj-add`. Solution: `dia scaffold sln-add` (extended in B2).

## Tasks

| # | Task | Test | Status | Model | Notes |
|---|------|------|--------|-------|-------|
| B1 | If the data-driven target registry (scaffold-app task 1) is approved and done first, B1 is only the `[targets.cow]` block in `pipeline.toml`; otherwise as below. **RED:** tests that `launch`, `run`, `pipeline` and `deploy` accept the `cow` target and resolve `Cluiche/bin/CoW/{config}/x64/CoW.exe`. **GREEN:** add `cow` to `launch.py` exe map, `pipeline.py`, `deploy_runner.py`; add `[targets.cow]` to `pipeline.toml` (stages `compile-code, build-assets, deploy`; `app_name`; `catalogue_manifest`; `build_deps` protobuf and bgfx shaders; deploy list with a stage-asset glob) | AC2 | Not Started | sonnet | DiaCLI + shared `pipeline.toml` (approved). `reflect` omitted |
| B2 | **RED:** test that `scaffold sln-add` can add a project outside `Dia/` (new `--root` option, default `Dia`), existing behaviour unchanged. **GREEN:** implement | AC1 | Not Started | sonnet | DiaCLI (approved) |
| B3 | Create `Cluiche/CoW/CoW.vcxproj` and filters from CluicheTest's (Application, `OutDir`, deploy PostBuildEvent with `cow`, `LocalDebuggerWorkingDirectory=$(OutDir)`, trimmed references plus baseline); add to `Cluiche.sln` with `dia scaffold sln-add CoW --root Cluiche --folder executables`; run `dia check sln-sync` | AC1, AC2a | Not Started | sonnet | Shared `Cluiche.sln` (approved). Needs B2 |
| B4 | Create `Cluiche/Assets/CoW/`: `cow.diagame`, `cow_main.diaapp` (stages `Boot`, `FrontEnd`; trimmed modules and streams), `cow.diaobservation`, minimal `assets.catalogue.json`; create the `FrontEnd` stub stage (`.diastage`, stage `.diaapp`, stage module under `Cluiche/CoW/`, main-manifest entry with `"Boot"` transition, `.diagame` import); `dia validate manifest` | AC3, AC4, AC5 | Not Started | sonnet | Needs the promotion complete. Start by listing CluicheTest's modules and streams, keep only the Boot path |
| B5 | Write `Cluiche/CoW/Main.cpp`: compose `.diagame`, register path aliases, validate, run loop at 30 Hz; no `TestResultsRegistry`, no schema dump | AC3, AC8 | Not Started | sonnet | Add files with `dia docs vcxproj-add`. Log the shared-bootstrap Dia need (already in `dia-needs.md`) |
| B6 | Verify: `dia run cow` end to end (build, menu appears, launch `FrontEnd`, log line seen, return to Boot, close, exit 0); `dia run googletest`; launch CluicheTest and use its menu; confirm the boot-choice AC (Play entry); user checks the Visual Studio startup and F5 | AC1 to AC8, boot-choice AC1 | Not Started | haiku | Quote fresh output; pass/fail only |
| B7 | Observation scan on the touched files; update the spec Status; `dia docs spec-done` for the skeleton and boot-choice specs | all | Not Started | haiku | Scan findings do not block |

## Dependencies

B1 and B2 are independent of each other and of the promotion. B3 needs B2. B4 needs the promotion and B1 (for validation paths). B5 needs B3 and B4. B6 needs all. B7 last.

## Risks

- A module the trimmed manifest still needs may be hidden behind another's dependency; B4 finds these by launching after each removal.
- Catalogue entries reference shaders and UI folders by id; removing one entry may orphan a reference. `dia asset` validation phase reports this.
- `LocalDebuggerWorkingDirectory` only affects Visual Studio F5; `dia launch` is unaffected.
