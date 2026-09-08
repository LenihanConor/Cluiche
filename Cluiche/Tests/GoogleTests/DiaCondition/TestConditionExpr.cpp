#include <gtest/gtest.h>

#include <DiaCondition/ConditionExpr.h>
#include <DiaCondition/ConditionRegistry.h>
#include <DiaCondition/Testing/ConditionTestHelpers.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
    // Parse a JSON string and return the root Json::Value.
    // Returns Json::nullValue on parse failure.
    Json::Value ParseJson(const char* src)
    {
        Json::Value root;
        Json::Reader reader;
        reader.parse(src, root);
        return root;
    }

    // Load a ConditionExpr from a JSON string.  outErrors is cleared first.
    Dia::Condition::ConditionExpr LoadExpr(
        const char* json,
        Dia::Core::Containers::DynamicArrayC<const char*, 32>& outErrors)
    {
        Json::Value root = ParseJson(json);
        return Dia::Condition::ConditionExpr::LoadFromJson(root, outErrors);
    }
}

// ---------------------------------------------------------------------------
// DiaCondition_Expr
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Default-constructed expr
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr, DefaultConstructed_IsValid_False)
{
    Dia::Condition::ConditionExpr expr;
    EXPECT_FALSE(expr.IsValid());
}

TEST(DiaCondition_Expr, DefaultConstructed_Evaluate_ReturnsFalse)
{
    Dia::Condition::ConditionExpr expr;
    Dia::Condition::Testing::MockConditionContext ctx;
    EXPECT_FALSE(expr.Evaluate(ctx));
}

// ---------------------------------------------------------------------------
// Move semantics
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr, MoveConstructed_FromValid_IsValidTrue)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr src = LoadExpr(
        R"({"op":">=","slot":"hero","field":"health","value":50})", errors);
    ASSERT_TRUE(src.IsValid());

    Dia::Condition::ConditionExpr dst(std::move(src));

    EXPECT_TRUE(dst.IsValid());
}

TEST(DiaCondition_Expr, MoveConstructed_SourceBecomesInvalid)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr src = LoadExpr(
        R"({"op":">=","slot":"hero","field":"health","value":50})", errors);
    ASSERT_TRUE(src.IsValid());

    Dia::Condition::ConditionExpr dst(std::move(src));

    EXPECT_FALSE(src.IsValid());  // moved-from must be invalid
}

TEST(DiaCondition_Expr, MoveAssign_FromValid_IsValidTrue)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr src = LoadExpr(
        R"({"op":">=","slot":"hero","field":"health","value":50})", errors);
    ASSERT_TRUE(src.IsValid());

    Dia::Condition::ConditionExpr dst;
    dst = std::move(src);

    EXPECT_TRUE(dst.IsValid());
    EXPECT_FALSE(src.IsValid());
}

// ---------------------------------------------------------------------------
// Float leaf comparisons — >=
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr, FloatLeaf_Gte_TrueWhenAboveThreshold)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":">=","slot":"hero","field":"health","value":50})", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 60.0f);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);
}

TEST(DiaCondition_Expr, FloatLeaf_Gte_FalseWhenBelowThreshold)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":">=","slot":"hero","field":"health","value":50})", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 40.0f);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, false);
}

TEST(DiaCondition_Expr, FloatLeaf_Gte_TrueWhenExactlyAtThreshold)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":">=","slot":"hero","field":"health","value":50})", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"), 50.0f);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);
}

// ---------------------------------------------------------------------------
// Float leaf — <
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr, FloatLeaf_Lt_TrueWhenBelow)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"<","slot":"sensor","field":"distance","value":20})", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("sensor"), Dia::Core::StringCRC("distance"), 10.0f);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);
}

TEST(DiaCondition_Expr, FloatLeaf_Lt_FalseWhenAtOrAbove)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"<","slot":"sensor","field":"distance","value":20})", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("sensor"), Dia::Core::StringCRC("distance"), 20.0f);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, false);
}

// ---------------------------------------------------------------------------
// Float leaf — ==
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr, FloatLeaf_Eq_TrueWhenEqual)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"==","slot":"world","field":"phase","value":3})", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("world"), Dia::Core::StringCRC("phase"), 3.0f);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);
}

