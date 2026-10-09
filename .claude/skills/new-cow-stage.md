---
name: new-cow-stage
description: Scaffold a new CoW testbed stage from the GDD — look up the testbed, run dia scaffold stage --app CoW, add panel stubs.
tags: [scaffold, cow, stage, testbed]
user_invocable: true
agent_invocable: false
---

# New CoW Testbed Stage

Creates a new CoW testbed stage. The mechanical scaffold (module, stage manifests, main-manifest entry, `.diagame` import, catalogue, vcxproj) is `dia scaffold stage <Name> --app CoW`. This skill adds what CluicheTest's skill does not need: CoW stages are driven by the GDD, so it reads the testbed's design first.

Spec: `docs/specs/applications/cow/systems/f1-harness/new-cow-stage-skill.md`. Mirrors `.claude/skills/new-cluichetest-stage.md`.

## Usage

```
/new-cow-stage <testbed id or name>
```

Examples:
- `/new-cow-stage S-2` — Dragon testbed
- `/new-cow-stage Dragon`

## Rules

- **The GDD owns design** (AD-003). Read it; never edit it and never invent design intent.
- **One stage per testbed** (AD-005). Modes and presets (for example isolated and live) become panels inside the one stage, never extra stages.
- **Dia or baseline changes are proposed, not made** (AD-001). Unknown engine needs go to `docs/specs/applications/cow/dia-needs.md` (AD-002), not into the stage.
- Never create stage files by hand; if the scaffold fails, report the error.

## Instructions for Claude

### Step 1 — Resolve the testbed

Look the argument up in the Systems table of `docs/specs/applications/cow/cow.md` (id, name, track). **If it is not listed, stop and ask the user** — do not scaffold.

### Step 2 — Read the design

The GDD is a sibling checkout. Root: the `COW_GDD_PATH` environment variable if set, otherwise `C:\GitHub\DesignDocs\COW`. Under `Output Docs/GDD/10_PrototypeStrategy/` read:

1. `TestbedRoster.md` — the summary entry for this testbed (what it absorbs, its modes and presets).
2. The testbed's own file if one exists (`F1_Harness.md`, `F2_CoreRuntime.md`, `S1_World.md`, ...).

If the GDD path does not exist, stop and tell the user how to set `COW_GDD_PATH`.

**Locked check:** the testbed is locked when its pieces are crossed out (`~~like this~~`) in the "CoW: Features to Be Designed (GDD)" table of `docs/BACKLOG.md`. If it is not locked, warn the user and ask whether to scaffold a skeleton stage anyway. Do not continue without an answer.

### Step 3 — Pick the name and modules

- Stage name: PascalCase `<Name>TestbedStage` (for example `DragonTestbedStage`). `--app` uses the name as given, with no suffix added.
- Choose `--modules` from the systems the testbed names, using baseline module patterns (`Cluiche/CluicheGameBaseline/Modules/`). If a needed system has no module, record it in `dia-needs.md` and leave it out of the stage.

### Step 4 — Scaffold

```bash
dia scaffold stage <Name>TestbedStage --app CoW [--modules <ModuleA>,<ModuleB>]
```

Use `--dry-run` first if unsure. `--budget` is accepted but ignored for app stages. The command prints every file it creates or edits.

### Step 5 — Add panel stubs

Read the generated `Cluiche/CoW/Modules/<Name>TestbedStageModule.h/.cpp`. For each mode and preset the roster names (for example isolated and live for S-2), add an empty ImGui panel stub inside this one stage, and a TODO naming the GDD section it implements. Update `kDescription` to a real one-liner. Do not write simulation or visual content.

### Step 6 — Build and prove

```bash
dia run cow
```

Confirm the stage is listed in the Boot menu, launch it, and return with Escape. Report pass/fail only.

### Step 7 — Record

- Add a note on the testbed row in `docs/BACKLOG.md` ("scaffolded" plus the stage name).
- Update the relevant system spec or plan row (`dia docs plan`). Do not touch the GDD.
- Tell the user: files created and edited, modules chosen, panels added, anything sent to `dia-needs.md`, and the run result.
