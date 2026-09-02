#pragma once

namespace Dia::SimTime {

    // SimTimeTier
    // -------------------------------------------------------------------------
    // LOD (level-of-detail) throttle axis for a budgeted system: how *often* the
    // system needs to run when it is awake. This is orthogonal to SimTimeState
    // (the sleep gate) — the per-tick gate sequence checks the sleep state
    // first, then the tier throttle (see SimTimeRegistry / the spec's gate
    // sequence). A system can be kAwake yet still skipped this tick because its
    // tier isn't due, and (separately) a kDormant-tier system that is kAwake is
    // still never ticked via the tier path by design.
    //
    // Tier -> Hz mapping is documented on SimTimeRegistry::DueThisTick (the only
    // place that interprets it); there is no config-driven override yet — that
    // is Task 4.4's OnConfigure job. A per-system SimTimePolicy::maxInterval,
    // when non-zero, overrides the tier default.
    enum class SimTimeTier
    {
        kImmediate,  // every SimPU tick (default) — no throttle
        kHigh,       // ~30 Hz
        kMedium,     // ~10 Hz
        kLow,        // ~2 Hz
        kDormant     // event-driven only; system stays registered but is never
                     // ticked through the tier-throttle path
    };

} // namespace Dia::SimTime