TEST(DiaCondition_Expr, FloatLeaf_Eq_FalseWhenNotEqual)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"==","slot":"world","field":"phase","value":3})", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("world"), Dia::Core::StringCRC("phase"), 2.0f);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, false);
}

// ---------------------------------------------------------------------------
// Float leaf — !=
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr, FloatLeaf_Neq_TrueWhenDifferent)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"!=","slot":"hero","field":"stamina","value":0})", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("stamina"), 10.0f);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);
}

TEST(DiaCondition_Expr, FloatLeaf_Neq_FalseWhenEqual)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"!=","slot":"hero","field":"stamina","value":0})", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("stamina"), 0.0f);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, false);
}

// ---------------------------------------------------------------------------
// Float leaf — <=
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr, FloatLeaf_Lte_TrueWhenAtThreshold)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"<=","slot":"hero","field":"mana","value":100})", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("mana"), 100.0f);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);
}

TEST(DiaCondition_Expr, FloatLeaf_Lte_FalseWhenAbove)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"<=","slot":"hero","field":"mana","value":100})", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("mana"), 101.0f);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, false);
}

// ---------------------------------------------------------------------------
// Float leaf — >
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr, FloatLeaf_Gt_TrueWhenAbove)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":">","slot":"hero","field":"score","value":1000})", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 1500.0f);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);
}

TEST(DiaCondition_Expr, FloatLeaf_Gt_FalseWhenAtOrBelow)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":">","slot":"hero","field":"score","value":1000})", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("score"), 1000.0f);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, false);
}

// ---------------------------------------------------------------------------
// Bool leaf — ==
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr, BoolLeaf_Eq_TrueWhenBothTrue)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"==","slot":"enemy","field":"visible","value":true})", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("visible"), true);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);
}

TEST(DiaCondition_Expr, BoolLeaf_Eq_FalseWhenValueMismatch)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"==","slot":"enemy","field":"visible","value":true})", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("visible"), false);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, false);
}

TEST(DiaCondition_Expr, BoolLeaf_Eq_TrueWhenBothFalse)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"==","slot":"door","field":"open","value":false})", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("door"), Dia::Core::StringCRC("open"), false);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);
}

// ---------------------------------------------------------------------------
// Bool leaf — !=
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr, BoolLeaf_Neq_TrueWhenDifferent)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"!=","slot":"hero","field":"is_dead","value":false})", errors);
    ASSERT_TRUE(expr.IsValid());

    // hero.is_dead == true  →  true != false  →  condition true
    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("is_dead"), true);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);
}

TEST(DiaCondition_Expr, BoolLeaf_Neq_FalseWhenSame)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"!=","slot":"hero","field":"is_dead","value":false})", errors);
    ASSERT_TRUE(expr.IsValid());

    // hero.is_dead == false  →  false != false  →  condition false
    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("is_dead"), false);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, false);
}

// ---------------------------------------------------------------------------
// AND node
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr, AndNode_BothTrue_ReturnsTrue)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(R"(
    {
        "op": "and",
        "conditions": [
            {"op":">=","slot":"hero","field":"health","value":50},
            {"op":"==","slot":"enemy","field":"visible","value":true}
        ]
    })", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"),  Dia::Core::StringCRC("health"),  60.0f);
    ctx.SetBool (Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("visible"), true);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);
}

TEST(DiaCondition_Expr, AndNode_FirstFalse_ReturnsFalse)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(R"(
    {
        "op": "and",
        "conditions": [
            {"op":">=","slot":"hero","field":"health","value":50},
            {"op":"==","slot":"enemy","field":"visible","value":true}
        ]
    })", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"),  Dia::Core::StringCRC("health"),  30.0f);  // fails
    ctx.SetBool (Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("visible"), true);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, false);
}

TEST(DiaCondition_Expr, AndNode_SecondFalse_ReturnsFalse)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(R"(
    {
        "op": "and",
        "conditions": [
            {"op":">=","slot":"hero","field":"health","value":50},
            {"op":"==","slot":"enemy","field":"visible","value":true}
        ]
    })", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"),  Dia::Core::StringCRC("health"),  70.0f);
    ctx.SetBool (Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("visible"), false);  // fails

    Dia::Condition::Testing::AssertExprResult(expr, ctx, false);
}

