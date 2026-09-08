---
name: new-cluichetest-stage
description: Scaffold a new CluicheTest test stage — infer domain, run dia scaffold stage, add domain-specific C++.
tags: [scaffold, cluichetest, stage, teststage]
user_invocable: true
agent_invocable: false
---

# New Test Stage Scaffold

Creates all boilerplate for a new CluicheTest test stage. The mechanical scaffold (files, manifests, vcxproj) is handled by `dia scaffold stage`. This skill infers the domain, picks the right modules, runs the script, and then adds domain-specific C++.

## Usage

```
/new-cluichetest-stage <description or domain name>
```

Examples:
- `/new-cluichetest-stage RigidBody2D` — physics stage
- `/new-cluichetest-stage "test entity spawning and hierarchy"` — entity stage
- `/new-cluichetest-stage Animation2D` — animation stage

## Instructions for Claude

### Step 1 — Infer domain and select modules

From the user's description, identify the domain and select the appropriate pattern from the Domain Patterns section below. Derive the PascalCase name if not already given (e.g. "test entity spawning" → `Entity`).

### Step 2 — Run the scaffold script

```bash
dia scaffold stage <Name> --modules <ModuleA>,<ModuleB> --budget <frames>
```

For a stage with no extra modules beyond AutomationModule:
```bash
dia scaffold stage <Name>
```

The script creates all 9 touch points and prints a summary. If it fails, report the error — do not manually create files.

### Step 3 — Add domain-specific C++ to the generated module

Read the generated `.h` and `.cpp` files. Based on the domain pattern:

1. Add `ModuleRef<T>` members to the header for each domain module
2. Include domain module headers
3. Override `AreDependenciesReady()` to check domain dependencies
4. Implement `OnStart()`: set up the scene, register the checkpoint lambda with correct pass logic
5. Implement `OnUpdate()`: check the pass condition, call `ReportPassed()`
6. Update `kDescription` from "TODO: describe..." to a real one-liner
7. Add `#ifdef DIA_DEBUG` visual debugger setup in `OnUpdate()` if the domain has a visual debugger

### Step 4 — Run pipeline to verify

```bash
dia pipeline --target cluichetest --config Debug
```

Expected: pipeline complete, 0 failed. The new stage should appear in the Boot menu.

### Step 5 — Report

Tell the user:
- All files created/modified (from script output)
- Domain pattern applied
- What still needs domain-specific implementation (if any)
- Pipeline result

---

## Domain Patterns

Use these to determine `--modules`, `--budget`, and the domain-specific C++ to add after scaffolding.

---

### Physics (RigidBody2D)

**Trigger:** stage name contains "RigidBody", "Physics", "collision", "rigid", "body"

**Script call:**
```bash
dia scaffold stage <Name> --modules Physics2DModule --budget 900
```

**Module members to add to header:**
```cpp
#include "Modules/Physics2DModule.h"
// in private:
Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::Physics2DModule> mPhysics{this};
```

**AreDependenciesReady:**
```cpp
bool AreDependenciesReady() override {
    auto* p = mPhysics.Get();
    return p && p->GetWorld();
}
```

**OnStart scene setup pattern:** Create rigid bodies (circles, boxes, ground plane) via `mPhysics.Get()->GetWorld()->AddRigidBody(def)`. Register checkpoint that checks a condition on the physics world.

**OnUpdate pass condition pattern:** Check physics body states (e.g. `AreAllBodiesAsleep()`), call `ReportPassed()`.

**Visual debugger (DIA_DEBUG):** Lazily create `PhysicsShapesDrawer`, `VelocityArrowsDrawer`, etc. in `OnUpdate`. Register with stageTag from `GetStageName()`.

```cpp
#ifdef DIA_DEBUG
#include <DiaRigidBody2DVisualDebugger/PhysicsShapesDrawer.h>
// ... other drawers
#endif
```

---

### Entity

**Trigger:** stage name contains "Entity", "spawn", "hierarchy", "component", "ECS"

**Script call:**
```bash
dia scaffold stage <Name> --modules EntityModule --budget 600
```

**Module members to add:**
```cpp
#include "Modules/EntityModule.h"
Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::EntityModule> mEntity{this};
```

**AreDependenciesReady:**
```cpp
bool AreDependenciesReady() override {
    return mEntity.Get() != nullptr;
}
```

**OnStart pattern:** Create entities via `mEntity.Get()->GetRealm()`. Set up checkpoints that validate entity state (existence, component values, hierarchy).

**OnUpdate pattern:** Query realm for entities, validate properties, call `ReportPassed()`.

---

### Asset Loading

**Trigger:** stage name contains "Asset", "loading", "texture", "resource"

**Script call:**
```bash
dia scaffold stage <Name> --budget 240
```

(No extra modules — uses `AssetServiceModule::GetStatic()` directly, same pattern as TestAssetRuntimeStageModule.)

**PersistsAcrossEntries:** Override `PersistsAcrossEntries() { return true; }` if testing reload behavior.

**OnUpdate pattern:** Call `AssetServiceModule::GetStatic()->IsStageLoadComplete(GetStageName())`, compare snapshots, call `ReportPassed()`.

---

### Animation

**Trigger:** stage name contains "Animation", "Anim", "clip", "blend", "pose"

**Script call:**
```bash
dia scaffold stage <Name> --modules Physics2DModule,EntityModule --budget 600
```

**AreDependenciesReady:** Check both Physics2DModule and EntityModule.

**OnUpdate pattern:** Validate animation state via entity component queries.

---

### State Machine

**Trigger:** stage name contains "StateMachine", "State", "FSM", "transition"

**Script call:**
```bash
dia scaffold stage <Name> --budget 300
```

**OnStart pattern:** Create a state machine, configure states and transitions, start it.

**OnUpdate pattern:** Drive the state machine, validate it reaches the expected state.

---

### Geometry / Spatial

**Trigger:** stage name contains "Geometry", "spatial", "intersection", "BVH", "quadtree"

**Script call:**
```bash
dia scaffold stage <Name> --budget 60
```

(Short budget — geometry validation is immediate, no simulation needed.)

**OnStart pattern:** Create shapes, run intersection tests, spatial structure queries. Set mPassed = result.

**OnUpdate pattern:** Single-frame check — call `ReportPassed()` immediately if mPassed.

---

### Minimal (no domain)

**Trigger:** No domain matched, or user explicitly wants a bare stage.

**Script call:**
```bash
dia scaffold stage <Name>
```

Implement `OnUpdate()` with a TODO comment only.

---

## Naming Convention

All stages follow `<Domain>TestStage` / `<Domain>TestStageModule`:

| Component | Pattern | Example |
|-----------|---------|---------|
| Stage name | `<Domain>TestStage` | `Animation2DTestStage` |
| Module class | `<Domain>TestStageModule` | `Animation2DTestStageModule` |
| Checkpoint prefix | `test.<domain>.` | `test.animation2d.passed` |

## Notes

- **Always place test stage modules on MainPU** — AutomationModule lives there
- **Call `ReportPassed()` from `OnUpdate`** — base handles timeout via `GetBudgetFrames()`
- **`AreDependenciesReady()` is the loading gate** — return false until all dependencies are available; base retries each frame
- **Visual debugger layers must use stageTag** — `mgr->Register(drawer.get(), priority, Dia::Core::StringCRC(GetStageName().AsChar()))` — so they deactivate automatically on stage exit
- **`PersistsAcrossEntries()`** — override returning true only for stages that compare state across stop/start cycles
