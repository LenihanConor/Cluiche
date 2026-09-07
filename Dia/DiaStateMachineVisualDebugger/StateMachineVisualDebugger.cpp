////////////////////////////////////////////////////////////////////////////////
// Filename: StateMachineVisualDebugger.cpp
// Description: IDebugDomain implementation for DiaStateMachine.
// System spec: docs/specs/applications/dia/systems/diastatemachinevisualdebugger/diastatemachinevisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#include "StateMachineVisualDebugger.h"

#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/DebugGroupAccents.h>

namespace Dia::StateMachine
{

namespace
{
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kDrawerStateList("StateList");
    const Dia::Core::StringCRC kDrawerTransitionHistory("TransitionHistory");
}

// ----------------------------------------------------------------------------
// Construction
// ----------------------------------------------------------------------------

StateMachineVisualDebugger::StateMachineVisualDebugger(IStateMachineInspectable& inspectable)
    : mInspectable(inspectable)
{
    mInspectable.SetTransitionListener(this);
}

StateMachineVisualDebugger::~StateMachineVisualDebugger()
{
    mInspectable.SetTransitionListener(nullptr);
}

// ----------------------------------------------------------------------------
// Identity
// ----------------------------------------------------------------------------

Dia::Core::StringCRC StateMachineVisualDebugger::GetDomainId() const
{
    return Dia::Core::StringCRC("statemachine");
}

const char* StateMachineVisualDebugger::GetDisplayName() const
{
    return "State Machine";
}

const char* StateMachineVisualDebugger::GetDescription() const
{
    return "State machine — states, transition history, guard pass/fail";
}

Dia::Core::StringCRC StateMachineVisualDebugger::GetGroup() const
{
    return Dia::Core::StringCRC("AIBehavior");
}

Dia::Core::RGBA StateMachineVisualDebugger::GetAccentColour() const
{
    return Dia::VisualDebugger::DebugGroupAccents::kAIBehavior;
}

// ----------------------------------------------------------------------------
// Panel bridge
// ----------------------------------------------------------------------------

void StateMachineVisualDebugger::GetJSONState(Json::Value& out)
{
    // Drawers array — always emitted
    Json::Value drawers(Json::arrayValue);
    {
        Json::Value entry(Json::objectValue);
        entry["name"]    = "StateList";
        entry["enabled"] = mStateListEnabled.load();
        drawers.append(entry);
    }
    {
        Json::Value entry(Json::objectValue);
        entry["name"]    = "TransitionHistory";
        entry["enabled"] = mTransitionHistEnabled.load();
        drawers.append(entry);
    }
    out["drawers"] = drawers;

    // Query inspectable
    Dia::Core::Containers::DynamicArrayC<StateInfo, 64> states;
    mInspectable.GetAllStates(states);

    Dia::Core::Containers::DynamicArrayC<TransitionRecord, 32> history;
    mInspectable.GetTransitionHistory(history);

    const Dia::Core::StringCRC currentId = mInspectable.GetCurrentStateId();

    // Stats — always emitted
    Json::Value stats(Json::objectValue);
    stats["stateCount"]    = static_cast<int>(states.Size());
    stats["currentState"]  = currentId.AsChar();
    stats["historyLength"] = static_cast<int>(history.Size());
    out["stats"] = stats;

    // States array — gated on mStateListEnabled
    if (mStateListEnabled.load())
    {
        Json::Value statesArr(Json::arrayValue);
        for (unsigned int i = 0; i < states.Size(); ++i)
        {
            Json::Value s(Json::objectValue);
            s["id"]     = states[i].id.AsChar();
            s["active"] = (states[i].id == currentId);
            s["isLeaf"] = states[i].isLeaf;
            statesArr.append(s);
        }
        out["states"] = statesArr;
    }

    // History array — gated on mTransitionHistEnabled
    if (mTransitionHistEnabled.load())
    {
        Json::Value histArr(Json::arrayValue);
        for (unsigned int i = 0; i < history.Size(); ++i)
        {
            Json::Value h(Json::objectValue);
            h["from"]      = history[i].sourceStateId.AsChar();
            h["to"]        = history[i].targetStateId.AsChar();
            h["trigger"]   = history[i].triggerId.AsChar();
            h["timestamp"] = history[i].timestamp;
            histArr.append(h);
        }
        out["history"] = histArr;
    }

    // Last guard results — always emitted
    Json::Value guardArr(Json::arrayValue);
    for (unsigned int i = 0; i < mLastGuardResults.Size(); ++i)
    {
        Json::Value g(Json::objectValue);
        g["guard"]  = mLastGuardResults[i].guardName.AsChar();
        g["passed"] = mLastGuardResults[i].passed;
        guardArr.append(g);
    }
    out["lastGuardResults"] = guardArr;

    out["lastTransitionFailed"] = mLastTransitionFailed;
}

void StateMachineVisualDebugger::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
{
    if (cmd == kCmdToggle)
    {
        if (!args.isMember("drawer") || !args["drawer"].isString()) return;
        const Dia::Core::StringCRC drawer(args["drawer"].asCString());
        if (drawer == kDrawerStateList)
        {
            mStateListEnabled.store(!mStateListEnabled.load());
        }
        else if (drawer == kDrawerTransitionHistory)
        {
            mTransitionHistEnabled.store(!mTransitionHistEnabled.load());
        }
        return;
    }
    // "setScale" is a no-op for panel-only domain; unknown commands silently ignored
}

// ----------------------------------------------------------------------------
// ITransitionListener
// ----------------------------------------------------------------------------

void StateMachineVisualDebugger::OnTransition(const TransitionEvent& event)
{
    mLastTransitionFailed = false;
    mLastGuardResults.RemoveAll();
    for (int i = 0; i < event.guardCount; ++i)
    {
        CachedGuardResult result;
        result.guardName = event.guardResults[i].guardName;
        result.passed    = event.guardResults[i].passed;
        mLastGuardResults.Add(result);
    }
}

void StateMachineVisualDebugger::OnTransitionFailed(
    Dia::Core::StringCRC /*machineId*/,
    Dia::Core::StringCRC /*currentStateId*/,
    Dia::Core::StringCRC /*triggerId*/)
{
    mLastTransitionFailed = true;
}

} // namespace Dia::StateMachine

#endif // DIA_DEBUG
