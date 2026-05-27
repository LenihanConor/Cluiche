---
name: new-cluichetest-stage
description: Scaffold a new CluicheTest test stage — all 6 touch points from a single stage name.
tags: [scaffold, cluichetest, stage, teststage]
user_invocable: true
agent_invocable: false
---

# New Test Stage Scaffold

Generates all boilerplate for a new CluicheTest test stage from a single PascalCase stage name.

## Usage

```
/new-cluichetest-stage <StageName>
```

Example: `/new-cluichetest-stage Geometry2D`

The stage name must be PascalCase. Derived names are computed automatically:
- `snake_case`: `geometry2d` → used in file names, asset IDs, stage IDs
- `stage_id`: `stage.geometry2d_stage`
- `module_name`: `Geometry2DStageModule`

## What Gets Created

All 6 touch points that every new test stage requires:

| # | File | Change |
|---|------|--------|
| 1 | `Cluiche/Assets/Stages/<Name>/asset_<snake>_stage.diastage` | New stage declaration |
| 2 | `Cluiche/Assets/Stages/<Name>/misc/ApplicationFlow/<snake>_stage.diaapp` | New app manifest |
| 3 | `Cluiche/CluicheTest/Modules/TestStages/<Name>StageModule.h/.cpp` | New module skeleton |
| 4 | `Cluiche/CluicheTest/CluicheTest.vcxproj` + `.vcxproj.filters` | Add source files |
| 5 | `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp` | Add stage + Boot transition + HUD coverage |
| 6 | `Cluiche/Assets/CluicheTest/cluichetest.diagame` | Add import |
| 7 | `Cluiche/Assets/CluicheTest/assets.catalogue.json` | Add stage + manifest entries |
| 8 | `pipeline.toml` | Add to asset_stages + deploy files |

## Instructions for Claude

When this skill is invoked with a stage name:

### Step 1 — Derive names

From `<StageName>` (e.g. `Geometry2D`):
- `snake` = PascalCase → snake_case, lowercase (e.g. `geometry2d`)
- `snake_stage` = `<snake>_stage` (e.g. `geometry2d_stage`)
- `stage_id` = `stage.<snake_stage>` (e.g. `stage.geometry2d_stage`)
- `module_name` = `<StageName>StageModule` (e.g. `Geometry2DStageModule`)
- `diastage_file` = `<snake_stage>.diastage`
- `diaapp_file` = `<snake_stage>.diaapp`

### Step 2 — Create stage directory and files

**`.diastage`** at `Cluiche/Assets/Stages/<StageName>/<diastage_file>`:
```json
{
    "name": "<StageName>",
    "manifest": "stages/<StageName>/misc/ApplicationFlow/<diaapp_file>",
    "config": {
        "path_aliases": {
            "stage_root": "."
        }
    }
}
```

**`.diaapp`** at `Cluiche/Assets/Stages/<StageName>/misc/ApplicationFlow/<diaapp_file>`:
```json
{
    "version": 3,
    "processing_units": [
        {
            "instance_id": "MainPU",
            "frequency_hz": 30,
            "dedicated_thread": false,
            "modules": [
                {
                    "instance_id": "<module_name>",
                    "type_id": "<module_name>",
                    "stages": [ "<StageName>" ],
                    "dependencies": [ "AutomationModule" ],
                    "channels": []
                }
            ]
        },
        {
            "instance_id": "SimPU",
            "frequency_hz": 30,
            "dedicated_thread": true,
            "modules": [
                {
                    "instance_id": "VisualDebuggerModule",
                    "type_id": "VisualDebuggerModule",
                    "stages": [ "<StageName>" ],
                    "dependencies": [],
                    "channels": [
                        { "id": "SimToRender", "role": "writes" }
                    ]
                }
            ]
        }
    ]
}
```

**Module header** at `Cluiche/CluicheTest/Modules/TestStages/<module_name>.h`:
```cpp
#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/ModuleRefV2.h>
#include <DiaCore/CRC/StringCRC.h>
#include "Modules/AutomationModule.h"

namespace Dia { namespace Observation { namespace Metric { class Gauge; } } }

namespace CluicheTest {

class <module_name> : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    explicit <module_name>(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float deltaTime) override;
    Dia::ApplicationFlow::StopResult DoStop() override;

private:
    void RegisterCheckpoints();

    Dia::ApplicationFlow::ModuleRef<Cluiche::AppFlow::AutomationModule> mAutomation{this};

    unsigned int mFrameCount = 0;
    bool         mCheckpointPassed = false;
};

} // namespace CluicheTest
```

