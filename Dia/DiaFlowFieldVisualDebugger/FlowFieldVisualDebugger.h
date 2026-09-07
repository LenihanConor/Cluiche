////////////////////////////////////////////////////////////////////////////////
// Filename: FlowFieldVisualDebugger.h
// Description: IDebugDomain implementation for DiaFlowField. World-space domain
//              with two drawers: DirectionArrows and ReachabilityOverlay.
// System spec: docs/specs/applications/dia/systems/diaflowfieldvisualdebugger/
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>
#include <atomic>
#include <memory>

namespace Dia { namespace FlowField { class FlowField; } }
namespace Dia { namespace Debug     { class DebugLayerManager; } }

namespace Dia { namespace FlowField {
    class DirectionArrowsDrawer;
    class ReachabilityOverlayDrawer;

    class FlowFieldVisualDebugger : public Dia::VisualDebugger::IDebugDomain
    {
    public:
        FlowFieldVisualDebugger(const FlowField& field, float cellSize);
        ~FlowFieldVisualDebugger() override;

        Dia::Core::StringCRC GetDomainId()     const override;
        const char*          GetDisplayName()  const override;
        const char*          GetDescription()  const override;
        Dia::Core::StringCRC GetGroup()        const override;
        Dia::Core::RGBA      GetAccentColour() const override;

        bool HasWorldDrawers() const override { return true; }

        void Register(Dia::Debug::DebugLayerManager& mgr)   override;
        void Unregister(Dia::Debug::DebugLayerManager& mgr) override;

        int                          GetDrawerCount() const override { return 2; }
        Dia::Debug::IVisualDebugger* GetDrawer(int index) override;

        void GetJSONState(Json::Value& out) override;
        void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

    private:
        const FlowField& mField;
        float            mCellSize;
        float            mArrowLengthScale = 1.0f;
        Dia::Debug::DebugLayerManager* mLayerManager = nullptr;

        std::atomic<bool> mDirectionArrowsEnabled{true};
        std::atomic<bool> mReachabilityOverlayEnabled{true};

        std::unique_ptr<DirectionArrowsDrawer>     mDirectionArrows;
        std::unique_ptr<ReachabilityOverlayDrawer> mReachabilityOverlay;
    };
} }

#endif // DIA_DEBUG
