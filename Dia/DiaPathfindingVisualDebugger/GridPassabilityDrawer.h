#pragma once
#ifdef DIA_DEBUG
#include <DiaCore/DebugDraw/IVisualDebugger.h>

namespace Dia { namespace Core      { class IDebugContext; } }
namespace Dia { namespace Pathfinding { class SquarePathGrid; } }

namespace Dia { namespace Pathfinding {
    class GridPassabilityDrawer : public Dia::Debug::IVisualDebugger {
    public:
        GridPassabilityDrawer(const SquarePathGrid& grid, const Dia::Core::IDebugContext& ctx, float cellSize);
        Dia::Core::StringCRC GetLayerName() const override;
        void Draw(Dia::Core::IDebugDraw& draw) override;
    private:
        const SquarePathGrid&           mGrid;
        const Dia::Core::IDebugContext& mCtx;
        float                           mCellSize;
    };
} }
#endif
