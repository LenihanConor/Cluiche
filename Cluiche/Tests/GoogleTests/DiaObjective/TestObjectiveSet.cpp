#include <gtest/gtest.h>

#include <DiaObjective/ObjectiveSet.h>
#include <DiaObjective/ObjectiveDef.h>
#include <DiaObjective/IObjectiveObserver.h>
#include <DiaObjective/Testing/ObjectiveTestHelpers.h>
#include <DiaCondition/Testing/ConditionTestHelpers.h>
#include <DiaCondition/ConditionRegistry.h>
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

    Dia::Objective::ObjectiveSet LoadObjectiveSet(const char* json,
        Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors)
    {
        Json::Value root = ParseJson(json);
        return Dia::Objective::ObjectiveSet::LoadFromJson(root, outErrors);
    }
}

// ---------------------------------------------------------------------------
// DiaObjective_ObjectiveSet — LoadFromJson
// ---------------------------------------------------------------------------

TEST(DiaObjective_ObjectiveSet, LoadFromJson_ValidJson_GetCountCorrect)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "ObjA",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 100.0 }
            },
            {
                "id": "ObjB",
                "completion": { "op": ">=", "slot": "hero", "field": "kills", "value": 5.0 }
            }
        ]
    })", errors);

    EXPECT_EQ(set.GetCount(), 2);
    EXPECT_EQ(errors.Size(), 0u);
}

TEST(DiaObjective_ObjectiveSet, LoadFromJson_MissingObjectivesKey_ReturnsEmptySet)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({})", errors);

    EXPECT_EQ(set.GetCount(), 0);
    EXPECT_GT(errors.Size(), 0u);
}

TEST(DiaObjective_ObjectiveSet, LoadFromJson_MissingCompletion_ObjectiveSkipped)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "NoCompletion"
            }
        ]
    })", errors);

    EXPECT_EQ(set.GetCount(), 0);
    EXPECT_GT(errors.Size(), 0u);
}

TEST(DiaObjective_ObjectiveSet, LoadFromJson_Classification_Primary_Default)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "PrimaryObj",
                "classification": "primary",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 100.0 }
            }
        ]
    })", errors);

    ASSERT_EQ(set.GetCount(), 1);
    const Dia::Objective::ObjectiveDef* def = set.GetAt(0);
    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->classification, Dia::Objective::ObjectiveClassification::kPrimary);
}

TEST(DiaObjective_ObjectiveSet, LoadFromJson_Classification_Secondary)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "SecondaryObj",
                "classification": "secondary",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 50.0 }
            }
        ]
    })", errors);

    ASSERT_EQ(set.GetCount(), 1);
    const Dia::Objective::ObjectiveDef* def = set.GetAt(0);
    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->classification, Dia::Objective::ObjectiveClassification::kSecondary);
}

TEST(DiaObjective_ObjectiveSet, LoadFromJson_Classification_Optional)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "OptionalObj",
                "classification": "optional",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 10.0 }
            }
        ]
    })", errors);

    ASSERT_EQ(set.GetCount(), 1);
    const Dia::Objective::ObjectiveDef* def = set.GetAt(0);
    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->classification, Dia::Objective::ObjectiveClassification::kOptional);
}

TEST(DiaObjective_ObjectiveSet, LoadFromJson_Prerequisites_ParsedCorrectly)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "Stage1",
                "completion": { "op": ">=", "slot": "hero", "field": "stage", "value": 1.0 }
            },
            {
                "id": "Stage2",
                "prerequisites": ["Stage1"],
                "completion": { "op": ">=", "slot": "hero", "field": "stage", "value": 2.0 }
            }
        ]
    })", errors);

    ASSERT_EQ(set.GetCount(), 2);
    // Stage2 is at index 1
    const Dia::Objective::ObjectiveDef* def = set.GetAt(1);
    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->prerequisites.Size(), 1u);
    EXPECT_EQ(def->prerequisites[0].Value(), Dia::Core::StringCRC("Stage1").Value());
}

TEST(DiaObjective_ObjectiveSet, LoadFromJson_RewardParsed)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "RewardObj",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 100.0 },
                "reward": [
                    { "type": "gold", "amount": 50.0 }
                ]
            }
        ]
    })", errors);

    ASSERT_EQ(set.GetCount(), 1);
    const Dia::Objective::ObjectiveDef* def = set.GetAt(0);
    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->reward.Size(), 1u);
    EXPECT_EQ(def->reward[0].type.Value(), Dia::Core::StringCRC("gold").Value());
    EXPECT_FLOAT_EQ(def->reward[0].amount, 50.0f);
}

