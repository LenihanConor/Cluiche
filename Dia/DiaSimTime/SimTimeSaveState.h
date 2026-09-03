#pragma once

#include <DiaSaveGame/ISaveable.h>
#include <cstdint>

// SaveContext / LoadContext are forward-declared by ISaveable.h (in namespace
// Dia::SaveGame); the DiaSimTime siblings we bind to are forward-declared below.

namespace Dia::SimTime {

    class SimTimeDomain;      // Dia::SimTime::SimTimeDomain (DiaCore/SimTime/SimTimeDomain.h)
    class SimTimeScheduler;
    class SimTimeRegistry;

    // SimTimeSaveState
    // -------------------------------------------------------------------------
    // The single, combined save/load participant for DiaSimTime (ST-015): it
    // persists and restores the world game clock, the scheduler's pending
    // entries, and each registered system's sleep state, sequencing the restore
    // internally (domain -> scheduler -> sleep state) rather than relying on any
    // cross-participant SaveRegistry ordering.
    //
    // All three collaborators are externally owned and injected by reference —
    // the same dependency-injection pattern SimTimeDomainRegistry (Task 2.1) and
    // SimTimeRegistry (Task 4.3) already use. DiaSimTimeModule::DoStart builds one
    // of these (bound to the PU's world domain + its own scheduler/registry) and
    // registers it with the SaveRegistry when a registry was injected via
    // SetSaveRegistry (Task 4.7).
    //
    // Handle identity is deliberately NOT preserved across save/load (ST-016):
    // restored scheduler entries always receive fresh handles.
    class SimTimeSaveState : public Dia::SaveGame::ISaveable
    {
    public:
        SimTimeSaveState(SimTimeDomain&    worldDomain,
                         SimTimeScheduler& scheduler,
                         SimTimeRegistry&  registry);

        void     Serialize  (Dia::SaveGame::SaveContext& ctx) const override;
        void     Deserialize(Dia::SaveGame::LoadContext& ctx)       override;
        uint32_t GetVersion () const override { return 1; }

    private:
        SimTimeDomain&    mWorldDomain;
        SimTimeScheduler& mScheduler;
        SimTimeRegistry&  mRegistry;
    };

} // namespace Dia::SimTime