**Module implementation** at `Cluiche/CluicheTest/Modules/TestStages/<module_name>.cpp`:
```cpp
#include "Modules/TestStages/<module_name>.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>

namespace CluicheTest {

const Dia::Core::StringCRC <module_name>::kTypeId("<module_name>");

<module_name>::<module_name>(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult <module_name>::DoStart()
{
    DIA_LOG_INFO("CluicheTest", "<module_name> DoStart entry");

    auto* automationModule = mAutomation.Get();
    if (!automationModule || !automationModule->GetService())
        return Dia::ApplicationFlow::StartResult::kLoading;

    mFrameCount = 0;
    mCheckpointPassed = false;
    RegisterCheckpoints();

    DIA_LOG_INFO("CluicheTest", "<module_name> DoStart ready");
    return Dia::ApplicationFlow::StartResult::kReady;
}

void <module_name>::DoUpdate(float /*deltaTime*/)
{
    ++mFrameCount;
    // TODO: implement stage logic
}

Dia::ApplicationFlow::StopResult <module_name>::DoStop()
{
    DIA_LOG_INFO("CluicheTest", "<module_name> DoStop");

    if (auto* automationModule = mAutomation.Get())
    {
        if (auto* service = automationModule->GetService())
            service->UnregisterCheckpoints(this);
    }

    mFrameCount = 0;
    mCheckpointPassed = false;
    return Dia::ApplicationFlow::StopResult::kDone;
}

void <module_name>::RegisterCheckpoints()
{
    auto* service = mAutomation.Get()->GetService();

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("<snake>.passed"),
        [this]() -> Dia::Automation::CheckpointResult {
            return { mCheckpointPassed,
                     mCheckpointPassed ? "passed" : "pending",
                     0.0f };
        });
}

} // namespace CluicheTest

namespace { using <module_name>_ = CluicheTest::<module_name>; }
DIA_MODULE(<module_name>_);
```

### Step 3 — Update `cluiche_main.diaapp`

Read `Cluiche/Assets/CluicheTest/Global/Misc/ApplicationFlow/cluiche_main.diaapp`.

1. Add to `"stages"` array (after the last stage entry):
   ```json
   { "name": "<StageName>", "transitions": ["Boot"], "auto_advance": false }
   ```

2. Add `"<StageName>"` to Boot's `"transitions"` array.

3. Add `"<StageName>"` to `TestStageHUDModule`'s `"stages"` array so the HUD is visible.

Then force-copy the file to the deployed bin:
```
Cluiche/bin/CluicheTest/Debug/x64/assets/global/misc/ApplicationFlow/cluiche_main.diaapp
```

### Step 4 — Update `cluichetest.diagame`

Read `Cluiche/Assets/CluicheTest/cluichetest.diagame`.

Add to `"imports"` array:
```json
{
    "path": "stages/<StageName>/<diastage_file>",
    "type": "stage"
}
```

### Step 5 — Update `assets.catalogue.json`

Read `Cluiche/Assets/CluicheTest/assets.catalogue.json`.

Add two entries to the `"assets"` array:

```json
{
  "id": "<stage_id>",
  "type": "stage",
  "source_path": "../Stages/<StageName>/<diastage_file>",
  "scope": "global",
  "tags": ["misc"],
  "references": [
    { "type": "contains", "target": "manifest.<snake_stage>" }
  ]
},
{
  "id": "manifest.<snake_stage>",
  "type": "manifest",
  "source_path": "../Stages/<StageName>/misc/ApplicationFlow/<diaapp_file>",
  "scope": "stage",
  "stage_name": "<StageName>",
  "tags": ["misc/ApplicationFlow"]
}
```

### Step 6 — Update `pipeline.toml`

Read `pipeline.toml`.

1. Add `"<stage_id>"` to `asset_stages` under `[targets.cluichetest]`.

2. Add to `[targets.cluichetest.deploy]` files array:
   ```toml
   { src = "Cluiche/Assets/Stages/<StageName>/<diastage_file>", dest = "$(OutDir)assets/stages/<StageName>/" },
   ```

### Step 7 — Update `.vcxproj` and `.vcxproj.filters`

**`CluicheTest.vcxproj`** — add inside the existing `<ItemGroup>` for ClCompile and ClInclude:
```xml
<ClCompile Include="Modules\TestStages\<module_name>.cpp" />
<ClInclude Include="Modules\TestStages\<module_name>.h" />
```

**`CluicheTest.vcxproj.filters`** — add under the `ApplicationFlow\Modules\TestStages` filter (which already exists). If the filter doesn't exist yet, create it with a new GUID.

### Step 8 — Run pipeline to verify

```
dia pipeline --target cluichetest --config Debug
```

Expected: pipeline complete, 0 failed. The new stage should appear in the Boot menu.

### Step 9 — Report

Tell the user:
- All files created/modified (list them)
- Reminder of what to implement next: fill in `DoUpdate` logic and replace the placeholder checkpoint name `<snake>.passed` with a real checkpoint name
- Pipeline result (pass/fail)

## Notes

- **Always place test stage modules on MainPU** — AutomationModule lives on MainPU; SimPU modules cannot depend on it
- **Always include a SimToRender writer on SimPU** — `SimToRender` is a FrameStream `from: SimPU`; without a writer the RenderPU starves and the application freezes (spinner stops). Add `VisualDebuggerModule` on SimPU writing to `SimToRender` in the `.diaapp` — it emits empty frames when there is nothing to draw
- **The HUD step is mandatory** — without it, there's no way to navigate back to Boot from the stage
- **pipeline.toml `asset_stages` is mandatory** — without it, the stage has 0 assets in the deployed runtime manifest
- **Force-copy `cluiche_main.diaapp`** to bin after editing — the pipeline's up-to-date check won't catch it
- **Do NOT add a `transitions` field to `.diastage`** — transitions live in `cluiche_main.diaapp`'s `stages` array only
