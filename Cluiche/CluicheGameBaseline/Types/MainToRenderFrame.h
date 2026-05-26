#pragma once
#include <DiaCore/CRC/StringCRC.h>
#include <cstdint>

namespace Cluiche { namespace AppFlow {

// Heartbeat/presence status forwarded from AutomationModule to RenderPU.
// Replaces direct AutomationModule::GetStatic() cross-PU access.
struct AutomationStatus
{
    bool heartbeatEnabled = false;
};

// Current HUD overlay state forwarded from MainPU to RenderPU each frame.
// Replaces direct TestResultsRegistry cross-PU access.
struct StageHUDState
{
    enum class StageState : uint8_t
    {
        kNotRun  = 0,
        kRunning = 1,
        kPassed  = 2,
        kFailed  = 3,
        kTimeout = 4,
    };

    static constexpr unsigned int kMaxCheckpoints = 8;

    Dia::Core::StringCRC activeStageName;
    StageState           stageState    = StageState::kNotRun;
    unsigned int         frameCount    = 0;
    unsigned int         budgetFrames  = 0;
    unsigned int         checkpointCount = 0;
    Dia::Core::StringCRC checkpoints[kMaxCheckpoints];
};

// Composite FrameStream payload: MainPU -> RenderPU (sent every frame).
// Carries all per-frame cross-PU data that RenderPU needs for HUD rendering.
struct MainToRenderFrame
{
    StageHUDState    stageHUD;
    AutomationStatus automationStatus;
};

} } // namespace Cluiche::AppFlow
