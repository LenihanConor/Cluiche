////////////////////////////////////////////////////////////////////////////////
// Filename: SteeringVisualDebugger.h
// Description: IDebugDomain implementation for DiaSteering. World-space domain
//              with three drawers: VelocityArrows, SeparationRadius, DetectionBoxes.
// System spec: docs/specs/applications/dia/systems/diasteeringvisualdebugger/diasteeringvisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>
#include <atomic>
#include <memory>

namespace Dia { namespace Steering { class SteeringSystem; } }
namespace Dia { namespace Debug    { class DebugLayerManager; } }

namespace Dia
{
    namespace Steering
    {
        class VelocityArrowsDrawer;
        class SeparationRadiusDrawer;
        class DetectionBoxDrawer;

        class SteeringVisualDebugger : public Dia::VisualDebugger::IDebugDomain
        {
        public:
            explicit SteeringVisualDebugger(const SteeringSystem& system);
            ~SteeringVisualDebugger() override;

            Dia::Core::StringCRC GetDomainId()     const override;
            const char*          GetDisplayName()  const override;
            const char*          GetDescription()  const override;
            Dia::Core::StringCRC GetGroup()        const override;
            Dia::Core::RGBA      GetAccentColour() const override;

            bool HasWorldDrawers() const override { return true; }

            void Register(Dia::Debug::DebugLayerManager& mgr)   override;
            void Unregister(Dia::Debug::DebugLayerManager& mgr) override;

            int                          GetDrawerCount() const override { return 3; }
            Dia::Debug::IVisualDebugger* GetDrawer(int index) override;

            void GetJSONState(Json::Value& out) override;
            void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

        private:
            const SteeringSystem& mSystem;
            Dia::Debug::DebugLayerManager* mLayerManager = nullptr;

            float mArrowScale = 1.0f;

            std::atomic<bool> mVelocityArrowsEnabled{true};
            std::atomic<bool> mSeparationRadiusEnabled{true};
            std::atomic<bool> mDetectionBoxesEnabled{false};  // SD-002: off by default

            std::unique_ptr<VelocityArrowsDrawer>   mVelocityArrows;
            std::unique_ptr<SeparationRadiusDrawer>  mSeparationRadius;
            std::unique_ptr<DetectionBoxDrawer>      mDetectionBoxes;
        };
    }
}

#endif // DIA_DEBUG
