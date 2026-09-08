#include <gtest/gtest.h>

#include <DiaCondition/ConditionExpr.h>
#include <DiaCondition/ConditionRegistry.h>
#include <DiaCondition/Testing/ConditionTestHelpers.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaCore/CRC/StringCRC.h>

// ---------------------------------------------------------------------------
// Helpers (local to this file)
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

    Dia::Condition::ConditionExpr LoadExpr(
        const char* json,
        Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors)
    {
        Json::Value root = ParseJson(json);
        return Dia::Condition::ConditionExpr::LoadFromJson(root, outErrors);
    }
}

// ---------------------------------------------------------------------------
// DiaCondition_Expr_Boundary
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Negative / zero threshold edge cases
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr_Boundary, FloatLeaf_NegativeThreshold_Gte_Correct)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":">=","slot":"obj","field":"temp","value":-10})", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("obj"), Dia::Core::StringCRC("temp"), -5.0f);

    // -5.0 >= -10.0 → true
    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);
}

TEST(DiaCondition_Expr_Boundary, FloatLeaf_ZeroThreshold_Lte_Correct)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"<=","slot":"obj","field":"delta","value":0})", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("obj"), Dia::Core::StringCRC("delta"), 0.0f);

    // 0.0 <= 0.0 → true
    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);
}

TEST(DiaCondition_Expr_Boundary, FloatLeaf_ZeroThreshold_Lt_Correct)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"<","slot":"obj","field":"delta","value":0})", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;

    // -0.001 < 0.0 → true
    ctx.SetFloat(Dia::Core::StringCRC("obj"), Dia::Core::StringCRC("delta"), -0.001f);
    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);

    // 0.0 < 0.0 → false (not strictly less)
    ctx.SetFloat(Dia::Core::StringCRC("obj"), Dia::Core::StringCRC("delta"), 0.0f);
    Dia::Condition::Testing::AssertExprResult(expr, ctx, false);
}

// ---------------------------------------------------------------------------
// Bool ordered comparisons: < and >
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr_Boundary, BoolLeaf_OrderedOps_Lt_FalseGtTrue)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errorsLt;
    Dia::Condition::ConditionExpr exprLt = LoadExpr(
        R"({"op":"<","slot":"hero","field":"flag","value":true})", errorsLt);
    ASSERT_TRUE(exprLt.IsValid());

    Dia::Condition::Testing::MockConditionContext ctxLt;

    // false (0) < true (1) → true
    ctxLt.SetBool(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("flag"), false);
    Dia::Condition::Testing::AssertExprResult(exprLt, ctxLt, true);

    // true (1) < true (1) → false
    ctxLt.SetBool(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("flag"), true);
    Dia::Condition::Testing::AssertExprResult(exprLt, ctxLt, false);

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errorsGt;
    Dia::Condition::ConditionExpr exprGt = LoadExpr(
        R"({"op":">","slot":"hero","field":"flag","value":false})", errorsGt);
    ASSERT_TRUE(exprGt.IsValid());

    Dia::Condition::Testing::MockConditionContext ctxGt;

    // true (1) > false (0) → true
    ctxGt.SetBool(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("flag"), true);
    Dia::Condition::Testing::AssertExprResult(exprGt, ctxGt, true);
}

// ---------------------------------------------------------------------------
// AND with 3 children
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr_Boundary, AndNode_ThreeChildren_AllTrue_True)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(R"(
    {
        "op": "and",
        "conditions": [
            {"op":">=","slot":"hero","field":"health","value":50},
            {"op":">","slot":"hero","field":"ammo","value":0},
            {"op":"==","slot":"hero","field":"alive","value":true}
        ]
    })", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 60.0f);
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("ammo"),    5.0f);
    ctx.SetBool (Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("alive"),   true);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);
}

