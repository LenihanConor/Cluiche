// Suite: DiaAttributeModifierKinds
// Kind-aware modifier legality (parameter-files Task 22, AC 16).

#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <DiaAttribute/AttributeSet.h>
#include <DiaAttribute/Testing/AttributeTestHelpers.h>
#include <DiaCellLayers/Testing/LogCapture.h>

using namespace Dia::Attribute;
using namespace Dia::Attribute::Testing;
using Dia::Core::StringCRC;

namespace {

    AttributeModifier Mod(const char* name, const char* attr, ModifierOperation op, float v)
    {
        AttributeModifier m{};
        m.modifier_name = StringCRC(name);
        m.attribute_name = StringCRC(attr);
        m.operation = op;
        m.value = v;
        m.when_condition[0] = '\0';
        return m;
    }

    struct KindFixture
    {
        AttributeSchema schema;
        AttributeSet    set;
        KindFixture()
        {
            const std::string text = MakeSidecarText({
                SidecarFloat("f", 0, 100, 10),
                SidecarInt("i", 0, 10, 2),
                SidecarInt("ineg", -10, 10, 0),
                SidecarBool("b", false),
                SidecarEnum("e", { "A", "B", "C" }, "A") });
            schema = LoadSchemaOrDie(text.c_str());
            set.InitializeFromSchema(schema);
        }
    };

    float AddAndRead(KindFixture& fx, const char* attr, float v)
    {
        ModifierHandle h = fx.set.AddModifier(Mod("m", attr, ModifierOperation::Add, v));
        EXPECT_TRUE(h.IsValid());
        return fx.set.GetValue(StringCRC(attr));
    }
}

TEST(DiaAttributeModifierKinds, FloatAddMultiply)
{
    KindFixture fx;
    EXPECT_FLOAT_EQ(fx.set.GetValue(StringCRC("f")), 10.0f);
    ModifierHandle a = fx.set.AddModifier(Mod("a", "f", ModifierOperation::Add, 5.5f));
    ModifierHandle m = fx.set.AddModifier(Mod("m", "f", ModifierOperation::Multiply, 2.0f));
    EXPECT_TRUE(a.IsValid());
    EXPECT_TRUE(m.IsValid());
    EXPECT_FLOAT_EQ(fx.set.GetValue(StringCRC("f")), 31.0f);   // (10 + 5.5) * 2
}

TEST(DiaAttributeModifierKinds, IntAddMultiplyAreWholeNumbers)
{
    KindFixture fx;
    EXPECT_TRUE(fx.set.AddModifier(Mod("a", "i", ModifierOperation::Add, 1.0f)).IsValid());
    EXPECT_FLOAT_EQ(fx.set.GetValue(StringCRC("i")), 3.0f);
    EXPECT_TRUE(fx.set.AddModifier(Mod("m", "i", ModifierOperation::Multiply, 2.0f)).IsValid());
    EXPECT_FLOAT_EQ(fx.set.GetValue(StringCRC("i")), 6.0f);
}

TEST(DiaAttributeModifierKinds, IntRoundsPointFiveUp)
{
    KindFixture fx;
    EXPECT_FLOAT_EQ(AddAndRead(fx, "ineg", 0.5f), 1.0f);
}

TEST(DiaAttributeModifierKinds, IntRoundsMinusPointFiveAwayFromZero)
{
    KindFixture fx;
    EXPECT_FLOAT_EQ(AddAndRead(fx, "ineg", -0.5f), -1.0f);
}

TEST(DiaAttributeModifierKinds, IntRoundsTwoPointFiveToThree)
{
    KindFixture fx;
    EXPECT_FLOAT_EQ(AddAndRead(fx, "ineg", 2.5f), 3.0f);   // half away from zero, not banker rounding
}

TEST(DiaAttributeModifierKinds, IntRoundsMinusTwoPointFiveToMinusThree)
{
    KindFixture fx;
    EXPECT_FLOAT_EQ(AddAndRead(fx, "ineg", -2.5f), -3.0f);
}

TEST(DiaAttributeModifierKinds, IntClampsAfterRounding)
{
    KindFixture fx;
    EXPECT_FLOAT_EQ(AddAndRead(fx, "i", 8.4f), 10.0f);    // 10.4 -> 10
    KindFixture fx2;
    EXPECT_FLOAT_EQ(AddAndRead(fx2, "i", 8.6f), 10.0f);   // 10.6 -> 11 -> clamped 10
    KindFixture fx3;
    EXPECT_FLOAT_EQ(AddAndRead(fx3, "i", -50.0f), 0.0f);  // clamped to min
}

