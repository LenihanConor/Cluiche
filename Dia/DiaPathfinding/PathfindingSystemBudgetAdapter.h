#pragma once
#include <DiaSimTime/ISimTimeBudgetedSystem.h>
#include <DiaPathfinding/PathfindingSystem.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::Pathfinding {

    //-------------------------------------------------------------------------------------------
    // PathfindingSystemBudgetAdapter
    //
    // Wraps a PathfindingSystem<TGraph>::Update(budgetMs) as an ISimTimeBudgetedSystem so it can
    // be registered with SimTimeBudget (DiaSimTime). Header-only, since PathfindingSystem itself
    // is a template — the adapter must be too.
    //
    // Non-owning: the wrapped PathfindingSystem must outlive the adapter.
    //-------------------------------------------------------------------------------------------
    template<CPathGraph TGraph>
    class PathfindingSystemBudgetAdapter : public Dia::SimTime::ISimTimeBudgetedSystem
    {
    public:
        PathfindingSystemBudgetAdapter(PathfindingSystem<TGraph>& system, Core::StringCRC systemId)
            : mSystem(system), mSystemId(systemId) {}

        Core::StringCRC GetSystemId() const override { return mSystemId; }
        // Pathfinding is latency-tolerant (not frame-critical) — kBackground.
        Dia::SimTime::SimTimePriority GetPriority() const override { return Dia::SimTime::SimTimePriority::kBackground; }
        void UpdateBudgeted(float budgetMs) override { mSystem.Update(budgetMs); }

    private:
        PathfindingSystem<TGraph>& mSystem;   // non-owning; must outlive the adapter
        Core::StringCRC             mSystemId;
    };

} // namespace Dia::Pathfinding
