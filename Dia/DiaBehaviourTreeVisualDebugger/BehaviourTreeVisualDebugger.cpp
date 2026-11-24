#include "BehaviourTreeVisualDebugger.h"

#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>
#include <DiaBehaviourTree/BehaviourTreeComponent.h>

namespace
{
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kDrawerTreeView("TreeView");

    const char* NodeResultToString(Dia::BehaviourTree::NodeResult result, bool entered)
    {
        if (entered) return "Entered";
        switch (result)
        {
        case Dia::BehaviourTree::NodeResult::kRunning: return "Running";
        case Dia::BehaviourTree::NodeResult::kSuccess: return "Success";
        case Dia::BehaviourTree::NodeResult::kFailure: return "Failure";
        }
        return "Unknown";
    }

    const char* LastResultToString(Dia::BehaviourTree::NodeResult result)
    {
        switch (result)
        {
        case Dia::BehaviourTree::NodeResult::kRunning: return "Running";
        case Dia::BehaviourTree::NodeResult::kSuccess: return "Success";
        case Dia::BehaviourTree::NodeResult::kFailure: return "Failure";
        }
        return "Unknown";
    }
}

namespace Dia::BehaviourTree
{

BehaviourTreeVisualDebugger::BehaviourTreeVisualDebugger(const BehaviourTreeComponent& component)
    : mComponent(component)
{
    const_cast<BehaviourTreeComponent&>(mComponent).AddEventListener(this);
}

BehaviourTreeVisualDebugger::~BehaviourTreeVisualDebugger()
{
    const_cast<BehaviourTreeComponent&>(mComponent).RemoveEventListener(this);
}

Dia::Core::StringCRC BehaviourTreeVisualDebugger::GetDomainId()    const { return Dia::Core::StringCRC("behaviour-tree"); }
const char* BehaviourTreeVisualDebugger::GetDisplayName()          const { return "Behaviour Tree"; }
const char* BehaviourTreeVisualDebugger::GetDescription()          const { return "Behaviour tree \xe2\x80\x94 active node, per-node tick result, entity cursor, resume state"; }
Dia::Core::StringCRC BehaviourTreeVisualDebugger::GetGroup()       const { return Dia::Core::StringCRC("AIBehavior"); }
Dia::Core::RGBA BehaviourTreeVisualDebugger::GetAccentColour()     const { return Dia::VisualDebugger::DebugGroupAccents::kAIBehavior; }

void BehaviourTreeVisualDebugger::GetJSONState(Json::Value& out)
{
    // drawers
    Json::Value drawers(Json::arrayValue);
    {
        Json::Value entry(Json::objectValue);
        entry["name"]    = "TreeView";
        entry["enabled"] = mTreeViewEnabled;
        drawers.append(entry);
    }
    out["drawers"] = drawers;

    // stats
    Json::Value stats(Json::objectValue);
    stats["nodeCount"]   = mLastTickCount;
    stats["lastResult"]  = mComponent.IsComplete()
        ? LastResultToString(mComponent.LastResult())
        : "None";
    out["stats"] = stats;

    // rootNodeId — only when an asset is loaded
    if (mComponent.HasAsset())
        out["rootNodeId"] = mComponent.GetRootNodeId().AsChar();

    // lastTickNodes — nodes visited during the last completed tick
    Json::Value lastTickNodes(Json::arrayValue);
    for (int i = 0; i < mLastTickCount; ++i)
    {
        Json::Value node(Json::objectValue);
        node["nodeId"] = mLastTickNodes[i].nodeId.AsChar();
        node["result"] = NodeResultToString(mLastTickNodes[i].result, mLastTickNodes[i].entered);
        lastTickNodes.append(node);
    }
    out["lastTickNodes"] = lastTickNodes;
}

void BehaviourTreeVisualDebugger::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
{
    if (cmd == kCmdToggle)
    {
        if (!args.isMember("drawer") || !args["drawer"].isString()) return;
        if (Dia::Core::StringCRC(args["drawer"].asCString()) == kDrawerTreeView)
            mTreeViewEnabled = !mTreeViewEnabled;
    }
    // "setScale" — no-op for panel-only domain
}

void BehaviourTreeVisualDebugger::OnNodeEntered(Dia::Core::StringCRC nodeId)
{
    if (mPendingCount < kMaxVisitsPerTick)
    {
        mPendingVisits[mPendingCount++] = {nodeId, NodeResult::kRunning, true};
    }
}

void BehaviourTreeVisualDebugger::OnNodeCompleted(Dia::Core::StringCRC nodeId, NodeResult result)
{
    if (mPendingCount < kMaxVisitsPerTick)
    {
        mPendingVisits[mPendingCount++] = {nodeId, result, false};
    }
}

void BehaviourTreeVisualDebugger::OnTreeCompleted(NodeResult /*result*/)
{
    // Snapshot the current tick's visits into mLastTickNodes, then clear for the next tick.
    mLastTickCount = mPendingCount;
    for (int i = 0; i < mPendingCount; ++i)
        mLastTickNodes[i] = mPendingVisits[i];
    mPendingCount = 0;
}

} // namespace Dia::BehaviourTree

#endif // DIA_DEBUG
