#pragma once
#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>
#include <atomic>
#include <memory>

namespace Dia::AICallout { class CalloutRegistry; }
namespace Dia::AICalloutVisualDebugger { class CalloutRadiiDrawer; }
namespace Dia::Debug { class DebugLayerManager; }

namespace Dia::AICalloutVisualDebugger {

class CalloutRegistryDebugger : public Dia::VisualDebugger::IDebugDomain
{
public:
    explicit CalloutRegistryDebugger(const Dia::AICallout::CalloutRegistry& registry);
    ~CalloutRegistryDebugger() override;

    Dia::Core::StringCRC GetDomainId()     const override;
    const char*          GetDisplayName()  const override;
    const char*          GetDescription()  const override;
    Dia::Core::StringCRC GetGroup()        const override;
    Dia::Core::RGBA      GetAccentColour() const override;
    bool                 HasWorldDrawers() const override { return true; }

    void Register  (Dia::Debug::DebugLayerManager& mgr) override;
    void Unregister(Dia::Debug::DebugLayerManager& mgr) override;

    int                          GetDrawerCount() const override { return 1; }
    Dia::Debug::IVisualDebugger* GetDrawer(int index) override;

    void GetJSONState(Json::Value& out) override;
    void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

private:
    const Dia::AICallout::CalloutRegistry& mRegistry;
    std::unique_ptr<CalloutRadiiDrawer>    mRadiiDrawer;
    std::atomic<bool>                      mRadiiEnabled{true};
};

} // namespace Dia::AICalloutVisualDebugger

#endif // DIA_DEBUG
