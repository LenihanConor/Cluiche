#include <gtest/gtest.h>

#include <DiaCondition/ConditionExpr.h>
#include <DiaCondition/ConditionRegistry.h>
#include <DiaCondition/Testing/ConditionTestHelpers.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Json/external/json/json.h>

#include <cstring>

namespace
{
    using Dia::Core::StringCRC;
    using Errors = Dia::Core::Containers::DynamicArrayC<const char*, 32>;

    Dia::Condition::ConditionExpr Load(const char* src)
    {
        Json::Value root;
        Json::Reader reader;
        EXPECT_TRUE(reader.parse(src, root));
        Errors errors;
        Dia::Condition::ConditionExpr expr = Dia::Condition::ConditionExpr::LoadFromJson(root, errors);
        EXPECT_TRUE(expr.IsValid());
        return expr;
    }

    Dia::Condition::Testing::TwoPairConditionContext MakeCtx()
    {
        return Dia::Condition::Testing::TwoPairConditionContext(
            StringCRC("cell"), StringCRC("height"), StringCRC("cell"), StringCRC("wet"));
    }

    bool ErrorsMention(const Errors& e, const char* text)
    {
        for (unsigned i = 0; i < e.Size(); ++i)
            if (std::strstr(e[i], text)) return true;
        return false;
    }

    // A context that does not override Has (all existing implementers).
    class NoHasContext : public Dia::Condition::IConditionContext
    {
    public:
        float GetFloat(StringCRC, StringCRC) const override { return 0.0f; }
        bool  GetBool (StringCRC, StringCRC) const override { return false; }
    };
}

TEST(ConditionValidateContext, KnownLeaf_Passes)
{
    auto expr = Load(R"({"op":">","slot":"cell","field":"height","value":1})");
    auto ctx = MakeCtx();
    Errors errors;
    EXPECT_TRUE(expr.Validate(ctx, errors));
    EXPECT_EQ(errors.Size(), 0u);
}

TEST(ConditionValidateContext, UnknownSlot_FailsNamingLeaf)
{
    auto expr = Load(R"({"op":">","slot":"bogus","field":"height","value":1})");
    auto ctx = MakeCtx();
    Errors errors;
    EXPECT_FALSE(expr.Validate(ctx, errors));
    ASSERT_EQ(errors.Size(), 1u);
    EXPECT_TRUE(ErrorsMention(errors, "bogus.height"));
}

TEST(ConditionValidateContext, UnknownField_FailsNamingLeaf)
{
    auto expr = Load(R"({"op":"<","slot":"cell","field":"nope","value":1})");
    auto ctx = MakeCtx();
    Errors errors;
    EXPECT_FALSE(expr.Validate(ctx, errors));
    EXPECT_TRUE(ErrorsMention(errors, "cell.nope"));
}

TEST(ConditionValidateContext, NestedAndOrNot_FindsOffendingLeaf)
{
    auto expr = Load(R"({"op":"and","conditions":[
        {"op":">","slot":"cell","field":"height","value":1},
        {"op":"or","conditions":[
            {"op":"==","slot":"cell","field":"wet","value":true},
            {"op":"not","condition":{"op":">","slot":"cell","field":"missing","value":0}}
        ]}]})");
    auto ctx = MakeCtx();
    Errors errors;
    EXPECT_FALSE(expr.Validate(ctx, errors));
    ASSERT_EQ(errors.Size(), 1u);
    EXPECT_TRUE(ErrorsMention(errors, "cell.missing"));
}

TEST(ConditionValidateContext, NestedAllKnown_Passes)
{
    auto expr = Load(R"({"op":"not","condition":{"op":"or","conditions":[
        {"op":">","slot":"cell","field":"height","value":1},
        {"op":"==","slot":"cell","field":"wet","value":true}]}})");
    auto ctx = MakeCtx();
    Errors errors;
    EXPECT_TRUE(expr.Validate(ctx, errors));
}

TEST(ConditionValidateContext, ContextWithoutHasOverride_BehavesAsBefore)
{
    NoHasContext ctx;
    EXPECT_TRUE(ctx.Has(StringCRC("a"), StringCRC("b")));
    auto expr = Load(R"({"op":">","slot":"anything","field":"goes","value":1})");
    Errors errors;
    EXPECT_TRUE(expr.Validate(ctx, errors));
}

TEST(ConditionValidateContext, RegistryOverload_StillUsesRegistry)
{
    auto expr = Load(R"({"op":">","slot":"hero","field":"health","value":1})");
    float dummy = 0.0f;
    Dia::Condition::ConditionRegistry registry(&dummy);
    Errors errors;
    EXPECT_FALSE(expr.Validate(registry, errors));
    EXPECT_GT(errors.Size(), 0u);
}

TEST(ConditionValidateContext, InvalidExpr_Fails)
{
    Dia::Condition::ConditionExpr expr;
    auto ctx = MakeCtx();
    Errors errors;
    EXPECT_FALSE(expr.Validate(ctx, errors));
    EXPECT_GT(errors.Size(), 0u);
}
