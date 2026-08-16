#include <DiaBehaviourTree/BehaviourTreeComponent.h>
#include <DiaBehaviourTree/IBehaviourTreeEventListener.h>
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
    int resumeChildIndex = 0;
};

// Per-node persistent state.
struct NodeState
{
    int      counter           = 0;
    float    accumulator       = 0.0f;
    int      resumeChildIndex  = 0;      // for Sequence/Selector cursor
    uint32_t parallelDone      = 0;      // bit i = child i has completed
    uint32_t parallelFailed    = 0;      // bit i = child i returned kFailure
};

// ============================================================================
// EvalContext — bundles all evaluation dependencies for recursive calls
// ============================================================================

struct EvalContext
{
    const BehaviourTreeAsset*                    asset;
    Dia::Blackboard::Blackboard*                 blackboard;
    const ActionRegistry*                        actionRegistry;
    void*                                        actionContext;
    std::unordered_map<unsigned int, NodeState>* nodeStates;  // owned by Impl
    const DecoratorRegistry*                     decoratorRegistry;
    std::vector<IBehaviourTreeEventListener*>*   listeners;   // snapshot; nullable
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
// Utility
// ============================================================================

static int CountBits(uint32_t v)
{
    int c = 0;
    while (v) { c += static_cast<int>(v & 1u); v >>= 1; }
    return c;
}

// ============================================================================
// Forward declaration — needed because Sequence/Selector/Parallel recurse
// into EvaluateNode, which is defined after them.
// ============================================================================

static NodeResult EvaluateNode(const EvalContext& ctx,
                               Dia::Core::StringCRC nodeId,
                               float deltaTime);

// ============================================================================
// Leaf node evaluators
// ============================================================================

static NodeResult EvaluateCondition(
    const EvalContext&                        ctx,
    const BehaviourTreeAsset::NodeDescriptor& node)
{
    if (!ctx.blackboard) return NodeResult::kFailure;
    const bool* slot = ctx.blackboard->TryGet<bool>(node.blackboardKey);
    if (!slot) return NodeResult::kFailure;
    return *slot ? NodeResult::kSuccess : NodeResult::kFailure;
}

static NodeResult EvaluateAction(
    const EvalContext&                        ctx,
    const BehaviourTreeAsset::NodeDescriptor& node)
{
    if (!ctx.actionRegistry) return NodeResult::kFailure;
    ActionFn fn = ctx.actionRegistry->Find(node.actionId);
    if (!fn) return NodeResult::kFailure;

    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> params;
    for (const Dia::Core::StringCRC& p : node.params)
        params.Add(p);

    return fn(ctx.actionContext, params);
}

// ============================================================================
// Control-flow evaluators
// ============================================================================

static NodeResult EvaluateSequence(
    const EvalContext&                        ctx,
    const BehaviourTreeAsset::NodeDescriptor& node,
    float                                     deltaTime)
{
    NodeState& state = (*ctx.nodeStates)[node.id.Value()];
    const int startIdx = state.resumeChildIndex;
    const int childCount = static_cast<int>(node.children.size());

    for (int i = startIdx; i < childCount; ++i)
    {
        NodeResult result = EvaluateNode(ctx, node.children[i], deltaTime);
        if (result == NodeResult::kRunning)
        {
            state.resumeChildIndex = i;  // resume from this child next tick
            return NodeResult::kRunning;
        }
        if (result == NodeResult::kFailure)
        {
            state.resumeChildIndex = 0;  // reset on failure
            return NodeResult::kFailure;
        }
        // kSuccess — advance to next child
    }

    state.resumeChildIndex = 0;
    return NodeResult::kSuccess;
}

static NodeResult EvaluateSelector(
    const EvalContext&                        ctx,
    const BehaviourTreeAsset::NodeDescriptor& node,
    float                                     deltaTime)
{
    NodeState& state = (*ctx.nodeStates)[node.id.Value()];
    const int startIdx = state.resumeChildIndex;
    const int childCount = static_cast<int>(node.children.size());

    for (int i = startIdx; i < childCount; ++i)
    {
        NodeResult result = EvaluateNode(ctx, node.children[i], deltaTime);
        if (result == NodeResult::kRunning)
        {
            state.resumeChildIndex = i;
            return NodeResult::kRunning;
        }
        if (result == NodeResult::kSuccess)
        {
            state.resumeChildIndex = 0;
            return NodeResult::kSuccess;
        }
        // kFailure — try next child
    }

    state.resumeChildIndex = 0;
    return NodeResult::kFailure;
}

static NodeResult EvaluateParallel(
    const EvalContext&                        ctx,
    const BehaviourTreeAsset::NodeDescriptor& node,
    float                                     deltaTime)
{
    static const Dia::Core::StringCRC kRequireAll {"require_all"};
    static const Dia::Core::StringCRC kRequireOne {"require_one"};
    // kRequireNone is the fallback

    NodeState& state = (*ctx.nodeStates)[node.id.Value()];
    const int totalChildren = static_cast<int>(node.children.size());

    for (int i = 0; i < totalChildren; ++i)
    {
        if ((state.parallelDone >> static_cast<unsigned>(i)) & 1u)
            continue;  // already completed this child

        NodeResult result = EvaluateNode(ctx, node.children[i], deltaTime);
        if (result != NodeResult::kRunning)
        {
            state.parallelDone |= (1u << static_cast<unsigned>(i));
            if (result == NodeResult::kFailure)
                state.parallelFailed |= (1u << static_cast<unsigned>(i));
        }
    }

    const int doneCount  = CountBits(state.parallelDone);
    const int failCount  = CountBits(state.parallelFailed);
    const int succCount  = doneCount - failCount;

    if (node.policy == kRequireAll)
    {
        if (failCount > 0)
        {
            state.parallelDone   = 0;
            state.parallelFailed = 0;
            return NodeResult::kFailure;
        }
        if (doneCount == totalChildren)
        {
            state.parallelDone   = 0;
            state.parallelFailed = 0;
            return NodeResult::kSuccess;
        }
        return NodeResult::kRunning;
    }

    if (node.policy == kRequireOne)
    {
        if (succCount > 0)
        {
            state.parallelDone   = 0;
            state.parallelFailed = 0;
            return NodeResult::kSuccess;
        }
        if (doneCount == totalChildren)
        {
            state.parallelDone   = 0;
            state.parallelFailed = 0;
            return NodeResult::kFailure;
        }
        return NodeResult::kRunning;
    }

    // kRequireNone — succeed when all children have completed (any result)
    if (doneCount == totalChildren)
    {
        state.parallelDone   = 0;
        state.parallelFailed = 0;
        return NodeResult::kSuccess;
    }
    return NodeResult::kRunning;
}

// ============================================================================
// EvaluateDecorator — handles all decorator node types
// ============================================================================

static NodeResult EvaluateDecorator(
    const EvalContext&                        ctx,
    const BehaviourTreeAsset::NodeDescriptor& node,
    float                                     deltaTime)
{
    static const Dia::Core::StringCRC kInverter {"inverter"};
    static const Dia::Core::StringCRC kRepeater {"repeater"};
    static const Dia::Core::StringCRC kCooldown {"cooldown"};
    static const Dia::Core::StringCRC kGuard    {"guard"};

    // ---- Inverter: flip kSuccess↔kFailure, pass kRunning through ----
    if (node.decoratorType == kInverter)
    {
        NodeResult childResult = EvaluateNode(ctx, node.childId, deltaTime);
        if (childResult == NodeResult::kSuccess) return NodeResult::kFailure;
        if (childResult == NodeResult::kFailure) return NodeResult::kSuccess;
        return NodeResult::kRunning;
    }

    // ---- Repeater ----
    if (node.decoratorType == kRepeater)
    {
        NodeResult childResult = EvaluateNode(ctx, node.childId, deltaTime);
        if (node.breakOnFailure && childResult == NodeResult::kFailure)
            return NodeResult::kFailure;
        if (childResult == NodeResult::kRunning)
            return NodeResult::kRunning;
        if (childResult == NodeResult::kSuccess)
        {
            // Re-fetch after EvaluateNode (potential map resize)
            int& counter = (*ctx.nodeStates)[node.id.Value()].counter;
            ++counter;
            if (node.repeatCount > 0 && counter >= node.repeatCount)
            {
                counter = 0;
                return NodeResult::kSuccess;
            }
        }
        // Reset child subtree state so it runs fresh on the next tick
        ctx.nodeStates->erase(node.childId.Value());
        return NodeResult::kRunning;
    }

    // ---- Cooldown ----
    if (node.decoratorType == kCooldown)
    {
        // Check and advance accumulator without holding a ref across EvaluateNode
        const float currentAccum = (*ctx.nodeStates)[node.id.Value()].accumulator;
        if (currentAccum < node.cooldownSeconds)
        {
            (*ctx.nodeStates)[node.id.Value()].accumulator += deltaTime;
            return NodeResult::kFailure;
        }
        NodeResult childResult = EvaluateNode(ctx, node.childId, deltaTime);
        if (childResult == NodeResult::kSuccess)
            (*ctx.nodeStates)[node.id.Value()].accumulator = 0.0f;
        return childResult;
    }

    // ---- Guard: read blackboard bool; return kFailure if absent or false ----
    if (node.decoratorType == kGuard)
    {
        if (!ctx.blackboard)
            return NodeResult::kFailure;
        const bool* slot = ctx.blackboard->TryGet<bool>(node.guardKey);
        if (!slot || !*slot)
            return NodeResult::kFailure;
        return EvaluateNode(ctx, node.childId, deltaTime);
    }

    // ---- Custom decorator via DecoratorRegistry ----
    if (ctx.decoratorRegistry)
    {
        const IDecoratorNode* decorator = ctx.decoratorRegistry->Find(node.decoratorType);
        if (decorator)
        {
            {
                NodeState& st = (*ctx.nodeStates)[node.id.Value()];
                DecoratorContext dctx{deltaTime, st.counter, st.accumulator};
                if (!decorator->ShouldTickChild(dctx))
                    return NodeResult::kFailure;
            }
            NodeResult childResult = EvaluateNode(ctx, node.childId, deltaTime);
            {
                // Fresh lookup after potential map resize in EvaluateNode
                NodeState& st = (*ctx.nodeStates)[node.id.Value()];
                DecoratorContext dctx{deltaTime, st.counter, st.accumulator};
                return decorator->Evaluate(childResult, dctx);
            }
        }
    }

    return NodeResult::kFailure;
}

// ============================================================================
// EvaluateNode — dispatches to the appropriate evaluator
// ============================================================================

static NodeResult EvaluateNode(
    const EvalContext&   ctx,
    Dia::Core::StringCRC nodeId,
    float                deltaTime)
{
    const BehaviourTreeAsset::NodeDescriptor* node = ctx.asset->GetNode(nodeId);
    if (!node) return NodeResult::kFailure;

    if (ctx.listeners)
        for (IBehaviourTreeEventListener* l : *ctx.listeners)
            l->OnNodeEntered(nodeId);

    static const Dia::Core::StringCRC kCondition{"condition"};
    static const Dia::Core::StringCRC kAction   {"action"};
    static const Dia::Core::StringCRC kSequence {"sequence"};
    static const Dia::Core::StringCRC kSelector {"selector"};
    static const Dia::Core::StringCRC kParallel {"parallel"};
    static const Dia::Core::StringCRC kDecorator{"decorator"};

    NodeResult result = NodeResult::kFailure;
    if      (node->type == kCondition) result = EvaluateCondition(ctx, *node);
    else if (node->type == kAction)    result = EvaluateAction(ctx, *node);
    else if (node->type == kSequence)  result = EvaluateSequence(ctx, *node, deltaTime);
    else if (node->type == kSelector)  result = EvaluateSelector(ctx, *node, deltaTime);
    else if (node->type == kParallel)  result = EvaluateParallel(ctx, *node, deltaTime);
    else if (node->type == kDecorator) result = EvaluateDecorator(ctx, *node, deltaTime);

    if (result != NodeResult::kRunning && ctx.listeners)
        for (IBehaviourTreeEventListener* l : *ctx.listeners)
            l->OnNodeCompleted(nodeId, result);

    return result;
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
    mImpl->listeners.push_back(listener);
}

void BehaviourTreeComponent::RemoveEventListener(IBehaviourTreeEventListener* listener)
{
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

    // Snapshot so RemoveEventListener mid-tick is safe — we iterate the copy, not the live list
    std::vector<IBehaviourTreeEventListener*> listenersSnapshot = mImpl->listeners;

    EvalContext ctx;
    ctx.asset              = mImpl->asset;
    ctx.blackboard         = mImpl->blackboard;
    ctx.actionRegistry     = mImpl->actionRegistry;
    ctx.actionContext      = mImpl->actionContext;
    ctx.nodeStates         = &mImpl->nodeStates;
    ctx.decoratorRegistry  = mImpl->decoratorRegistry;
    ctx.listeners          = listenersSnapshot.empty() ? nullptr : &listenersSnapshot;

    NodeResult result = EvaluateNode(ctx, mImpl->asset->GetRootNodeId(), deltaTime);

    mImpl->lastResult = result;
    mImpl->isComplete = (result != NodeResult::kRunning);

    if (result != NodeResult::kRunning && !listenersSnapshot.empty())
        for (IBehaviourTreeEventListener* l : listenersSnapshot)
            l->OnTreeCompleted(result);

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