TEST(DiaCondition_Expr_Boundary, AndNode_ThreeChildren_MiddleFalse_False)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(R"(
    {
        "op": "and",
        "conditions": [
            {"op":">=","slot":"hero","field":"health","value":50},
            {"op":">","slot":"hero","field":"ammo","value":0},
            {"op":"==","slot":"hero","field":"alive","value":true}
        ]
    })", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 60.0f);
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("ammo"),    0.0f);  // fails
    ctx.SetBool (Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("alive"),   true);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, false);
}

// ---------------------------------------------------------------------------
// OR with 3 children
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr_Boundary, OrNode_ThreeChildren_AllFalse_False)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(R"(
    {
        "op": "or",
        "conditions": [
            {"op":">=","slot":"hero","field":"health","value":50},
            {"op":">","slot":"hero","field":"ammo","value":0},
            {"op":"==","slot":"hero","field":"on_fire","value":true}
        ]
    })", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 20.0f);  // < 50  → false
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("ammo"),    0.0f);  // == 0  → false
    ctx.SetBool (Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("on_fire"), false); // false → false

    Dia::Condition::Testing::AssertExprResult(expr, ctx, false);
}

TEST(DiaCondition_Expr_Boundary, OrNode_ThreeChildren_OnlyLastTrue_True)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(R"(
    {
        "op": "or",
        "conditions": [
            {"op":">=","slot":"hero","field":"health","value":50},
            {"op":">","slot":"hero","field":"ammo","value":0},
            {"op":"==","slot":"hero","field":"on_fire","value":true}
        ]
    })", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 20.0f);  // fails
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("ammo"),    0.0f);  // fails
    ctx.SetBool (Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("on_fire"), true);  // passes

    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);
}

// ---------------------------------------------------------------------------
// NOT wrapping AND
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr_Boundary, NotNode_WrappingAndNode_InvertsCorrectly)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(R"(
    {
        "op": "not",
        "condition": {
            "op": "and",
            "conditions": [
                {"op":">=","slot":"hero","field":"health","value":50},
                {"op":"==","slot":"hero","field":"alive","value":true}
            ]
        }
    })", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;

    // Both children true → AND true → NOT false
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 60.0f);
    ctx.SetBool (Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("alive"),  true);
    Dia::Condition::Testing::AssertExprResult(expr, ctx, false);

    // One child false → AND false → NOT true
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 20.0f);
    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);
}

// ---------------------------------------------------------------------------
// Double NOT: NOT(NOT(leaf)) == leaf
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr_Boundary, DoubleNot_InvertsBack_Invariant)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr exprDouble = LoadExpr(R"(
    {
        "op": "not",
        "condition": {
            "op": "not",
            "condition": {"op":">=","slot":"hero","field":"health","value":50}
        }
    })", errors);
    ASSERT_TRUE(exprDouble.IsValid());

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errorsLeaf;
    Dia::Condition::ConditionExpr exprLeaf = LoadExpr(
        R"({"op":">=","slot":"hero","field":"health","value":50})", errorsLeaf);
    ASSERT_TRUE(exprLeaf.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;

    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 60.0f);
    EXPECT_EQ(exprDouble.Evaluate(ctx), exprLeaf.Evaluate(ctx));  // both true

    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 30.0f);
    EXPECT_EQ(exprDouble.Evaluate(ctx), exprLeaf.Evaluate(ctx));  // both false
}

// ---------------------------------------------------------------------------
// Empty conditions arrays on AND/OR nodes
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr_Boundary, AndNode_EmptyConditionsArray_IsInvalid)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"and","conditions":[]})", errors);

    EXPECT_FALSE(expr.IsValid());
    EXPECT_GT(errors.Size(), 0u);
}

TEST(DiaCondition_Expr_Boundary, OrNode_EmptyConditionsArray_IsInvalid)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"or","conditions":[]})", errors);

    EXPECT_FALSE(expr.IsValid());
    EXPECT_GT(errors.Size(), 0u);
}

