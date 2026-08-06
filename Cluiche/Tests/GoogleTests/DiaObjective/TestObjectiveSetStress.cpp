#include <gtest/gtest.h>
#include <DiaObjective/ObjectiveSet.h>
#include <DiaObjective/Testing/ObjectiveTestHelpers.h>
#include <DiaCondition/Testing/ConditionTestHelpers.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <string>
#include <sstream>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
    Dia::Objective::ObjectiveSet LoadObjectiveSet(const std::string& json,
        Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors)
    {
        Json::Value root;
        Json::Reader reader;
        reader.parse(json, root);
        return Dia::Objective::ObjectiveSet::LoadFromJson(root, outErrors);
    }
}

// ---------------------------------------------------------------------------
// DiaObjective_ObjectiveSet_Stress
// ---------------------------------------------------------------------------

TEST(DiaObjective_ObjectiveSet_Stress, Stress_ManyObjectives_EvaluateAllComplete)
{
    // Build JSON with 20 objectives, each completing at score >= 10.
    std::ostringstream oss;
    oss << R"({ "objectives": [)";
    for (int i = 0; i < 20; ++i)
    {
        if (i > 0) oss << ",";
        oss << R"({)";
        oss << R"("id": "StressObj)" << i << R"(",)";
        oss << R"("classification": "primary",)";
        oss << R"("completion": { "op": ">=", "slot": "hero", "field": "score", "value": 10.0 })";
        oss << R"(})";
    }
    oss << R"(] })";

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(oss.str(), errors);

    ASSERT_EQ(set.GetCount(), 20);
    ASSERT_EQ(errors.Size(), 0u);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 10.0f);

    int transitions = set.Evaluate(ctx);

    EXPECT_EQ(transitions, 20);

    for (int i = 0; i < 20; ++i)
    {
        std::string id = "StressObj" + std::to_string(i);
        EXPECT_EQ(set.GetState(Dia::Core::StringCRC(id.c_str())),
                  Dia::Objective::ObjectiveState::kComplete)
            << "Objective " << id << " should be Complete";
    }

    EXPECT_TRUE(set.AllPrimaryComplete());
}

TEST(DiaObjective_ObjectiveSet_Stress, Stress_PrereqChain_LinearSequence)
{
    // Chain: A->B->C->D->E
    // Each objective completes at score >= N (1..5).
    // We advance score one step at a time, verifying state transitions.
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "ChainA",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 1.0 }
            },
            {
                "id": "ChainB",
                "prerequisites": ["ChainA"],
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 2.0 }
            },
            {
                "id": "ChainC",
                "prerequisites": ["ChainB"],
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 3.0 }
            },
            {
                "id": "ChainD",
                "prerequisites": ["ChainC"],
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 4.0 }
            },
            {
                "id": "ChainE",
                "prerequisites": ["ChainD"],
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 5.0 }
            }
        ]
    })", errors);

    ASSERT_EQ(set.GetCount(), 5);
    ASSERT_EQ(errors.Size(), 0u);

    // Initial: only ChainA is Active; B-E are Inactive
    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("ChainA")), Dia::Objective::ObjectiveState::kActive);
    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("ChainB")), Dia::Objective::ObjectiveState::kInactive);
    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("ChainE")), Dia::Objective::ObjectiveState::kInactive);

    // Tick 1: score=1 — completes ChainA, activates ChainB
    Dia::Condition::Testing::MockConditionContext ctx1;
    ctx1.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 1.0f);
    set.Evaluate(ctx1);
    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("ChainA")), Dia::Objective::ObjectiveState::kComplete);

    // Tick 2: score=1 still — ChainB now Active but score < 2, stays Active
    set.Evaluate(ctx1);
    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("ChainB")), Dia::Objective::ObjectiveState::kActive);

    // Tick 3: score=2 — completes ChainB, activates ChainC
    Dia::Condition::Testing::MockConditionContext ctx2;
    ctx2.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 2.0f);
    set.Evaluate(ctx2);
    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("ChainB")), Dia::Objective::ObjectiveState::kComplete);

    // Tick 4: score=3 — activates+completes ChainC (and ChainD activated next pass)
    Dia::Condition::Testing::MockConditionContext ctx3;
    ctx3.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 3.0f);
    set.Evaluate(ctx3);
    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("ChainC")), Dia::Objective::ObjectiveState::kComplete);

    // Tick 5: score=4 — activates+completes ChainD
    Dia::Condition::Testing::MockConditionContext ctx4;
    ctx4.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 4.0f);
    set.Evaluate(ctx4);
    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("ChainD")), Dia::Objective::ObjectiveState::kComplete);

    // Tick 6: score=5 — activates+completes ChainE
    Dia::Condition::Testing::MockConditionContext ctx5;
    ctx5.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 5.0f);
    set.Evaluate(ctx5);
    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("ChainE")), Dia::Objective::ObjectiveState::kComplete);

    EXPECT_TRUE(set.AllPrimaryComplete());
}

TEST(DiaObjective_ObjectiveSet_Stress, Stress_ManyObservers_AllNotified)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "NotifyObj",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 1.0 }
            }
        ]
    })", errors);

    constexpr int kObserverCount = 8;
    Dia::Objective::Testing::CapturingObserver observers[kObserverCount];

    for (int i = 0; i < kObserverCount; ++i)
        set.AddObserver(&observers[i]);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 1.0f);
    set.Evaluate(ctx);

    for (int i = 0; i < kObserverCount; ++i)
    {
        const auto& events = observers[i].GetEvents();
        EXPECT_EQ(events.Size(), 1u) << "Observer " << i << " should have received 1 event";
        if (events.Size() > 0)
        {
            EXPECT_EQ(events[0].type,
                      Dia::Objective::Testing::CapturingObserver::Event::Type::Completed);
            EXPECT_EQ(events[0].id.Value(), Dia::Core::StringCRC("NotifyObj").Value());
        }
    }
}

TEST(DiaObjective_ObjectiveSet_Stress, Stress_RepeatedEvaluate_100Ticks_NoTransitionsAfterLatch)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "LatchStress",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 10.0 }
            }
        ]
    })", errors);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 10.0f);

    // First evaluate completes the objective
    int firstTransitions = set.Evaluate(ctx);
    EXPECT_EQ(firstTransitions, 1);
    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("LatchStress")),
              Dia::Objective::ObjectiveState::kComplete);

    // 100 subsequent evaluates must all return 0 transitions
    for (int tick = 0; tick < 100; ++tick)
    {
        int transitions = set.Evaluate(ctx);
        EXPECT_EQ(transitions, 0) << "Unexpected transition at tick " << tick;
    }

    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("LatchStress")),
              Dia::Objective::ObjectiveState::kComplete);
}

TEST(DiaObjective_ObjectiveSet_Stress, Stress_AddRemoveObserver_100Cycles)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "CycleObj",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 1.0 }
            }
        ]
    })", errors);

    Dia::Objective::Testing::CapturingObserver obs;

    for (int i = 0; i < 100; ++i)
    {
        set.AddObserver(&obs);
        set.RemoveObserver(&obs);
    }

    // Observer was always removed before evaluation — should receive no events
    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 1.0f);
    set.Evaluate(ctx);

    EXPECT_EQ(obs.GetEvents().Size(), 0u);
    // No crash and objective transitions correctly
    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("CycleObj")),
              Dia::Objective::ObjectiveState::kComplete);
}
