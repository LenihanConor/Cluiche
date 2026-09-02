#pragma once

#include <DiaCore/Time/TimeRelative.h>
#include <DiaSimTime/SimTimeTier.h>

namespace Dia::SimTime {

    // SimTimePolicy
    // -------------------------------------------------------------------------
    // Per-system declaration handed to SimTimeRegistry::Register: how often the
    // system needs to run (tier / maxInterval), and whether it is allowed to
    // sleep. Default-constructible (all members carry defaults; TimeRelative is
    // factory-only, so maxInterval is seeded via TimeRelative::Zero()) so it can
    // live inside a DynamicArrayC-backed registry entry.
    struct SimTimePolicy
    {
        // LOD throttle. See SimTimeRegistry::DueThisTick for the tier -> Hz map.
        SimTimeTier         tier                   = SimTimeTier::kImmediate;

        // Explicit minimum interval between runs; 0 (the default) = use the tier
        // default. When non-zero, this overrides the tier default (the LOD
        // throttle uses maxInterval instead). Ignored for kDormant (which never
        // runs via the throttle path regardless).
        Core::TimeRelative  maxInterval            = Core::TimeRelative::Zero();

        // Whether this system may be put to sleep (Sleep()). Advisory metadata
        // for the caller/gate loop; the registry itself does not forbid sleeping
        // a canSleep==false system (Task 4.4's gate loop decides policy).
        bool                canSleep               = false;

        // Phase 5 hook (analytic fast-forward). Recorded but not acted on here.
        bool                canAnalyticallyAdvance = false;
    };

} // namespace Dia::SimTime