// ---------------------------------------------------------------------------
// OR node
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr, OrNode_FirstTrue_ReturnsTrue)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(R"(
    {
        "op": "or",
        "conditions": [
            {"op":"==","slot":"hero","field":"on_fire","value":true},
            {"op":"==","slot":"hero","field":"poisoned","value":true}
        ]
    })", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("on_fire"),  true);
    ctx.SetBool(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("poisoned"), false);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);
}

TEST(DiaCondition_Expr, OrNode_SecondTrue_ReturnsTrue)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(R"(
    {
        "op": "or",
        "conditions": [
            {"op":"==","slot":"hero","field":"on_fire","value":true},
            {"op":"==","slot":"hero","field":"poisoned","value":true}
        ]
    })", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("on_fire"),  false);
    ctx.SetBool(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("poisoned"), true);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);
}

TEST(DiaCondition_Expr, OrNode_BothFalse_ReturnsFalse)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(R"(
    {
        "op": "or",
        "conditions": [
            {"op":"==","slot":"hero","field":"on_fire","value":true},
            {"op":"==","slot":"hero","field":"poisoned","value":true}
        ]
    })", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("on_fire"),  false);
    ctx.SetBool(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("poisoned"), false);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, false);
}

// ---------------------------------------------------------------------------
// NOT node
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr, NotNode_ChildTrue_ReturnsFalse)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(R"(
    {
        "op": "not",
        "condition": {"op":"==","slot":"hero","field":"grounded","value":true}
    })", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("grounded"), true);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, false);
}

TEST(DiaCondition_Expr, NotNode_ChildFalse_ReturnsTrue)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(R"(
    {
        "op": "not",
        "condition": {"op":"==","slot":"hero","field":"grounded","value":true}
    })", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetBool(Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("grounded"), false);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);
}

// ---------------------------------------------------------------------------
// Nested composite: AND(health>=50, OR(enemy_visible, has_ammo))
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr, Nested_AndOrComposite_AllConditionsMet_True)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(R"(
    {
        "op": "and",
        "conditions": [
            {"op":">=","slot":"hero","field":"health","value":50},
            {
                "op": "or",
                "conditions": [
                    {"op":"==","slot":"enemy","field":"visible","value":true},
                    {"op":">","slot":"hero","field":"ammo","value":0}
                ]
            }
        ]
    })", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"),  Dia::Core::StringCRC("health"),  60.0f);
    ctx.SetBool (Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("visible"), true);
    ctx.SetFloat(Dia::Core::StringCRC("hero"),  Dia::Core::StringCRC("ammo"),    5.0f);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, true);
}

TEST(DiaCondition_Expr, Nested_AndOrComposite_HealthFails_ReturnsFalse)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(R"(
    {
        "op": "and",
        "conditions": [
            {"op":">=","slot":"hero","field":"health","value":50},
            {
                "op": "or",
                "conditions": [
                    {"op":"==","slot":"enemy","field":"visible","value":true},
                    {"op":">","slot":"hero","field":"ammo","value":0}
                ]
            }
        ]
    })", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"),  Dia::Core::StringCRC("health"),  20.0f);  // fails
    ctx.SetBool (Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("visible"), true);
    ctx.SetFloat(Dia::Core::StringCRC("hero"),  Dia::Core::StringCRC("ammo"),    5.0f);

    Dia::Condition::Testing::AssertExprResult(expr, ctx, false);
}

TEST(DiaCondition_Expr, Nested_AndOrComposite_OrChildrenBothFail_ReturnsFalse)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(R"(
    {
        "op": "and",
        "conditions": [
            {"op":">=","slot":"hero","field":"health","value":50},
            {
                "op": "or",
                "conditions": [
                    {"op":"==","slot":"enemy","field":"visible","value":true},
                    {"op":">","slot":"hero","field":"ammo","value":0}
                ]
            }
        ]
    })", errors);
    ASSERT_TRUE(expr.IsValid());

    Dia::Condition::Testing::MockConditionContext ctx;
    ctx.SetFloat(Dia::Core::StringCRC("hero"),  Dia::Core::StringCRC("health"),  60.0f);
    ctx.SetBool (Dia::Core::StringCRC("enemy"), Dia::Core::StringCRC("visible"), false);  // fails
    ctx.SetFloat(Dia::Core::StringCRC("hero"),  Dia::Core::StringCRC("ammo"),    0.0f);   // fails

    Dia::Condition::Testing::AssertExprResult(expr, ctx, false);
}

