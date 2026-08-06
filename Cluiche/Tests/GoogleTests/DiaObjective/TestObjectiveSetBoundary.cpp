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
// DiaObjective_ObjectiveSet_Boundary — GetAt / GetState bounds
// ---------------------------------------------------------------------------

TEST(DiaObjective_ObjectiveSet_Boundary, GetAt_OutOfBounds_ReturnsNullptr)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "OnlyObj",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 1.0 }
            }
        ]
    })", errors);

    ASSERT_EQ(set.GetCount(), 1);
    EXPECT_EQ(set.GetAt(-1),  nullptr);
    EXPECT_EQ(set.GetAt(100), nullptr);
}

TEST(DiaObjective_ObjectiveSet_Boundary, GetAt_NegativeIndex_ReturnsNullptr)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "NegObj",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 1.0 }
            }
        ]
    })", errors);

    EXPECT_EQ(set.GetAt(-1), nullptr);
}

TEST(DiaObjective_ObjectiveSet_Boundary, GetState_UnknownId_ReturnsInactive)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "KnownObj",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 1.0 }
            }
        ]
    })", errors);

    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("DoesNotExist")),
              Dia::Objective::ObjectiveState::kInactive);
}

// ---------------------------------------------------------------------------
// DiaObjective_ObjectiveSet_Boundary — LoadFromJson edge inputs
// ---------------------------------------------------------------------------

TEST(DiaObjective_ObjectiveSet_Boundary, LoadFromJson_NonArrayObjectives_ReturnsEmptyWithError)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({ "objectives": 42 })", errors);

    EXPECT_EQ(set.GetCount(), 0);
    EXPECT_GT(errors.Size(), 0u);
}

TEST(DiaObjective_ObjectiveSet_Boundary, LoadFromJson_NonObjectEntry_SkipsEntry)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [ 123 ]
    })", errors);

    EXPECT_EQ(set.GetCount(), 0);
}

TEST(DiaObjective_ObjectiveSet_Boundary, LoadFromJson_EmptyObjectivesArray_ReturnsEmptyNoErrors)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({ "objectives": [] })", errors);

    EXPECT_EQ(set.GetCount(), 0);
    EXPECT_EQ(errors.Size(), 0u);
}

TEST(DiaObjective_ObjectiveSet_Boundary, LoadFromJson_UnknownClassification_DefaultsToPrimary)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "EliteObj",
                "classification": "elite",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 1.0 }
            }
        ]
    })", errors);

    ASSERT_EQ(set.GetCount(), 1);
    const Dia::Objective::ObjectiveDef* def = set.GetAt(0);
    ASSERT_NE(def, nullptr);
    EXPECT_EQ(def->classification, Dia::Objective::ObjectiveClassification::kPrimary);
}

// ---------------------------------------------------------------------------
// DiaObjective_ObjectiveSet_Boundary — Evaluate edge cases
// ---------------------------------------------------------------------------

TEST(DiaObjective_ObjectiveSet_Boundary, Evaluate_EmptySet_ReturnsZeroNoCrash)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({ "objectives": [] })", errors);

    Dia::Condition::Testing::MockConditionContext ctx;
    int transitions = set.Evaluate(ctx);

    EXPECT_EQ(transitions, 0);
}

TEST(DiaObjective_ObjectiveSet_Boundary, Evaluate_InactiveObjectiveNotEvaluated_StaysInactive)
{
    // ObjB has unmet prereq ObjA. Even though ObjB's completion would be true,
    // it should remain Inactive because ObjA has not completed.
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "BlockerA",
                "completion": { "op": ">=", "slot": "hero", "field": "kills", "value": 99.0 }
            },
            {
                "id": "LockedB",
                "prerequisites": ["BlockerA"],
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 1.0 }
            }
        ]
    })", errors);

    ASSERT_EQ(set.GetCount(), 2);
    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("LockedB")),
              Dia::Objective::ObjectiveState::kInactive);

    // Context satisfies LockedB's completion but NOT BlockerA's
    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("kills"), 0.0f);
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 100.0f);
    set.Evaluate(ctx);

    // LockedB must still be Inactive — prereq not met
    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("LockedB")),
              Dia::Objective::ObjectiveState::kInactive);
}

// ---------------------------------------------------------------------------
// DiaObjective_ObjectiveSet_Boundary — AllPrimaryComplete / AnyPrimaryFailed vacuous
// ---------------------------------------------------------------------------

TEST(DiaObjective_ObjectiveSet_Boundary, AllPrimaryComplete_EmptySet_ReturnsTrue)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({ "objectives": [] })", errors);

    EXPECT_TRUE(set.AllPrimaryComplete());
}

TEST(DiaObjective_ObjectiveSet_Boundary, AnyPrimaryFailed_EmptySet_ReturnsFalse)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({ "objectives": [] })", errors);

    EXPECT_FALSE(set.AnyPrimaryFailed());
}

