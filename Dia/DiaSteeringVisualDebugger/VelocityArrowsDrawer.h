#pragma once
#ifdef DIA_DEBUG
#include <DiaCore/DebugDraw/IVisualDebugger.h>
namespace Dia { namespace Core { class IDebugContext; } }
namespace Dia { namespace Steering { class SteeringSystem; } }
namespace Dia { namespace Steering {
    class VelocityArrowsDrawer : public Dia::Debug::IVisualDebugger {
    public:
        VelocityArrowsDrawer(const SteeringSystem& system, const Dia::Core::IDebugContext& ctx, float arrowScale = 1.0f);
        Dia::Core::StringCRC GetLayerName() const override;
        void Draw(Dia::Core::IDebugDraw& draw) override;
        void SetArrowScale(float scale) { mArrowScale = scale; }
    private:
        const SteeringSystem&           mSystem;
        const Dia::Core::IDebugContext& mCtx;
        float                           mArrowScale;
    };
} }
#endif