TEST(DiaAttributeModifierKinds, IntMultiplyRounds)
{
    KindFixture fx;
    EXPECT_TRUE(fx.set.AddModifier(Mod("m", "i", ModifierOperation::Multiply, 1.25f)).IsValid());   // 2.5 -> 3
    EXPECT_FLOAT_EQ(fx.set.GetValue(StringCRC("i")), 3.0f);
}

TEST(DiaAttributeModifierKinds, BoolAddMultiplyFailWithoutValueChange)
{
    KindFixture fx;
    EXPECT_FALSE(fx.set.AddModifier(Mod("a", "b", ModifierOperation::Add, 1.0f)).IsValid());
    EXPECT_FALSE(fx.set.AddModifier(Mod("m", "b", ModifierOperation::Multiply, 1.0f)).IsValid());
    EXPECT_FLOAT_EQ(fx.set.GetValue(StringCRC("b")), 0.0f);
    EXPECT_EQ(fx.set.GetModifierCountForAttribute(StringCRC("b")), 0u);
}

TEST(DiaAttributeModifierKinds, EnumAddMultiplyFailWithoutValueChange)
{
    KindFixture fx;
    EXPECT_FALSE(fx.set.AddModifier(Mod("a", "e", ModifierOperation::Add, 1.0f)).IsValid());
    EXPECT_FALSE(fx.set.AddModifier(Mod("m", "e", ModifierOperation::Multiply, 2.0f)).IsValid());
    EXPECT_FLOAT_EQ(fx.set.GetValue(StringCRC("e")), 0.0f);
    EXPECT_EQ(fx.set.GetModifierCountForAttribute(StringCRC("e")), 0u);
}

TEST(DiaAttributeModifierKinds, BoolOverrideValid)
{
    KindFixture fx;
    ModifierHandle h = fx.set.AddModifier(Mod("o", "b", ModifierOperation::Override, 1.0f));
    EXPECT_TRUE(h.IsValid());
    EXPECT_FLOAT_EQ(fx.set.GetValue(StringCRC("b")), 1.0f);
    fx.set.RemoveModifier(h);
    EXPECT_TRUE(fx.set.AddModifier(Mod("o0", "b", ModifierOperation::Override, 0.0f)).IsValid());
}

TEST(DiaAttributeModifierKinds, BoolOverrideInvalidValues)
{
    KindFixture fx;
    EXPECT_FALSE(fx.set.AddModifier(Mod("o", "b", ModifierOperation::Override, 2.0f)).IsValid());
    EXPECT_FALSE(fx.set.AddModifier(Mod("o", "b", ModifierOperation::Override, 0.5f)).IsValid());
    EXPECT_FALSE(fx.set.AddModifier(Mod("o", "b", ModifierOperation::Override, -1.0f)).IsValid());
    EXPECT_FLOAT_EQ(fx.set.GetValue(StringCRC("b")), 0.0f);
}

TEST(DiaAttributeModifierKinds, EnumOverrideValid)
{
    KindFixture fx;
    EXPECT_TRUE(fx.set.AddModifier(Mod("o", "e", ModifierOperation::Override, 2.0f)).IsValid());
    EXPECT_FLOAT_EQ(fx.set.GetValue(StringCRC("e")), 2.0f);
}

TEST(DiaAttributeModifierKinds, EnumOverrideInvalidValues)
{
    KindFixture fx;
    EXPECT_FALSE(fx.set.AddModifier(Mod("o", "e", ModifierOperation::Override, 3.0f)).IsValid());
    EXPECT_FALSE(fx.set.AddModifier(Mod("o", "e", ModifierOperation::Override, -1.0f)).IsValid());
    EXPECT_FALSE(fx.set.AddModifier(Mod("o", "e", ModifierOperation::Override, 1.5f)).IsValid());
    EXPECT_FLOAT_EQ(fx.set.GetValue(StringCRC("e")), 0.0f);
}

TEST(DiaAttributeModifierKinds, IntOverrideWholeNumberOnly)
{
    KindFixture fx;
    EXPECT_FALSE(fx.set.AddModifier(Mod("o", "i", ModifierOperation::Override, 2.5f)).IsValid());
    EXPECT_FLOAT_EQ(fx.set.GetValue(StringCRC("i")), 2.0f);
    EXPECT_TRUE(fx.set.AddModifier(Mod("o", "i", ModifierOperation::Override, 7.0f)).IsValid());
    EXPECT_FLOAT_EQ(fx.set.GetValue(StringCRC("i")), 7.0f);
}

