// S-1 Timeout and Failure Handling test helper: drives a WorldGenSession through a Regenerate that now takes one Tick per
// attempt. Header-only.
#pragma once

#include <Shared/WorldGeneration/WorldGenSession.h>

#include <DiaMaths/Core/RandomStream.h>

namespace CoW {
namespace WorldGeneration {
namespace Testing {

// Ticks until no Regenerate is in progress (at most maxFrames). Returns true if any Tick changed the world.
inline bool TickUntilIdle(WorldGenSession& session, Dia::Maths::RandomStream& rng, int maxFrames = 1000)
{
    bool changed = session.Tick(rng);
    for (int frame = 1; frame < maxFrames && session.IsGenerating(); ++frame)
    {
        changed = session.Tick(rng) || changed;
    }
    return changed;
}

} // namespace Testing
} // namespace WorldGeneration
} // namespace CoW