// ---------------------------------------------------------------------------
// Malformed JSON — IsValid() false, outErrors populated
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr, Malformed_MissingOp_IsInvalid)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"slot":"hero","field":"health","value":50})", errors);

    EXPECT_FALSE(expr.IsValid());
    EXPECT_GT(errors.Size(), 0u);
}

TEST(DiaCondition_Expr, Malformed_AndMissingConditionsArray_IsInvalid)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"and"})", errors);

    EXPECT_FALSE(expr.IsValid());
    EXPECT_GT(errors.Size(), 0u);
}

TEST(DiaCondition_Expr, Malformed_OrMissingConditionsArray_IsInvalid)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"or"})", errors);

    EXPECT_FALSE(expr.IsValid());
    EXPECT_GT(errors.Size(), 0u);
}

TEST(DiaCondition_Expr, Malformed_NotMissingConditionObject_IsInvalid)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":"not"})", errors);

    EXPECT_FALSE(expr.IsValid());
    EXPECT_GT(errors.Size(), 0u);
}

TEST(DiaCondition_Expr, Malformed_LeafMissingSlot_IsInvalid)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":">=","field":"health","value":50})", errors);

    EXPECT_FALSE(expr.IsValid());
    EXPECT_GT(errors.Size(), 0u);
}

TEST(DiaCondition_Expr, Malformed_LeafMissingField_IsInvalid)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":">=","slot":"hero","value":50})", errors);

    EXPECT_FALSE(expr.IsValid());
    EXPECT_GT(errors.Size(), 0u);
}

TEST(DiaCondition_Expr, Malformed_LeafMissingValue_IsInvalid)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":">=","slot":"hero","field":"health"})", errors);

    EXPECT_FALSE(expr.IsValid());
    EXPECT_GT(errors.Size(), 0u);
}

// ---------------------------------------------------------------------------
// Validate
// ---------------------------------------------------------------------------

TEST(DiaCondition_Expr, Validate_AllLeavesRegistered_ReturnsTrue)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":">=","slot":"hero","field":"health","value":50})", errors);
    ASSERT_TRUE(expr.IsValid());

    float dummy = 0.0f;
    Dia::Condition::ConditionRegistry registry(&dummy);
    registry.RegisterFloat(
        Dia::Core::StringCRC("hero"), Dia::Core::StringCRC("health"),
        [](void* d) { return *static_cast<float*>(d); });

    Dia::Core::Containers::DynamicArrayC<const char*, 32> validateErrors;
    EXPECT_TRUE(expr.Validate(registry, validateErrors));
    EXPECT_EQ(validateErrors.Size(), 0u);
}

TEST(DiaCondition_Expr, Validate_UnresolvableLeaf_ReturnsFalse_ErrorAdded)
{
    Dia::Core::Containers::DynamicArrayC<const char*, 32> errors;
    Dia::Condition::ConditionExpr expr = LoadExpr(
        R"({"op":">=","slot":"hero","field":"health","value":50})", errors);
    ASSERT_TRUE(expr.IsValid());

    // Empty registry — leaf "hero.health" is not registered.
    float dummy = 0.0f;
    Dia::Condition::ConditionRegistry registry(&dummy);

    Dia::Core::Containers::DynamicArrayC<const char*, 32> validateErrors;
    EXPECT_FALSE(expr.Validate(registry, validateErrors));
    EXPECT_GT(validateErrors.Size(), 0u);
}

TEST(DiaCondition_Expr, Validate_InvalidExpr_ReturnsFalse_ErrorAdded)
{
    // Default-constructed expr is invalid — Validate must return false.
    Dia::Condition::ConditionExpr expr;
    ASSERT_FALSE(expr.IsValid());

    float dummy = 0.0f;
    Dia::Condition::ConditionRegistry registry(&dummy);
    Dia::Core::Containers::DynamicArrayC<const char*, 32> validateErrors;

    EXPECT_FALSE(expr.Validate(registry, validateErrors));
    EXPECT_GT(validateErrors.Size(), 0u);
}
