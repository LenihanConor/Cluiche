#pragma once
#ifdef DIA_DEBUG
#include <DiaCore/DebugDraw/IVisualDebugger.h>

namespace Dia { namespace Core      { class IDebugContext; } }
namespace Dia { namespace FlowField { class FlowField; } }

namespace Dia { namespace FlowField {
    class ReachabilityOverlayDrawer : public Dia::Debug::IVisualDebugger {
    public:
        ReachabilityOverlayDrawer(const FlowField& field, const Dia::Core::IDebugContext& ctx, float cellSize);
        Dia::Core::StringCRC GetLayerName() const override;
        void Draw(Dia::Core::IDebugDraw& draw) override;
    private:
        const FlowField&                mField;
        const Dia::Core::IDebugContext& mCtx;
        float                           mCellSize;
    };
} }
#endif // DIA_DEBUG