// ---------------------------------------------------------------------------
// Unknown op is invalid
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr_Boundary, UnknownOp_IsInvalid)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"xor","conditions":[]})", errors);

    EXPECT_FALSE(expr.IsValid());
    EXPECT_GT(errors.Size(), 0u);
}

// ---------------------------------------------------------------------------
// String value in leaf node is invalid
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr_Boundary, LeafStringValue_IsInvalid)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"==","slot":"x","field":"y","value":"hello"})", errors);

    EXPECT_FALSE(expr.IsValid());
    EXPECT_GT(errors.Size(), 0u);
}

// ---------------------------------------------------------------------------
// Re-evaluation with changed context values
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr_Boundary, Evaluate_ContextValuesChangeBeforeSecondEval)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":">=","slot":"hero","field":"health","value":50})", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;

    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 60.0f);
    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);

    // Update context value below threshold — same expr, now false.
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 30.0f);
    Dia::Condition::Testing::AssertExprResult(expr, ctx, false);
}

// ---------------------------------------------------------------------------
// Validate: partial registration produces the correct error count
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr_Boundary, Validate_AndNode_OneLeafUnregistered_ErrorAdded)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(R"(
    {
        "op": "and",
        "conditions": [
            {"op":">=","slot":"hero","field":"health","value":50},
            {"op":"==","slot":"hero","field":"alive","value":true}
        ]
    })", errors);
    ASSERT_TRUE(expr.IsValid());

    float dummy = 0.0f;
    Dia::Condition::ConditionRegistry registry(&dummy);
    // Only register one of the two leaves.
    registry.RegisterFloat(
        Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"),
        [](void* d) { return *static_cast<float*>(d); });

    Dia::Core::Containers::DynamicArrayC<const char*, 32> validateErrors;
    EXPECT_FALSE(expr.Validate(registry, validateErrors));
    EXPECT_GE(validateErrors.Size(), 1u);
}

TEST(DiaCondition_Expr_Boundary, Validate_AndNode_BothUnregistered_TwoErrorsAdded)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(R"(
    {
        "op": "and",
        "conditions": [
            {"op":">=","slot":"hero","field":"health","value":50},
            {"op":"==","slot":"hero","field":"alive","value":true}
        ]
    })", errors);
    ASSERT_TRUE(expr.IsValid());

    // Empty registry — neither leaf is registered.
    float dummy = 0.0f;
    Dia::Condition::ConditionRegistry registry(&dummy);

    Dia::Core::Containers::DynamicArrayC<const char*, 32> validateErrors;
    EXPECT_FALSE(expr.Validate(registry, validateErrors));
    EXPECT_EQ(validateErrors.Size(), 2u);
}

TEST(DiaCondition_Expr_Boundary, Validate_NotNode_UnregisteredChild_ErrorAdded)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(R"(
    {
        "op": "not",
        "condition": {"op":">=","slot":"hero","field":"health","value":50}
    })", errors);
    ASSERT_TRUE(expr.IsValid());

    float dummy = 0.0f;
    Dia::Condition::ConditionRegistry registry(&dummy);  // empty

    Dia::Core::Containers::DynamicArrayC<const char*, 32> validateErrors;
    EXPECT_FALSE(expr.Validate(registry, validateErrors));
    EXPECT_GE(validateErrors.Size(), 1u);
}

