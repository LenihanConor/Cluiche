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

NodeResult BehaviourTreeComponent::Tick(float /*deltaTime*/)
{
    if (!mImpl->asset)
        return NodeResult::kFailure;

    // Stub: full traversal is implemented in Tasks 5-7.
    return NodeResult::kRunning;
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
