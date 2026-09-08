#pragma once
#ifdef DIA_DEBUG
#include <DiaCore/DebugDraw/IVisualDebugger.h>

namespace Dia { namespace Core      { class IDebugContext; } }
namespace Dia { namespace FlowField { class FlowField; } }

namespace Dia { namespace FlowField {
    class DirectionArrowsDrawer : public Dia::Debug::IVisualDebugger {
    public:
        DirectionArrowsDrawer(const FlowField& field, const Dia::Core::IDebugContext& ctx, float cellSize);
        Dia::Core::StringCRC GetLayerName() const override;
        void Draw(Dia::Core::IDebugDraw& draw) override;
        void SetArrowLengthScale(float scale) { mArrowLengthScale = scale; }
    private:
        const FlowField&                mField;
        const Dia::Core::IDebugContext& mCtx;
        float                           mCellSize;
        float                           mArrowLengthScale = 1.0f;
    };
} }
#endif // DIA_DEBUG
