#pragma once

#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaCore/Time/TimeRelative.h>
#include <cstdint>

namespace Dia::SimTime {

    // Passed to SimModule::DoUpdate() each SimPU tick.
    struct SimTimeContext {
        Core::TimeAbsolute  gameTime;    // canonical game clock — int64 µs, deterministic
        Core::TimeRelative  gameDt;      // this tick's step (timeStep * timeScale)
        uint64_t            tick;        // monotonic tick counter (for replay / debug)
        float               timeScale;   // current scale (1.0 normal, 0.5 slow-mo)
        bool                isPaused;    // true if sim is paused (Step() ticks still fire)
    };

    // Passed to RenderModule::DoUpdate() each RenderPU frame.
    struct RenderTimeContext {
        float               frameDt;      // wall-clock seconds since last render frame
        Core::TimeAbsolute  simTime;      // READ-ONLY snapshot from the sim-time FrameStream
        uint64_t            renderFrame;  // render frame counter (independent of sim tick)
    };

    // Passed to MainModule::DoUpdate() each MainPU tick.
    struct MainTimeContext {
        float  wallClockDt;   // wall-clock seconds since last main tick
    };

} // namespace Dia::SimTime