TEST(DiaObjective_ObjectiveSet_Boundary, AllPrimaryComplete_OnlySecondary_ReturnsTrue)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "SecObj",
                "classification": "secondary",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 999.0 }
            },
            {
                "id": "OptObj",
                "classification": "optional",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 999.0 }
            }
        ]
    })", errors);

    // No primary objectives — vacuously true
    EXPECT_TRUE(set.AllPrimaryComplete());
}

// ---------------------------------------------------------------------------
// DiaObjective_ObjectiveSet_Boundary — Latch: Failed stays Failed
// ---------------------------------------------------------------------------

TEST(DiaObjective_ObjectiveSet_Boundary, Evaluate_FailedObjective_NotReevaluated_LatchSemantics)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "FailLatch",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 100.0 },
                "failure":    { "op": "==", "slot": "hero", "field": "dead",  "value": true  }
            }
        ]
    })", errors);

    // First evaluate: fail it
    Dia::Condition::Testing::MockConditionContext ctxFail;
    ctxFail.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 0.0f);
    ctxFail.SetBool (Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("dead"),  true);
    set.Evaluate(ctxFail);

    ASSERT_EQ(set.GetState(Dia::Core::StringCRC("FailLatch")),
              Dia::Objective::ObjectiveState::kFailed);

    // Second evaluate: failure condition now false — but state must still be kFailed
    Dia::Condition::Testing::MockConditionContext ctxRecover;
    ctxRecover.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 200.0f);
    ctxRecover.SetBool (Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("dead"),  false);
    int transitions = set.Evaluate(ctxRecover);

    EXPECT_EQ(transitions, 0);
    EXPECT_EQ(set.GetState(Dia::Core::StringCRC("FailLatch")),
              Dia::Objective::ObjectiveState::kFailed);
}

// ---------------------------------------------------------------------------
// DiaObjective_ObjectiveSet_Boundary — Move semantics
// ---------------------------------------------------------------------------

TEST(DiaObjective_ObjectiveSet_Boundary, MoveConstruct_TransfersState)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet src = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "MoveObj1",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 1.0 }
            },
            {
                "id": "MoveObj2",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 2.0 }
            }
        ]
    })", errors);

    ASSERT_EQ(src.GetCount(), 2);

    Dia::Objective::ObjectiveSet dst(std::move(src));

    EXPECT_EQ(dst.GetCount(), 2);
    EXPECT_NE(dst.GetAt(0), nullptr);
    EXPECT_NE(dst.GetAt(1), nullptr);
}

TEST(DiaObjective_ObjectiveSet_Boundary, MoveAssign_TransfersState)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet src = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "AssignObj1",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 1.0 }
            },
            {
                "id": "AssignObj2",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 2.0 }
            },
            {
                "id": "AssignObj3",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 3.0 }
            }
        ]
    })", errors);

    ASSERT_EQ(src.GetCount(), 3);

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors2;
    Dia::Objective::ObjectiveSet dst = LoadObjectiveSet(R"({ "objectives": [] })", errors2);

    dst = std::move(src);

    EXPECT_EQ(dst.GetCount(), 3);
    EXPECT_NE(dst.GetAt(0), nullptr);
    EXPECT_NE(dst.GetAt(2), nullptr);
}

// ---------------------------------------------------------------------------
// DiaObjective_ObjectiveSet_Boundary — Validate
// ---------------------------------------------------------------------------

TEST(DiaObjective_ObjectiveSet_Boundary, Validate_ValidRegistry_ReturnsTrue)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "ValidateObj",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 100.0 }
            }
        ]
    })", errors);

    ASSERT_EQ(set.GetCount(), 1);
    ASSERT_EQ(errors.Size(), 0u);

    float dummy = 0.0f;
    Dia::Condition::ConditionRegistry registry(&dummy);
    registry.RegisterFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"),
        [](void* d) { return *static_cast<float*>(d); });

    Dia::Core::Containers::DynamicArrayC<const char*, 32> validateErrors;
    bool result = set.Validate(registry, validateErrors);

    EXPECT_TRUE(result);
    EXPECT_EQ(validateErrors.Size(), 0u);
}

TEST(DiaObjective_ObjectiveSet_Boundary, Validate_MissingSlotInRegistry_ReturnsFalse)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Objective::ObjectiveSet set = LoadObjectiveSet(R"({
        "objectives": [
            {
                "id": "MissingSlotObj",
                "completion": { "op": ">=", "slot": "hero", "field": "score", "value": 100.0 }
            }
        ]
    })", errors);

    ASSERT_EQ(set.GetCount(), 1);

    // Empty registry — "hero"/"score" not registered
    float dummy = 0.0f;
    Dia::Condition::ConditionRegistry emptyRegistry(&dummy);

    Dia::Core::Containers::DynamicArrayC<const char*, 32> validateErrors;
    bool result = set.Validate(emptyRegistry, validateErrors);

    EXPECT_FALSE(result);
    EXPECT_GT(validateErrors.Size(), 0u);
}
