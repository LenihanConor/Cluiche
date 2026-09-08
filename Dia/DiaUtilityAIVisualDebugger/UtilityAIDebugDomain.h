////////////////////////////////////////////////////////////////////////////////
// Filename: UtilityAIDebugDomain.h
// Description: IDebugDomain implementation for the Utility AI subsystem.
//              Panel-only domain — exposes per-action utility scores via
//              GetJSONState(). No world-space drawers.
// System spec: docs/specs/applications/dia/systems/diautilityaivisualdebugger/diautilityaivisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>
#include <atomic>

namespace Dia::UtilityAI { class UtilitySet; }

namespace Dia::UtilityAI
{

class UtilityAIDebugDomain : public Dia::VisualDebugger::IDebugDomain
{
public:
    explicit UtilityAIDebugDomain(const UtilitySet& utilitySet);

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
    const UtilitySet& mUtilitySet;
    std::atomic<bool> mScoresEnabled{true};
};

} // namespace Dia::UtilityAI

#endif // DIA_DEBUG
