////////////////////////////////////////////////////////////////////////////////
// Filename: ScalarFieldVisualDebugger.h
// Description: IDebugDomain implementation for DiaScalarField. Panel-only domain —
//              two logical drawers (Heatmap, Gradient) with arrowScale control.
//              World-space overlays are template classes managed by the consumer.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <atomic>

namespace Dia
{
    namespace ScalarField
    {
        class ScalarFieldVisualDebugger : public Dia::VisualDebugger::IDebugDomain
        {
        public:
            ScalarFieldVisualDebugger();

            Dia::Core::StringCRC GetDomainId()     const override;
            const char*          GetDisplayName()  const override;
            const char*          GetDescription()  const override;
            Dia::Core::StringCRC GetGroup()        const override;
            Dia::Core::RGBA      GetAccentColour() const override;
            bool                 HasWorldDrawers() const override { return false; }

            void GetJSONState(Json::Value& out) override;
            void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

            float GetArrowScale() const { return mArrowScale; }

        private:
            std::atomic<bool> mHeatmapEnabled{true};
            std::atomic<bool> mGradientEnabled{true};
            float             mArrowScale = 0.35f;
        };

    } // namespace ScalarField
} // namespace Dia

#endif // DIA_DEBUG
