#include <gtest/gtest.h>
#include <DiaBehaviourTree/BehaviourTreeSystem.h>
#include <DiaBehaviourTree/BehaviourTreeComponent.h>
#include <DiaBehaviourTree/BehaviourTreeAsset.h>
#include <DiaBehaviourTree/ActionRegistry.h>
#include <DiaBehaviourTree/IBehaviourTreeEventListener.h>
#include <DiaBehaviourTree/NodeResult.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>

using namespace Dia::BehaviourTree;
using Dia::Core::StringCRC;
using Dia::Core::Containers::DynamicArrayC;

// ============================================================================
// Helpers
// ============================================================================

namespace
{

static NodeResult SysSuccessAction(void*, const DynamicArrayC<StringCRC, 8>&)
{
    return NodeResult::kSuccess;
}

BehaviourTreeAsset MakeSingleActionAsset()
{
    Json::Value root;
    Json::Reader().parse(R"({
        "root": "a",
        "nodes": {
            "a": { "type": "action", "action_id": "SysSuccessAction", "params": [] }
        }
    })", root);
    DynamicArrayC<const char*, 32> errors;
    return BehaviourTreeAsset::LoadFromJson(root, errors);
}

// Spy listener that counts OnTreeCompleted events
struct TickSpy : public IBehaviourTreeEventListener
{
    int tickCount = 0;
    void OnNodeEntered(StringCRC) override {}
    void OnNodeCompleted(StringCRC, NodeResult) override {}
    void OnTreeCompleted(NodeResult) override { ++tickCount; }
};

struct ComponentFixture
{
    BehaviourTreeAsset asset;
    ActionRegistry     registry;
    BehaviourTreeComponent comp;
    TickSpy            spy;

    ComponentFixture()
        : asset(MakeSingleActionAsset())
    {
        registry.Register(StringCRC{"SysSuccessAction"}, SysSuccessAction);
        comp.SetAsset(&asset);
        comp.SetActionRegistry(&registry);
        comp.AddEventListener(&spy);
    }
};

} // namespace

// ============================================================================
// DiaBehaviourTree_System
// ============================================================================

TEST(DiaBehaviourTree_System, Register_IncreasesCount)
{
    ComponentFixture f;
    BehaviourTreeSystem sys;
    EXPECT_EQ(sys.GetRegisteredCount(), 0);
    sys.Register(&f.comp);
    EXPECT_EQ(sys.GetRegisteredCount(), 1);
}

TEST(DiaBehaviourTree_System, Unregister_DecreasesCount)
{
    ComponentFixture f;
    BehaviourTreeSystem sys;
    sys.Register(&f.comp);
    sys.Unregister(&f.comp);
    EXPECT_EQ(sys.GetRegisteredCount(), 0);
}

TEST(DiaBehaviourTree_System, Update_BudgetAmple_TicksAllComponents)
{
    ComponentFixture f1, f2, f3;
    BehaviourTreeSystem sys;
    sys.Register(&f1.comp);
    sys.Register(&f2.comp);
    sys.Register(&f3.comp);

    sys.Update(9999.0f, 0.016f);

    EXPECT_EQ(f1.spy.tickCount, 1);
    EXPECT_EQ(f2.spy.tickCount, 1);
    EXPECT_EQ(f3.spy.tickCount, 1);
}

TEST(DiaBehaviourTree_System, Update_BudgetZero_TicksNoComponents)
{
    ComponentFixture f1, f2;
    BehaviourTreeSystem sys;
    sys.Register(&f1.comp);
    sys.Register(&f2.comp);

    sys.Update(0.0f, 0.016f);

    EXPECT_EQ(f1.spy.tickCount, 0);
    EXPECT_EQ(f2.spy.tickCount, 0);
}

TEST(DiaBehaviourTree_System, UnregisteredComponent_NotTicked)
{
    ComponentFixture f;
    BehaviourTreeSystem sys;
    sys.Register(&f.comp);
    sys.Unregister(&f.comp);

    sys.Update(9999.0f, 0.016f);

    EXPECT_EQ(f.spy.tickCount, 0);
}

TEST(DiaBehaviourTree_System, ComponentWithNoAsset_Skipped)
{
    // Two components: one without asset (skipped), one with asset (ticked)
    BehaviourTreeComponent noAssetComp;
    TickSpy noAssetSpy;
    noAssetComp.AddEventListener(&noAssetSpy);

    ComponentFixture withAsset;
    BehaviourTreeSystem sys;
    sys.Register(&noAssetComp);
    sys.Register(&withAsset.comp);

    sys.Update(9999.0f, 0.016f);

    EXPECT_EQ(noAssetSpy.tickCount, 0);   // no asset — skipped
    EXPECT_EQ(withAsset.spy.tickCount, 1); // has asset — ticked
}
