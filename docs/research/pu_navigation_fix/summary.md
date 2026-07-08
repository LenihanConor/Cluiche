# Fix: Navigation Requests Must Not Cross from RenderPU to SimPU via ExecuteCommandJson

**Rule:** `pu-rules.md` — RenderPU must not call `TransitionTo()` or `ExecuteCommandJson` for navigation.

## Violations

Both modules call `Dia::API::ExecuteCommandJson("dia.automation.navigate_to")` from their `DoUpdate()` on the render thread in response to button presses. The call chain resolves to `Application::TransitionTo()`, which is mutex-protected and technically safe, but the **decision is being made on the wrong thread**.

| Module | PU | Call site |
|---|---|---|
| `BootMenuModule` | kRender | `DrawMenu()` → Launch button click |
| `TestStageHUDModule` | kRender | `RenderBottomBar()` → X button click |

## Fix Design

Introduce a typed one-way stream from RenderPU → SimPU carrying navigation intent. SimPU reads it and calls `TransitionTo()`.

### Step 1 — Define `RenderToSimNavRequest`

New type alongside `MainToRenderFrame.h` in `Cluiche/CluicheGameBaseline/Types/`:

```cpp
// Cluiche/CluicheGameBaseline/Types/RenderToSimNavRequest.h
#pragma once
#include <DiaCore/CRC/StringCRC.h>

namespace Cluiche { namespace AppFlow {

struct RenderToSimNavRequest
{
    Dia::Core::StringCRC target;   // empty CRC = no request this frame
};

} } // namespace Cluiche::AppFlow
```

### Step 2 — Wire `BootMenuModule`

Remove the `ExecuteCommandJson` call. Add a `StreamWriter` and write the target when the button is clicked.

```cpp
// BootMenuModule.h — add:
#include <DiaStreams/StreamWriter.h>
#include "Types/RenderToSimNavRequest.h"

Dia::ApplicationFlow::StreamWriter<Cluiche::AppFlow::RenderToSimNavRequest>
    mNavRequest{this, "BootMenuNavRequest"};
```

```cpp
// BootMenuModule.cpp — replace ExecuteCommandJson call with:
mNavRequest.Write({Dia::Core::StringCRC(target.AsChar())});
```

### Step 3 — Wire `TestStageHUDModule`

Same pattern, different stream name.

```cpp
// TestStageHUDModule.h — add:
#include <DiaStreams/StreamWriter.h>
#include "Types/RenderToSimNavRequest.h"

Dia::ApplicationFlow::StreamWriter<Cluiche::AppFlow::RenderToSimNavRequest>
    mNavRequest{this, "HUDNavRequest"};
```

```cpp
// TestStageHUDModule.cpp — replace ExecuteCommandJson call with:
mNavRequest.Write({Dia::Core::StringCRC("Boot")});
```

### Step 4 — New `SimNavigationHandlerModule` on SimPU

New module in `Cluiche/CluicheGameBaseline/Modules/`. Reads both nav request streams, calls `TransitionTo()` when either carries a non-empty target.

```cpp
// SimNavigationHandlerModule.h
#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaStreams/StreamReader.h>
#include "Types/RenderToSimNavRequest.h"

namespace Cluiche { namespace AppFlow {

class SimNavigationHandlerModule : public Dia::ApplicationFlow::Module
{
public:
    static const Dia::Core::StringCRC kTypeId;
    static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs =
        Dia::ApplicationFlow::PUAffinity::kSim;
    static constexpr const char* kDescription =
        "Reads navigation requests from RenderPU and calls TransitionTo on SimPU";

    explicit SimNavigationHandlerModule(const Dia::Core::StringCRC& instanceId);

protected:
    Dia::ApplicationFlow::StartResult DoStart() override;
    void DoUpdate(float dt) override;
    void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

private:
    Dia::ApplicationFlow::StreamReader<RenderToSimNavRequest>
        mBootNavInput{this,  "BootMenuNavRequest"};
    Dia::ApplicationFlow::StreamReader<RenderToSimNavRequest>
        mHUDNavInput{this,   "HUDNavRequest"};
};

} } // namespace Cluiche::AppFlow
```

```cpp
// SimNavigationHandlerModule.cpp
void SimNavigationHandlerModule::DoUpdate(float)
{
    auto tryNavigate = [&](const RenderToSimNavRequest& req) {
        if (req.target.IsValid())
            TransitionTo(req.target);
    };

    if (mBootNavInput.HasNewData()) tryNavigate(mBootNavInput.Read());
    if (mHUDNavInput.HasNewData())  tryNavigate(mHUDNavInput.Read());
}
```

### Step 5 — Register in the .diaapp manifests

Add `SimNavigationHandlerModule` to `SimPU` in:
- `Cluiche/Assets/CluicheTest/Global/misc/ApplicationFlow/cluiche_main.diaapp`
- Any other .diaapp that uses BootMenuModule or TestStageHUDModule

## Updated Cross-PU Data Flow

```
BootMenuModule (RenderPU) ──("BootMenuNavRequest")──► SimNavigationHandlerModule (SimPU)
TestStageHUDModule (RenderPU) ──("HUDNavRequest")───► SimNavigationHandlerModule (SimPU)
SimNavigationHandlerModule ──TransitionTo()──────────► Application (thread-safe, on SimPU)
```

## Backlog Entry

> **Fix RenderPU navigation violations** — `BootMenuModule` and `TestStageHUDModule` call `ExecuteCommandJson("navigate_to")` from the render thread. Replace with a `RenderToSimNavRequest` FrameStream + `SimNavigationHandlerModule` on SimPU. See `docs/research/pu_navigation_fix/summary.md`.
