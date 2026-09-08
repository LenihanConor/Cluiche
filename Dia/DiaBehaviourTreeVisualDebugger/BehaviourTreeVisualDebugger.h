////////////////////////////////////////////////////////////////////////////////
// Filename: BehaviourTreeVisualDebugger.h
// Description: IDebugDomain implementation for the behaviour tree subsystem.
//              Panel-only domain — exposes active node visit sequence, per-node
//              tick result, and TreeView toggle via GetJSONState(). No world drawers.
// System spec: docs/specs/applications/dia/systems/diabehaviourtreevisualdebugger/
////////////////////////////////////////////////////////////////////////////////
#pragma once

#ifdef DIA_DEBUG

#include <DiaDebugDraw/Domain/IDebugDomain.h>
#include <DiaBehaviourTree/IBehaviourTreeEventListener.h>
#include <DiaBehaviourTree/NodeResult.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::BehaviourTree { class BehaviourTreeComponent; }

namespace Dia::BehaviourTree
{

class BehaviourTreeVisualDebugger
    : public Dia::VisualDebugger::IDebugDomain
    , public Dia::BehaviourTree::IBehaviourTreeEventListener
{
public:
    explicit BehaviourTreeVisualDebugger(const BehaviourTreeComponent& component);
    ~BehaviourTreeVisualDebugger() override;

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

    // ---- IBehaviourTreeEventListener ----
    void OnNodeEntered(Dia::Core::StringCRC nodeId) override;
    void OnNodeCompleted(Dia::Core::StringCRC nodeId, NodeResult result) override;
    void OnTreeCompleted(NodeResult result) override;

private:
    struct NodeVisit
    {
        Dia::Core::StringCRC nodeId;
        NodeResult           result;  // kRunning = "Entered but not yet completed"
        bool                 entered; // true = OnNodeEntered; false = OnNodeCompleted
    };

    const BehaviourTreeComponent& mComponent;
    bool                          mTreeViewEnabled = true;

    static constexpr int kMaxVisitsPerTick = 128;
    NodeVisit mPendingVisits[kMaxVisitsPerTick];
    int       mPendingCount = 0;

    NodeVisit mLastTickNodes[kMaxVisitsPerTick];
    int       mLastTickCount = 0;
};

} // namespace Dia::BehaviourTree

#endif // DIA_DEBUG
