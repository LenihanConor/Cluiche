#pragma once
#ifdef DIA_DEBUG
#include <DiaCore/DebugDraw/IVisualDebugger.h>
namespace Dia { namespace Core { class IDebugContext; } }
namespace Dia { namespace Steering { class SteeringSystem; } }
namespace Dia { namespace Steering {
    class SeparationRadiusDrawer : public Dia::Debug::IVisualDebugger {
    public:
        SeparationRadiusDrawer(const SteeringSystem& system, const Dia::Core::IDebugContext& ctx);
        Dia::Core::StringCRC GetLayerName() const override;
        void Draw(Dia::Core::IDebugDraw& draw) override;
    private:
        const SteeringSystem&           mSystem;
        const Dia::Core::IDebugContext& mCtx;
    };
} }
#endif
