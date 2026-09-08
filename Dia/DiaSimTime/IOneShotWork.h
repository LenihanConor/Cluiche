#pragma once

namespace Dia::SimTime {

    // IOneShotWork
    // -------------------------------------------------------------------------
    // Interface for transient, one-shot async work items (ST-012, finding 12):
    // e.g. a single UtilityAI evaluation request or an HTN plan job. Such work
    // is short-lived and scales with population, so it deliberately does NOT
    // register with SimTimeRegistry / SimTimeBudget's steady-state,
    // kMaxSystems-bounded priority-tier tables. Instead it is submitted to
    // SimTimeBudget's separate one-shot completion queue (SubmitOneShot) and
    // driven to completion by RunOneShots(), which never touches the four
    // priority-tier budget pools.
    class IOneShotWork
    {
    public:
        virtual ~IOneShotWork() = default;

        // Called with the remaining ms in the current one-shot budget slice.
        // Return true once fully complete (the item is removed from the queue);
        // return false to stay queued and be called again next RunOneShots()
        // call with a fresh slice. budgetMs may be small or 0.0f — implementations
        // must tolerate a zero/near-zero slice (do partial or no work, stay queued).
        virtual bool Step(float budgetMs) = 0;
    };

}