TEST(DiaObjective_ObjectiveSet, LoadFromJson_InitialState_NoPrereqs_IsActive)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "NoPrereqObj",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 100.0 }
            }
        ]
    })", errors);

    ASSERT_EQ(set.GetCount(), 1);
    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("NoPrereqObj")),
              Dia::Objective::ObjectiveState::kActive);
}

TEST(DiaObjective_ObjectiveSet, LoadFromJson_InitialState_WithPrereqs_IsInactive)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "PrereqA",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 10.0 }
            },
            {
                "id": "DependsOnA",
                "prerequisites": ["PrereqA"],
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 50.0 }
            }
        ]
    })", errors);

    ASSERT_EQ(set.GetCount(), 2);
    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("DependsOnA")),
              Dia::Objective::ObjectiveState::kInactive);
}

// ---------------------------------------------------------------------------
// DiaObjective_ObjectiveSet — Evaluate
// ---------------------------------------------------------------------------

TEST(DiaObjective_ObjectiveSet, Evaluate_ActiveObjective_CompletionTrue_TransitionsToComplete)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "Obj1",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 100.0 }
            }
        ]
    })", errors);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 100.0f);

    int transitions = set.Evaluate(ctx);

    EXPECT_EQ(transitions, 1);
    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("Obj1")),
              Dia::Objective::ObjectiveState::kComplete);
}

TEST(DiaObjective_ObjectiveSet, Evaluate_ActiveObjective_CompletionFalse_StaysActive)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "Obj1",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 100.0 }
            }
        ]
    })", errors);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 50.0f);

    int transitions = set.Evaluate(ctx);

    EXPECT_EQ(transitions, 0);
    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("Obj1")),
              Dia::Objective::ObjectiveState::kActive);
}

TEST(DiaObjective_ObjectiveSet, Evaluate_CompletedObjective_NotReevaluated_LatchSemantics)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "LatchObj",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 100.0 }
            }
        ]
    })", errors);

    // First evaluate: complete it
    Dia::Condition::Testing::MockConditionContext ctxTrue;
    ctxTrue.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 100.0f);
    set.Evaluate(ctxTrue);

    ASSERT_EQ(set.GetState(Dia::Core::StringCRC("LatchObj")),
              Dia::Objective::ObjectiveState::kComplete);

    // Second evaluate: context no longer satisfies completion — state should still be kComplete
    Dia::Condition::Testing::MockConditionContext ctxFalse;
    ctxFalse.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 0.0f);
    set.Evaluate(ctxFalse);

    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("LatchObj")),
              Dia::Objective::ObjectiveState::kComplete);
}

TEST(DiaObjective_ObjectiveSet, Evaluate_FailureCondition_TransitionsToFailed)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "FailObj",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 100.0 },
                "failure":    { "op": "==", "slot": "hero", "field": "dead",  "value": true  }
            }
        ]
    })", errors);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 0.0f);
    ctx.SetBool (Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("dead"),  true);

    int transitions = set.Evaluate(ctx);

    EXPECT_EQ(transitions, 1);
    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("FailObj")),
              Dia::Objective::ObjectiveState::kFailed);
}

TEST(DiaObjective_ObjectiveSet, Evaluate_CompletionTakesPrecedenceOverFailure)
{
    // Both completion and failure conditions are true — completion fires first (kComplete, not kFailed)
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "BothObj",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 100.0 },
                "failure":    { "op": "==", "slot": "hero", "field": "dead",  "value": true  }
            }
        ]
    })", errors);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 100.0f);
    ctx.SetBool (Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("dead"),  true);

    set.Evaluate(ctx);

    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("BothObj")),
              Dia::Objective::ObjectiveState::kComplete);
}

TEST(DiaObjective_ObjectiveSet, Evaluate_PrereqChain_ActivatesWhenPrereqComplete)
{
    // ObjA has no prereqs; ObjB depends on ObjA.
    // After ObjA completes, the next Evaluate activates ObjB.
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "ObjA",
                "completion": { "op": ">=", "slot": "hero", "field": "kills", "value": 5.0 }
            },
            {
                "id": "ObjB",
                "prerequisites": ["ObjA"],
                "completion": { "op": ">=", "slot": "hero", "field": "kills", "value": 10.0 }
            }
        ]
    })", errors);

    ASSERT_EQ(set.GetCount(), 2);
    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("ObjB")),
              Dia::Objective::ObjectiveState::kInactive);

    // First evaluate: complete ObjA
    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("kills"), 5.0f);
    set.Evaluate(ctx);

    ASSERT_EQ(set.GetState(Dia::Core::StringCRC("ObjA")),
              Dia::Objective::ObjectiveState::kComplete);

    // Second evaluate: ObjB should now be activated (and still not complete at kills=5)
    set.Evaluate(ctx);

    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("ObjB")),
              Dia::Objective::ObjectiveState::kActive);
}

