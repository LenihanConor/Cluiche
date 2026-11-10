////////////////////////////////////////////////////////////////////////////////
// Filename: StateMachineVisualDebugger.h
// Description: IDebugDomain + ITransitionListener implementation for DiaStateMachine.
//              Panel-only (no world drawers). Registers as ITransitionListener
//              at construction to cache live guard results.
// System spec: docs/specs/applications/dia/systems/diastatemachinevisualdebugger/diastatemachinevisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <DiaStateMachine/IStateMachineInspectable.h>
#include <atomic>

namespace Dia::StateMachine
{

class StateMachineVisualDebugger
    : public Dia::VisualDebugger::IDebugDomain
    , public Dia::StateMachine::ITransitionListener
{
public:
    explicit StateMachineVisualDebugger(IStateMachineInspectable& inspectable);
    ~StateMachineVisualDebugger() override;

    // ---- IDebugDomain: identity ----
    Dia::Core::StringCRC  GetDomainId()     const override;
    const char*           GetDisplayName()  const override;
    const char*           GetDescription()  const override;
    Dia::Core::StringCRC  GetGroup()        const override;
    Dia::Core::RGBA       GetAccentColour() const override;

    bool HasWorldDrawers() const override { return false; }

    // ---- IDebugDomain: panel bridge ----
    void GetJSONState(Json::Value& out) override;
    void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

    // ---- ITransitionListener ----
    void OnTransition(const TransitionEvent& event) override;
    void OnTransitionFailed(Dia::Core::StringCRC machineId,
                            Dia::Core::StringCRC currentStateId,
                            Dia::Core::StringCRC triggerId) override;

private:
    IStateMachineInspectable& mInspectable;
    std::atomic<bool> mStateListEnabled{true};
    std::atomic<bool> mTransitionHistEnabled{true};

    struct CachedGuardResult
    {
        Dia::Core::StringCRC guardName;
        bool passed = false;
    };
    Dia::Core::Containers::DynamicArrayC<CachedGuardResult, 8> mLastGuardResults;
    bool mLastTransitionFailed = false;
};

} // namespace Dia::StateMachine

#endif // DIA_DEBUG
