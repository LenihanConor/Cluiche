////////////////////////////////////////////////////////////////////////////////
// Filename: RulesVisualDebugger.h
// Description: IDebugDomain implementation for the DiaRules subsystem.
//              Panel-only domain — exposes per-rule fired/not-fired state via
//              GetJSONState(). No world-space drawers.
// System spec: docs/specs/applications/dia/systems/diarulesvisualdebugger/diarulesvisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>
#include <atomic>

namespace Dia::Rules { class RuleSetComponent; }

namespace Dia::Rules
{

class RulesVisualDebugger : public Dia::VisualDebugger::IDebugDomain
{
public:
    explicit RulesVisualDebugger(const RuleSetComponent& component);

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
    const RuleSetComponent& mComponent;
    std::atomic<bool>       mFireLogEnabled{true};
};

} // namespace Dia::Rules

#endif // DIA_DEBUG