TEST(DiaObjective_ObjectiveSet, Evaluate_AllPrimaryComplete_TrueWhenAllPrimaryDone)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "Primary1",
                "classification": "primary",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 10.0 }
            },
            {
                "id": "Primary2",
                "classification": "primary",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 10.0 }
            }
        ]
    })", errors);

    EXPECT_FALSE(set.AllPrimaryComplete());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 10.0f);
    set.Evaluate(ctx);

    EXPECT_TRUE(set.AllPrimaryComplete());
}

TEST(DiaObjective_ObjectiveSet, Evaluate_AnyPrimaryFailed_TrueWhenOneFails)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "PrimaryFail",
                "classification": "primary",
                "completion": { "op": ">=", "slot": "hero", "field": "score",  "value": 100.0 },
                "failure":    { "op": "==", "slot": "hero", "field": "dead",   "value": true  }
            }
        ]
    })", errors);

    EXPECT_FALSE(set.AnyPrimaryFailed());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 0.0f);
    ctx.SetBool (Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("dead"),  true);
    set.Evaluate(ctx);

    EXPECT_TRUE(set.AnyPrimaryFailed());
}

// ---------------------------------------------------------------------------
// DiaObjective_ObjectiveSet — Observer
// ---------------------------------------------------------------------------

TEST(DiaObjective_ObjectiveSet, Observer_Completed_FiredOnComplete)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "CompObj",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 100.0 }
            }
        ]
    })", errors);

    Dia::Objective::Testing::CapturingObserver obs;
    set.AddObserver(&obs);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 100.0f);
    set.Evaluate(ctx);

    const auto& events = obs.GetEvents();
    ASSERT_EQ(events.Size(), 1u);
    EXPECT_EQ(events[0].type, Dia::Objective::Testing::CapturingObserver::Event::Type::Completed);
    EXPECT_EQ(events[0].id.Value(), Dia::Core::StringCRC("CompObj").Value());
}

TEST(DiaObjective_ObjectiveSet, Observer_Failed_FiredOnFail)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "FailObjObs",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 100.0 },
                "failure":    { "op": "==", "slot": "hero", "field": "dead",  "value": true  }
            }
        ]
    })", errors);

    Dia::Objective::Testing::CapturingObserver obs;
    set.AddObserver(&obs);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 0.0f);
    ctx.SetBool (Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("dead"),  true);
    set.Evaluate(ctx);

    const auto& events = obs.GetEvents();
    ASSERT_EQ(events.Size(), 1u);
    EXPECT_EQ(events[0].type, Dia::Objective::Testing::CapturingObserver::Event::Type::Failed);
    EXPECT_EQ(events[0].id.Value(), Dia::Core::StringCRC("FailObjObs").Value());
}

TEST(DiaObjective_ObjectiveSet, Observer_Activated_FiredOnPrereqCompletion)
{
    // ObjA (no prereqs) completes, which triggers ObjB (has prereq ObjA) to activate.
    // The Activated event should fire for ObjB on the same Evaluate call that also
    // completes ObjA (activation is processed before completion in the same pass).
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "ActivatorA",
                "completion": { "op": ">=", "slot": "hero", "field": "kills", "value": 5.0 }
            },
            {
                "id": "ActivatedB",
                "prerequisites": ["ActivatorA"],
                "completion": { "op": ">=", "slot": "hero", "field": "kills", "value": 10.0 }
            }
        ]
    })", errors);

    // Complete ObjA first
    Dia::Condition::Testing::MockConditionContext ctx5;
    ctx5.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("kills"), 5.0f);
    set.Evaluate(ctx5);

    ASSERT_EQ(set.GetState(Dia::Core::StringCRC("ActivatorA")),
              Dia::Objective::ObjectiveState::kComplete);

    // Now attach observer and do second evaluate — Activated for ActivatedB should fire
    Dia::Objective::Testing::CapturingObserver obs;
    set.AddObserver(&obs);

    set.Evaluate(ctx5); // ObjB activates; kills=5 doesn't complete ObjB

    const auto& events = obs.GetEvents();
    ASSERT_GE(events.Size(), 1u);
    EXPECT_EQ(events[0].type, Dia::Objective::Testing::CapturingObserver::Event::Type::Activated);
    EXPECT_EQ(events[0].id.Value(), Dia::Core::StringCRC("ActivatedB").Value());
}

TEST(DiaObjective_ObjectiveSet, Observer_RemovedObserver_NotCalled)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "RemoveObsObj",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 100.0 }
            }
        ]
    })", errors);

    Dia::Objective::Testing::CapturingObserver obs;
    set.AddObserver(&obs);
    set.RemoveObserver(&obs);

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 100.0f);
    set.Evaluate(ctx);

    EXPECT_EQ(obs.GetEvents().Size(), 0u);
}
