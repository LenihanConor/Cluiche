#pragma once

#include <DiaBehaviourTree/ActionRegistry.h>
#include <DiaBehaviourTree/BehaviourTreeComponent.h>
#include <DiaBehaviourTree/IBehaviourTreeEventListener.h>
#include <DiaBehaviourTree/NodeResult.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <gtest/gtest.h>
#include <vector>

namespace Dia
{
    namespace BehaviourTree
    {
        namespace Testing
        {

// ============================================================================
// SpyAction
//
// Registerable action that records calls and returns a configurable result.
//
// Usage:
//   SpyAction spy;
//   spy.RegisterIn(registry, StringCRC{"MyAction"});
//   comp.SetActionContext(spy.AsContext());   // must match the registered action
//   comp.Tick(0.0f);
//   EXPECT_EQ(spy.GetCallCount(), 1);
//
// Limitation: a component has one actionContext — only one SpyAction per
// component can route via ctx. Register multiple static actions for multi-action tests.
// ============================================================================

class SpyAction
{
public:
    SpyAction() = default;
    SpyAction(const SpyAction&) = delete;
    SpyAction& operator=(const SpyAction&) = delete;

    void SetResult(NodeResult result) { mResult = result; }
    int  GetCallCount() const         { return mCallCount; }

    const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>&
    LastParams() const { return mLastParams; }

    // Registers Fn in the registry under actionId.
    // The component must also call SetActionContext(spy.AsContext()).
    void RegisterIn(ActionRegistry& registry, Dia::Core::StringCRC actionId)
    {
        registry.Register(actionId, &SpyAction::Fn);
    }

    // Returns the void* that must be passed to comp.SetActionContext().
    void* AsContext() { return this; }

private:
    static NodeResult Fn(
        void* ctx,
        const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>& params)
    {
        SpyAction* self = static_cast<SpyAction*>(ctx);
        ++self->mCallCount;
        self->mLastParams = params;
        return self->mResult;
    }

    NodeResult mResult    = NodeResult::kSuccess;
    int        mCallCount = 0;
    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8> mLastParams;
};

// ============================================================================
// AssertNodeVisited
//
// Attaches a temporary listener to comp, ticks it, then asserts nodeId was entered.
// GTest EXPECT_ failures propagate to the calling test.
// ============================================================================

inline void AssertNodeVisited(
    BehaviourTreeComponent& comp,
    Dia::Core::StringCRC    nodeId,
    float                   deltaTime = 0.0f)
{
    class Listener : public IBehaviourTreeEventListener
    {
    public:
        std::vector<Dia::Core::StringCRC> entered;
        void OnNodeEntered(Dia::Core::StringCRC id) override { entered.push_back(id); }
        void OnNodeCompleted(Dia::Core::StringCRC, NodeResult) override {}
        void OnTreeCompleted(NodeResult) override {}
    };

    Listener listener;
    comp.AddEventListener(&listener);
    comp.Tick(deltaTime);
    comp.RemoveEventListener(&listener);

    bool found = false;
    for (const Dia::Core::StringCRC& id : listener.entered)
    {
        if (id == nodeId) { found = true; break; }
    }
    EXPECT_TRUE(found) << "AssertNodeVisited: node was not entered during Tick";
}

// ============================================================================
// AssertLastResult
//
// Asserts comp.LastResult() matches expected after the most recent Tick.
// ============================================================================

inline void AssertLastResult(
    const BehaviourTreeComponent& comp,
    NodeResult                    expected)
{
    EXPECT_EQ(comp.LastResult(), expected);
}

        } // namespace Testing
    } // namespace BehaviourTree
} // namespace Dia