// ---------------------------------------------------------------------------
// Deep nesting: AND( OR( NOT(a), b ), c ) with explicit truth table
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr_Boundary, DeepNesting_ThreeLevels_EvaluatesCorrectly)
{
    // expr = AND( OR( NOT(a>=50), b==true ), c>=1 )
    //   a = hero.health, b = hero.alive, c = hero.ammo
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(R"(
    {
        "op": "and",
        "conditions": [
            {
                "op": "or",
                "conditions": [
                    {
                        "op": "not",
                        "condition": {"op":">=","slot":"hero","field":"health","value":50}
                    },
                    {"op":"==","slot":"hero","field":"alive","value":true}
                ]
            },
            {"op":">=","slot":"hero","field":"ammo","value":1}
        ]
    })", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;

    // Case 1: a=true (health=60>=50), b=true, c=true(ammo=5)
    //   NOT(a)=false, OR(false,true)=true, AND(true,true)=true
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 60.0f);
    ctx.SetBool (Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("alive"),  true);
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("ammo"),   5.0f);
    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);

    // Case 2: a=true, b=false, c=true
    //   NOT(a)=false, OR(false,false)=false, AND(false,true)=false
    ctx.SetBool(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("alive"), false);
    Dia::Condition::Testing::AssertExprResult(expr, ctx, false);

    // Case 3: a=false (health=20<50), b=false, c=true
    //   NOT(a)=true, OR(true,false)=true, AND(true,true)=true
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 20.0f);
    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);

    // Case 4: a=false, b=false, c=false(ammo=0)
    //   NOT(a)=true, OR(true,false)=true, AND(true,false)=false
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("ammo"), 0.0f);
    Dia::Condition::Testing::AssertExprResult(expr, ctx, false);
}

// ===========================================================================
// DiaCondition_Expr_Stress
// ===========================================================================

TEST(DiaCondition_Expr_Stress, Stress_EvaluateSameExpr_1000Times_NoCorruption)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":">=","slot":"hero","field":"health","value":50})", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;

    for (int i = 0; i < 1000; ++i)
    {
        // Alternate between true and false each iteration.
        const float value = (i % 2 == 0) ? 60.0f : 30.0f;
        const bool  expected = (i % 2 == 0);
        ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), value);
        EXPECT_EQ(expr.Evaluate(ctx), expected) << "Iteration " << i;
    }
}

TEST(DiaCondition_Expr_Stress, Stress_LargeOrNode_TenChildren_CorrectResult)
{
    // Build an OR with 10 float leaf children all checking hero.health_N >= 50
    // using distinct field names health_0 .. health_9.
    // We keep them in separate slots by using distinct field names.
    static const char* const kFields[10] = {
        "h0","h1","h2","h3","h4","h5","h6","h7","h8","h9"
    };

    // Build JSON manually.
    std::string json = R"({"op":"or","conditions":[)";
    for (int i = 0; i < 10; ++i)
    {
        if (i > 0) json += ",";
        json += R"({"op":">=","slot":"hero","field":")";
        json += kFields[i];
        json += R"(","value":50})";
    }
    json += "]}";

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(json.c_str(), errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;

    // All failing → OR false
    for (int i = 0; i < 10; ++i)
        ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC(kFields[i]), 0.0f);
    EXPECT_FALSE(expr.Evaluate(ctx));

    // Set field h5 above threshold → OR true
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("h5"), 60.0f);
    EXPECT_TRUE(expr.Evaluate(ctx));
}

TEST(DiaCondition_Expr_Stress, Stress_LargeAndNode_TenChildren_CorrectResult)
{
    static const char* const kFields[10] = {
        "a0","a1","a2","a3","a4","a5","a6","a7","a8","a9"
    };

    std::string json = R"({"op":"and","conditions":[)";
    for (int i = 0; i < 10; ++i)
    {
        if (i > 0) json += ",";
        json += R"({"op":">=","slot":"unit","field":")";
        json += kFields[i];
        json += R"(","value":1})";
    }
    json += "]}";

    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(json.c_str(), errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;

    // All passing → AND true
    for (int i = 0; i < 10; ++i)
        ctx.SetFloat(Dia::Core::StringCRC("unit"), Dia::Core::StringCRC(kFields[i]), 5.0f);
    EXPECT_TRUE(expr.Evaluate(ctx));

    // Set a3 below threshold → AND false
    ctx.SetFloat(Dia::Core::StringCRC("unit"), Dia::Core::StringCRC("a3"), 0.0f);
    EXPECT_FALSE(expr.Evaluate(ctx));
}
