////////////////////////////////////////////////////////////////////////////////
// Filename: HTNVisualDebugger.h
// Description: IDebugDomain implementation for the HTN planner subsystem.
//              Panel-only domain — exposes active plan state, task cursor,
//              divergence flag via GetJSONState(). No world-space drawers.
// System spec: docs/specs/applications/dia/systems/diahtnvisualdebugger/diahtnvisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <atomic>

namespace Dia::HTN { class HTNPlannerComponent; }

namespace Dia::HTN
{

class HTNVisualDebugger : public Dia::VisualDebugger::IDebugDomain
{
public:
    explicit HTNVisualDebugger(const HTNPlannerComponent& component);

    // ---- IDebugDomain: identity ----
    Dia::Core::StringCRC GetDomainId()     const override;
    const char*          GetDisplayName()  const override;
    const char*          GetDescription()  const override;
    Dia::Core::StringCRC GetGroup()        const override;
    Dia::Core::RGBA      GetAccentColour() const override;
    bool                 HasWorldDrawers() const override { return false; }

    // ---- IDebugDomain: panel bridge ----
    void GetJSONState(Json::Value& out) override;
    void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

private:
    const HTNPlannerComponent& mComponent;
    std::atomic<bool> mPlanViewEnabled{true};
};

} // namespace Dia::HTN

#endif // DIA_DEBUG
