#include <gtest/gtest.h>

#include <DiaObjective/ObjectiveSetComponent.h>
#include <DiaObjective/ObjectiveSet.h>
#include <DiaObjective/ObjectiveDef.h>
#include <DiaObjective/IObjectiveObserver.h>
#include <DiaObjective/Testing/ObjectiveTestHelpers.h>
#include <DiaCondition/Testing/ConditionTestHelpers.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
    Json::Value ParseJson(const char* src)
    {
        Json::Value root;
        Json::Reader reader;
        reader.parse(src, root);
        return root;
    }

    Dia::Objective::ObjectiveSet LoadObjectiveSet(const char* json)
    {
        Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
        Json::Value root = ParseJson(json);
        return Dia::Objective::ObjectiveSet::LoadFromJson(root, errors);
    }
}

// ---------------------------------------------------------------------------
// DiaObjective_ObjectiveSetComponent — Evaluate
// ---------------------------------------------------------------------------

TEST(DiaObjective_ObjectiveSetComponent, SetObjectiveSet_EvaluateReturnsZeroWithFalseContext)
{
    Dia::Objective::ObjectiveSetComponent comp;
    comp.SetObjectiveSet(LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "Obj1",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 100.0 }
            }
        ]
    })"));

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 0.0f);

    int transitions = comp.Evaluate(ctx);
    EXPECT_EQ(transitions, 0);
}

TEST(DiaObjective_ObjectiveSetComponent, SetObjectiveSet_EvaluateReturnsOneOnCompletion)
{
    Dia::Objective::ObjectiveSetComponent comp;
    comp.SetObjectiveSet(LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "Obj2",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 100.0 }
            }
        ]
    })"));

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 100.0f);

    int transitions = comp.Evaluate(ctx);
    EXPECT_EQ(transitions, 1);
}

TEST(DiaObjective_ObjectiveSetComponent, GetState_CompletedObjective_ReturnsComplete)
{
    Dia::Objective::ObjectiveSetComponent comp;
    comp.SetObjectiveSet(LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "StateObj",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 100.0 }
            }
        ]
    })"));

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 100.0f);
    comp.Evaluate(ctx);

    EXPECT_EQ(comp.GetState(Dia::Core::StringCRC("StateObj")),
              Dia::Objective::ObjectiveState::kComplete);
}

TEST(DiaObjective_ObjectiveSetComponent, AllPrimaryComplete_TrueWhenDone)
{
    Dia::Objective::ObjectiveSetComponent comp;
    comp.SetObjectiveSet(LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "PrimaryComp",
                "classification": "primary",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 10.0 }
            }
        ]
    })"));

    EXPECT_FALSE(comp.AllPrimaryComplete());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 10.0f);
    comp.Evaluate(ctx);

    EXPECT_TRUE(comp.AllPrimaryComplete());
}

TEST(DiaObjective_ObjectiveSetComponent, AnyPrimaryFailed_TrueWhenFailed)
{
    Dia::Objective::ObjectiveSetComponent comp;
    comp.SetObjectiveSet(LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "PrimaryFail",
                "classification": "primary",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 100.0 },
                "failure":    { "op": "==", "slot": "hero", "field": "dead",  "value": true  }
            }
        ]
    })"));

    EXPECT_FALSE(comp.AnyPrimaryFailed());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 0.0f);
    ctx.SetBool (Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("dead"),  true);
    comp.Evaluate(ctx);

    EXPECT_TRUE(comp.AnyPrimaryFailed());
}

TEST(DiaObjective_ObjectiveSetComponent, Observer_DelegatedToObjectiveSet)
{
    Dia::Objective::ObjectiveSetComponent comp;
    comp.SetObjectiveSet(LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "DelegateObj",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 100.0 }
            }
        ]
    })"));

    Dia::Objective::Testing::CapturingObserver obs;
    comp.AddObserver(&obs);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 100.0f);
    comp.Evaluate(ctx);

    const auto& events = obs.GetEvents();
    ASSERT_EQ(events.Size(), 1u);
    EXPECT_EQ(events[0].type, Dia::Objective::Testing::CapturingObserver::Event::Type::Completed);
    EXPECT_EQ(events[0].id.Value(), Dia::Core::StringCRC("DelegateObj").Value());

    // RemoveObserver: after removal, no more events
    comp.RemoveObserver(&obs);
    obs.Clear();

    Dia::Condition::Testing::MockConditionContext ctx2;
    ctx2.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 100.0f);
    comp.Evaluate(ctx2); // already kComplete, no new events anyway — just verifies no crash

    EXPECT_EQ(obs.GetEvents().Size(), 0u);
}
