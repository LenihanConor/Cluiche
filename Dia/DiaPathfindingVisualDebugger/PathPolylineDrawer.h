#pragma once
#ifdef DIA_DEBUG
#include <DiaCore/DebugDraw/IVisualDebugger.h>

namespace Dia { namespace Core    { class IDebugContext; } }
namespace Dia { namespace Pathfinding { class SquarePathGrid; struct PathResult; } }

namespace Dia { namespace Pathfinding {
    class PathPolylineDrawer : public Dia::Debug::IVisualDebugger {
    public:
        PathPolylineDrawer(const SquarePathGrid& grid, const PathResult& result,
                           const Dia::Core::IDebugContext& ctx, float cellSize);
        Dia::Core::StringCRC GetLayerName() const override;
        void Draw(Dia::Core::IDebugDraw& draw) override;
        void SetMarkerRadius(float r) { mMarkerRadius = r; }
    private:
        const SquarePathGrid&           mGrid;
        const PathResult&               mResult;
        const Dia::Core::IDebugContext& mCtx;
        float                           mCellSize;
        float                           mMarkerRadius;
    };
} }
#endif
