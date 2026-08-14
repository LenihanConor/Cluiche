////////////////////////////////////////////////////////////////////////////////
// Filename: UtilityAIDebugDomain.h
// Description: IDebugDomain implementation for the Utility AI subsystem.
//              Owns the UtilityScoreDrawer (ImGui-only, no world-space output)
//              and bridges its state to DiaDebugPanel.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
// Feature spec: docs/specs/applications/dia/systems/diadebugdomain/debugger-contract.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <memory>

namespace Dia::Debug    { class DebugLayerManager; }
namespace Dia::UtilityAI { class UtilitySet; class UtilityScoreDrawer; }

namespace Dia::UtilityAI
{

class UtilityAIDebugDomain : public Dia::VisualDebugger::IDebugDomain
{
public:
    static const int kDrawerCount = 1;

    explicit UtilityAIDebugDomain(const UtilitySet& utilitySet);
    ~UtilityAIDebugDomain() override;

    // ---- IDebugDomain: identity ----
    Dia::Core::StringCRC GetDomainId()     const override;
    const char*          GetDisplayName()  const override;
    const char*          GetDescription()  const override;
    Dia::Core::StringCRC GetGroup()        const override;
    Dia::Core::RGBA      GetAccentColour() const override;
    bool                 HasWorldDrawers() const override { return false; }

    // ---- IDebugDomain: lifecycle ----
    void Register(Dia::Debug::DebugLayerManager& mgr)   override;
    void Unregister(Dia::Debug::DebugLayerManager& mgr) override;

    // ---- IDebugDomain: panel bridge ----
    void GetJSONState(Json::Value& out) override;
    void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

    // ---- IDebugDomain: drawer access ----
    int                          GetDrawerCount() const override;
    Dia::Debug::IVisualDebugger* GetDrawer(int index) override;

private:
    Dia::Core::StringCRC ResolveLayerName(const char* drawerName) const;

    const UtilitySet&              mUtilitySet;
    Dia::Debug::DebugLayerManager* mLayerManager = nullptr;

    std::unique_ptr<UtilityScoreDrawer> mScoreDrawer;
};

} // namespace Dia::UtilityAI

#endif // DIA_DEBUG