TEST(DiaAttributeModifierKinds, FloatOverrideAnyValue)
{
    KindFixture fx;
    EXPECT_TRUE(fx.set.AddModifier(Mod("o", "f", ModifierOperation::Override, 33.3f)).IsValid());
    EXPECT_FLOAT_EQ(fx.set.GetValue(StringCRC("f")), 33.3f);
}

TEST(DiaAttributeModifierKinds, RemovalRestoresBase)
{
    KindFixture fx;
    ModifierHandle a = fx.set.AddModifier(Mod("a", "i", ModifierOperation::Add, 4.0f));
    ModifierHandle o = fx.set.AddModifier(Mod("o", "e", ModifierOperation::Override, 1.0f));
    EXPECT_FLOAT_EQ(fx.set.GetValue(StringCRC("i")), 6.0f);
    EXPECT_FLOAT_EQ(fx.set.GetValue(StringCRC("e")), 1.0f);
    fx.set.RemoveModifier(a);
    fx.set.RemoveModifier(o);
    EXPECT_FLOAT_EQ(fx.set.GetValue(StringCRC("i")), 2.0f);
    EXPECT_FLOAT_EQ(fx.set.GetValue(StringCRC("e")), 0.0f);
}

TEST(DiaAttributeModifierKinds, FloatStackingOrderUnchanged)
{
    KindFixture fx;
    // resolved = (base + sum(Add)) * product(Multiply), regardless of insertion order
    EXPECT_TRUE(fx.set.AddModifier(Mod("m", "f", ModifierOperation::Multiply, 3.0f)).IsValid());
    EXPECT_TRUE(fx.set.AddModifier(Mod("a1", "f", ModifierOperation::Add, 1.0f)).IsValid());
    EXPECT_TRUE(fx.set.AddModifier(Mod("a2", "f", ModifierOperation::Add, 2.0f)).IsValid());
    EXPECT_FLOAT_EQ(fx.set.GetValue(StringCRC("f")), 39.0f);   // (10+1+2)*3
}

// AC 16: an illegal Add/Multiply "fails and logs a warning naming the attribute".
TEST(DiaAttributeModifierKinds, IllegalOperationLogsOneWarningNamingTheAttribute)
{
    Dia::CellLayers::Testing::LogCapture log("Attribute");
    KindFixture fx;
    EXPECT_FALSE(fx.set.AddModifier(Mod("a", "b", ModifierOperation::Add, 1.0f)).IsValid());
    EXPECT_EQ(log.Warnings("'b'"), 1u);
    EXPECT_FALSE(fx.set.AddModifier(Mod("m", "e", ModifierOperation::Multiply, 2.0f)).IsValid());
    EXPECT_EQ(log.Warnings("'e'"), 1u);
    EXPECT_EQ(log.Warnings(), 2u);

    // A legal modifier logs nothing.
    log.Clear();
    EXPECT_TRUE(fx.set.AddModifier(Mod("ok", "f", ModifierOperation::Add, 1.0f)).IsValid());
    EXPECT_TRUE(fx.set.AddModifier(Mod("ovr", "b", ModifierOperation::Override, 1.0f)).IsValid());
    EXPECT_EQ(log.Warnings(), 0u);
}

// An illegal Override value (bool 2, enum ordinal out of range, fractional int) also logs a warning naming the attribute.
TEST(DiaAttributeModifierKinds, InvalidOverrideValueLogsWarningNamingTheAttribute)
{
    Dia::CellLayers::Testing::LogCapture log("Attribute");
    KindFixture fx;
    EXPECT_FALSE(fx.set.AddModifier(Mod("o1", "b", ModifierOperation::Override, 2.0f)).IsValid());
    EXPECT_EQ(log.Warnings("'b'"), 1u);
    EXPECT_FALSE(fx.set.AddModifier(Mod("o2", "e", ModifierOperation::Override, 3.0f)).IsValid());
    EXPECT_EQ(log.Warnings("'e'"), 1u);
    EXPECT_FALSE(fx.set.AddModifier(Mod("o3", "i", ModifierOperation::Override, 2.5f)).IsValid());
    EXPECT_EQ(log.Warnings("'i'"), 1u);
}
