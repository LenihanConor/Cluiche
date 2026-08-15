#include <DiaBehaviourTree/BehaviourTreeComponent.h>
#include <DiaBlackboard/Blackboard.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaCore/Reflect/ReflectMacros.h>

#include <vector>
#include <unordered_map>

DIA_SERIALIZE(Dia::BehaviourTree::BehaviourTreeComponent, Dia::BehaviourTree::BehaviourTreeComponent::kVersion)
DIA_SERIALIZE_END

namespace Dia
{
    namespace BehaviourTree
    {

DIA_COMPONENT_REGISTER(BehaviourTreeComponent, "behaviour-tree-component", false, true,
    nullptr, 0,
    nullptr, 0,
    nullptr, 0)

// ============================================================================
// Internal execution structures (not exposed in header)
// ============================================================================

// Frame on the execution stack — one entry per composite node on the active path.
struct FrameEntry
{
    Dia::Core::StringCRC nodeId;
    int resumeChildIndex = 0;   // sequence/selector: which child to run next
                                // parallel: bitmask or count (handled in Task 6)
};

// Per-node persistent state (for decorator nodes).
struct NodeState
{
    int   counter     = 0;
    float accumulator = 0.0f;
};

// ============================================================================
// Impl
// ============================================================================

struct BehaviourTreeComponent::Impl
{
    const BehaviourTreeAsset*    asset            = nullptr;
    Dia::Blackboard::Blackboard* blackboard       = nullptr;
    const ActionRegistry*        actionRegistry   = nullptr;
    void*                        actionContext    = nullptr;
    const DecoratorRegistry*     decoratorRegistry = nullptr;

    std::vector<IBehaviourTreeEventListener*> listeners;

    // Execution cursor
    std::vector<FrameEntry>                     executionStack;
    std::unordered_map<unsigned int, NodeState> nodeStates;     // keyed by nodeId.Value()

    bool       isComplete = false;
    NodeResult lastResult = NodeResult::kFailure;
};

// ============================================================================
// Leaf node evaluators — internal to this translation unit.
// Parameters are passed individually to avoid referencing the private Impl type.
// ============================================================================

static NodeResult EvaluateCondition(
    Dia::Blackboard::Blackboard*              blackboard,
    const BehaviourTreeAsset::NodeDescriptor& node)
{
    if (!blackboard) return NodeResult::kFailure;
    const bool* slot = blackboard->TryGet<bool>(node.blackboardKey);
    if (!slot) return NodeResult::kFailure;  // key absent
    return *slot ? NodeResult::kSuccess : NodeResult::kFailure;
}

static NodeResult EvaluateAction(
    const ActionRegistry*                     actionRegistry,
    void*                                     actionContext,
    const BehaviourTreeAsset::NodeDescriptor& node)
{
    if (!actionRegistry) return NodeResult::kFailure;
    ActionFn fn = actionRegistry->Find(node.actionId);
    if (!fn) return NodeResult::kFailure;  // unregistered action

    // Build params array from node.params
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> params;
    for (const Dia::Core::StringCRC& p : node.params)
        params.Add(p);

    return fn(actionContext, params);
}

static NodeResult EvaluateNode(
    const BehaviourTreeAsset*    asset,
    Dia::Blackboard::Blackboard* blackboard,
    const ActionRegistry*        actionRegistry,
    void*                        actionContext,
    Dia::Core::StringCRC         nodeId,
    float                        /*deltaTime*/)
{
    const BehaviourTreeAsset::NodeDescriptor* node = asset->GetNode(nodeId);
    if (!node) return NodeResult::kFailure;

    static const Dia::Core::StringCRC kCondition{"condition"};
    static const Dia::Core::StringCRC kAction{"action"};

    if (node->type == kCondition) return EvaluateCondition(blackboard, *node);
    if (node->type == kAction)    return EvaluateAction(actionRegistry, actionContext, *node);

    // Composite/decorator nodes — implemented in Tasks 6 and 7
    return NodeResult::kRunning;
}

// ============================================================================
// BehaviourTreeComponent
// ============================================================================

BehaviourTreeComponent::BehaviourTreeComponent()
    : mImpl(new Impl())
{
}

BehaviourTreeComponent::~BehaviourTreeComponent()
{
    delete mImpl;
    mImpl = nullptr;
}

void BehaviourTreeComponent::SetAsset(const BehaviourTreeAsset* asset)
{
    mImpl->asset = asset;
}

void BehaviourTreeComponent::SetBlackboard(Dia::Blackboard::Blackboard* blackboard)
{
    mImpl->blackboard = blackboard;
}

void BehaviourTreeComponent::SetActionRegistry(const ActionRegistry* registry)
{
    mImpl->actionRegistry = registry;
}

void BehaviourTreeComponent::SetActionContext(void* context)
{
    mImpl->actionContext = context;
}

void BehaviourTreeComponent::SetDecoratorRegistry(const DecoratorRegistry* registry)
{
    mImpl->decoratorRegistry = registry;
}

void BehaviourTreeComponent::AddEventListener(IBehaviourTreeEventListener* listener)
{
    // No-op until Task 8 implements IBehaviourTreeEventListener.
    mImpl->listeners.push_back(listener);
}

void BehaviourTreeComponent::RemoveEventListener(IBehaviourTreeEventListener* listener)
{
    // No-op until Task 8 implements IBehaviourTreeEventListener.
    auto& v = mImpl->listeners;
    for (auto it = v.begin(); it != v.end(); ++it)
    {
        if (*it == listener)
        {
            v.erase(it);
            break;
        }
    }
}

NodeResult BehaviourTreeComponent::Tick(float deltaTime)
{
    if (!mImpl->asset)
        return NodeResult::kFailure;

    NodeResult result = EvaluateNode(
        mImpl->asset,
        mImpl->blackboard,
        mImpl->actionRegistry,
        mImpl->actionContext,
        mImpl->asset->GetRootNodeId(),
        deltaTime);

    mImpl->lastResult = result;
    mImpl->isComplete = (result != NodeResult::kRunning);
    return result;
}

void BehaviourTreeComponent::Reset()
{
    mImpl->executionStack.clear();
    mImpl->nodeStates.clear();
    mImpl->isComplete  = false;
    mImpl->lastResult  = NodeResult::kFailure;
}

bool BehaviourTreeComponent::IsComplete() const
{
    return mImpl->isComplete;
}

NodeResult BehaviourTreeComponent::LastResult() const
{
    return mImpl->lastResult;
}

bool BehaviourTreeComponent::HasAsset() const
{
    return mImpl->asset != nullptr;
}

    } // namespace BehaviourTree
} // namespace Dia
