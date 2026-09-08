#pragma once

namespace Dia::SimTime {

    // SimTimeState
    // -------------------------------------------------------------------------
    // The dormancy / sleep axis for a registered budgeted system. This is the
    // FIRST gate in the per-tick sequence (checked before the SimTimeTier LOD
    // throttle): a kSleeping system is skipped at zero cost until an explicit
    // wake condition (SimTimeRegistry::RegisterWakeOnTime /
    // RegisterWakeOnMessage) or a direct Wake() call flips it back to kAwake.
    //
    // Distinct from SimTimeTier::kDormant — that is an LOD throttle value; this
    // is the runtime sleep state.
    enum class SimTimeState
    {
        kAwake,
        kSleeping
    };

} // namespace Dia::SimTime
