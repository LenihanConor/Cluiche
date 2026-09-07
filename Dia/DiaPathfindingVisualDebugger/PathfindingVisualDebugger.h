////////////////////////////////////////////////////////////////////////////////
// Filename: PathfindingVisualDebugger.h
// Description: IDebugDomain implementation for DiaPathfinding. World-space
//              domain with two drawers: PathPolyline and GridPassability.
// System spec: docs/specs/applications/dia/systems/diapathfindingvisualdebugger/
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>
#include <atomic>
#include <memory>

namespace Dia { namespace Pathfinding { class SquarePathGrid; struct PathResult; } }
namespace Dia { namespace Debug       { class IDebugLayerRegistry; } }

namespace Dia { namespace Pathfinding {
    class PathPolylineDrawer;
    class GridPassabilityDrawer;

    class PathfindingVisualDebugger : public Dia::VisualDebugger::IDebugDomain
    {
    public:
        PathfindingVisualDebugger(const SquarePathGrid& grid, const PathResult& result, float cellSize);
        ~PathfindingVisualDebugger() override;

        Dia::Core::StringCRC GetDomainId()     const override;
        const char*          GetDisplayName()  const override;
        const char*          GetDescription()  const override;
        Dia::Core::StringCRC GetGroup()        const override;
        Dia::Core::RGBA      GetAccentColour() const override;

        bool HasWorldDrawers() const override { return true; }

        void Register(Dia::Debug::IDebugLayerRegistry& mgr)   override;
        void Unregister(Dia::Debug::IDebugLayerRegistry& mgr) override;

        int                          GetDrawerCount() const override { return 2; }
        Dia::Debug::IVisualDebugger* GetDrawer(int index) override;

        void GetJSONState(Json::Value& out) override;
        void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

    private:
        const SquarePathGrid& mGrid;
        const PathResult&     mResult;
        float                 mCellSize;
        Dia::Debug::IDebugLayerRegistry* mLayerManager = nullptr;

        float mMarkerRadius = 1.0f;

        std::atomic<bool> mPathPolylineEnabled{true};
        std::atomic<bool> mGridPassabilityEnabled{true};

        std::unique_ptr<PathPolylineDrawer>    mPathPolyline;
        std::unique_ptr<GridPassabilityDrawer> mGridPassability;
    };
} }

#endif // DIA_DEBUG
