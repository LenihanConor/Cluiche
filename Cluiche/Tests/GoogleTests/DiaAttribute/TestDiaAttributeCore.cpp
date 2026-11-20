// Suite: DiaAttribute
// Covers all acceptance criteria (AC-1..AC-15) from Task 1 (Core) of the DiaAttribute spec.

#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <DiaAttribute/AttributeSchema.h>
#include <DiaAttribute/AttributeSet.h>
#include <DiaAttribute/AttributeSetComponent.h>
#include <DiaAttribute/AttributeAccessorBridge.h>
#include <DiaAttribute/Testing/AttributeTestHelpers.h>

#include <DiaCondition/ConditionRegistry.h>

#include <DiaEntity/Domain.h>
#include <DiaEntity/ComponentPool.h>

#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/LogEntry.h>
#include <DiaObservation/Log/LogLevel.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Core/Assert.h>
#include <DiaCore/Json/external/json/json.h>

#include <cfloat>
#include <string.h>
#include <stdio.h>
#include <cmath>

using namespace Dia::Attribute;
using Dia::Core::StringCRC;
using namespace Dia::Attribute::Testing;

// ===========================================================================
// Helpers
// ===========================================================================

static AttributeSchema MakeDragonSchema()
{
    const std::string text = DragonSidecarText();
    return LoadSchemaOrDie(text.c_str());
}

// Attempts to build a one-attribute ("health") schema; false (with err naming the key) if it is rejected.
static bool TryMakeSingleAttributeSchema(float min_val, float max_val, float default_val, AttributeSchema& out, SchemaError& err)
{
    const std::string text = MakeSidecarText({ SidecarFloat("health", min_val, max_val, default_val) }, nullptr, "test_schema");
    return TryLoadSchema(text.c_str(), out, err);
}

static AttributeModifier MakeModifier(const char* name, const char* attr, ModifierOperation op, float value)
{
    AttributeModifier mod{};
    mod.modifier_name     = StringCRC(name);
    mod.attribute_name    = StringCRC(attr);
    mod.operation         = op;
    mod.value             = value;
    mod.when_condition[0] = '\0';
    return mod;
}

static AttributeModifier MakeConditionalModifier(const char* name, const char* attr, ModifierOperation op, float value, const char* whenConditionJson)
{
    AttributeModifier mod{};
    mod.modifier_name  = StringCRC(name);
    mod.attribute_name = StringCRC(attr);
    mod.operation       = op;
    mod.value           = value;
    strncpy_s(mod.when_condition, sizeof(mod.when_condition), whenConditionJson, _TRUNCATE);
    return mod;
}

// ===========================================================================
// AC-1 / AC-2 — AttributeSchema load + validation
// ===========================================================================

TEST(DiaAttribute, SchemaLoad_ValidSidecarText_IsValidAndAttributesMatch)
{
    const std::string text = MakeSidecarText({ SidecarFloat("health", 0, 2000, 1000) }, nullptr, "dragon");
    AttributeSchema schema;
    SchemaError err;
    ASSERT_TRUE(TryLoadSchema(text.c_str(), schema, err)) << err.key.AsCStr() << ": " << err.reason.AsCStr();
    EXPECT_TRUE(schema.IsValid());

    const AttributeDefinition* def = schema.FindAttribute(StringCRC("health"));
    ASSERT_NE(def, nullptr);
    EXPECT_FLOAT_EQ(def->minimum_value, 0.0f);
    EXPECT_FLOAT_EQ(def->maximum_value, 2000.0f);
    EXPECT_FLOAT_EQ(def->default_value, 1000.0f);
}

TEST(DiaAttribute, SchemaLoad_DragonSidecar_IsValidAndAttributesMatch)
{
    AttributeSchema schema = MakeDragonSchema();
    EXPECT_TRUE(schema.IsValid());
    EXPECT_EQ(schema.GetAttributeCount(), 2u);

    const AttributeDefinition* health = schema.FindAttribute(StringCRC("health"));
    ASSERT_NE(health, nullptr);
    EXPECT_FLOAT_EQ(health->minimum_value, 0.0f);
    EXPECT_FLOAT_EQ(health->maximum_value, 2000.0f);
    EXPECT_FLOAT_EQ(health->default_value, 1000.0f);
}

TEST(DiaAttribute, SchemaLoad_MalformedSidecarText_IsInvalid)
{
    AttributeSchema schema;
    SchemaError err;
    EXPECT_FALSE(TryLoadSchema("{ this is not json", schema, err));
    EXPECT_FALSE(schema.IsValid());
    EXPECT_EQ(schema.GetAttributeCount(), 0u);
}

TEST(DiaAttribute, SchemaLoad_ParameterWithoutDefault_IsInvalidNamingKey)
{
    const char* text =
        "{\"format\":\"dia.value_schema\",\"version\":1,\"for\":\"test.params\",\"entries\":["
        "{\"key\":\"mystery\",\"kind\":\"parameter\",\"label\":\"l\",\"description\":\"d\",\"type\":\"float\",\"min\":0.0,\"max\":10.0}"
        "]}";
    AttributeSchema schema;
    SchemaError err;
    EXPECT_FALSE(TryLoadSchema(text, schema, err));
    EXPECT_FALSE(schema.IsValid());
    EXPECT_STREQ(err.key.AsCStr(), "mystery");
}

TEST(DiaAttribute, SchemaLoad_DefaultAboveMaximum_IsInvalid)
{
    AttributeSchema schema;
    SchemaError err;
    EXPECT_FALSE(TryMakeSingleAttributeSchema(0.0f, 100.0f, 500.0f, schema, err)); // default > max
    EXPECT_FALSE(schema.IsValid());
    EXPECT_STREQ(err.key.AsCStr(), "health");
}

TEST(DiaAttribute, SchemaLoad_DefaultBelowMinimum_IsInvalid)
{
    AttributeSchema schema;
    SchemaError err;
    EXPECT_FALSE(TryMakeSingleAttributeSchema(50.0f, 100.0f, 0.0f, schema, err)); // default < min
    EXPECT_FALSE(schema.IsValid());
    EXPECT_STREQ(err.key.AsCStr(), "health");
}

TEST(DiaAttribute, FindAttribute_UnknownName_ReturnsNull)
{
    AttributeSchema schema = MakeDragonSchema();
    EXPECT_EQ(schema.FindAttribute(StringCRC("mana")), nullptr);
}

// ===========================================================================
// AttributeSchema copy semantics (copy-ctor / operator= deep-copy the internal
// DynamicArray rather than shallow-copying it — see AttributeSchema.cpp).
// ===========================================================================

TEST(DiaAttribute, SchemaCopy_CopyConstruct_MatchesOriginalExactly)
{
    AttributeSchema original = MakeDragonSchema();
    AttributeSchema copy(original);

    EXPECT_EQ(copy.GetAttributeCount(), original.GetAttributeCount());
    ASSERT_EQ(copy.GetAttributeCount(), 2u);

    for (unsigned int i = 0; i < copy.GetAttributeCount(); ++i)
    {
        const AttributeDefinition& a = original.GetAttributeByIndex(i);
        const AttributeDefinition& b = copy.GetAttributeByIndex(i);
        EXPECT_TRUE(a.attribute_name == b.attribute_name) << "index " << i;
        EXPECT_FLOAT_EQ(a.minimum_value, b.minimum_value) << "index " << i;
        EXPECT_FLOAT_EQ(a.maximum_value, b.maximum_value) << "index " << i;
        EXPECT_FLOAT_EQ(a.default_value, b.default_value) << "index " << i;
    }

    const AttributeDefinition* health = copy.FindAttribute(StringCRC("health"));
    ASSERT_NE(health, nullptr);
    EXPECT_FLOAT_EQ(health->default_value, 1000.0f);
}

TEST(DiaAttribute, SchemaCopy_SelfAssignment_DoesNotCorruptState)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSchema& ref = schema;
    schema = ref;

    EXPECT_EQ(schema.GetAttributeCount(), 2u);
    EXPECT_TRUE(schema.IsValid());

    const AttributeDefinition* health = schema.FindAttribute(StringCRC("health"));
    ASSERT_NE(health, nullptr);
    EXPECT_FLOAT_EQ(health->minimum_value, 0.0f);
    EXPECT_FLOAT_EQ(health->maximum_value, 2000.0f);
    EXPECT_FLOAT_EQ(health->default_value, 1000.0f);

    const AttributeDefinition* strength = schema.FindAttribute(StringCRC("strength"));
    ASSERT_NE(strength, nullptr);
    EXPECT_FLOAT_EQ(strength->default_value, 50.0f);
}

TEST(DiaAttribute, SchemaCopy_EmptySchema_NoCrash_ZeroCount)
{
    const std::string emptyText = MakeSidecarText({}, nullptr, "empty");
    AttributeSchema empty = LoadSchemaOrDie(emptyText.c_str());
    ASSERT_EQ(empty.GetAttributeCount(), 0u);

    AttributeSchema copy(empty);
    EXPECT_EQ(copy.GetAttributeCount(), 0u);

    AttributeSchema assigned;
    assigned = empty;
    EXPECT_EQ(assigned.GetAttributeCount(), 0u);
}

TEST(DiaAttribute, SchemaCopy_OriginalDestroyedAfterCopy_CopyDataStillIntact)
{
    AttributeSchema copy;
    {
        AttributeSchema original = MakeDragonSchema();
        copy = original; // deep copy via operator= (or copy-ctor) — no aliasing of original's storage
    } // `original` destroyed here

    // If the copy had aliased `original`'s DynamicArray storage, this would now be
    // reading freed/reused memory instead of correct data.
    ASSERT_EQ(copy.GetAttributeCount(), 2u);
    const AttributeDefinition* health = copy.FindAttribute(StringCRC("health"));
    ASSERT_NE(health, nullptr);
    EXPECT_FLOAT_EQ(health->minimum_value, 0.0f);
    EXPECT_FLOAT_EQ(health->maximum_value, 2000.0f);
    EXPECT_FLOAT_EQ(health->default_value, 1000.0f);

    const AttributeDefinition* strength = copy.FindAttribute(StringCRC("strength"));
    ASSERT_NE(strength, nullptr);
    EXPECT_FLOAT_EQ(strength->default_value, 50.0f);
}

// ===========================================================================
// Sidecar text that is not an object (null / array / string) is rejected
// ===========================================================================

TEST(DiaAttribute, SchemaLoad_NullSidecarText_IsInvalid)
{
    AttributeSchema schema;
    SchemaError err;
    EXPECT_FALSE(TryLoadSchema("null", schema, err));
    EXPECT_FALSE(schema.IsValid());
    EXPECT_EQ(schema.GetAttributeCount(), 0u);
}

TEST(DiaAttribute, SchemaLoad_NonObjectSidecarText_ArrayIsInvalid)
{
    AttributeSchema schema;
    SchemaError err;
    EXPECT_FALSE(TryLoadSchema("[]", schema, err));
    EXPECT_FALSE(schema.IsValid());
}

TEST(DiaAttribute, SchemaLoad_NonObjectSidecarText_StringIsInvalid)
{
    AttributeSchema schema;
    SchemaError err;
    EXPECT_FALSE(TryLoadSchema("\"not an object\"", schema, err));
    EXPECT_FALSE(schema.IsValid());
}

// ===========================================================================
// Omitted fields. The old loader defaulted omitted min/max/default to 0.0f; a sidecar
// requires a default (rejected, naming the key) and treats an omitted bound as unbounded.
// ===========================================================================

TEST(DiaAttribute, SchemaLoad_OmittedRangeFields_NoDefaultIsRejected)
{
    const char* text =
        "{\"format\":\"dia.value_schema\",\"version\":1,\"for\":\"test.params\",\"entries\":["
        "{\"key\":\"mystery\",\"kind\":\"parameter\",\"label\":\"l\",\"description\":\"d\",\"type\":\"float\"}"
        "]}";
    AttributeSchema schema;
    SchemaError err;
    EXPECT_FALSE(TryLoadSchema(text, schema, err));
    EXPECT_EQ(schema.FindAttribute(StringCRC("mystery")), nullptr);
    EXPECT_STREQ(err.key.AsCStr(), "mystery");
}

TEST(DiaAttribute, SchemaLoad_OmittedMaximumOnly_IsUnbounded_OthersRespected)
{
    const char* text =
        "{\"format\":\"dia.value_schema\",\"version\":1,\"for\":\"test.params\",\"entries\":["
        "{\"key\":\"partial\",\"kind\":\"parameter\",\"label\":\"l\",\"description\":\"d\",\"type\":\"float\",\"min\":-10.0,\"default\":-5.0}"
        "]}";
    AttributeSchema schema;
    SchemaError err;
    ASSERT_TRUE(TryLoadSchema(text, schema, err)) << err.key.AsCStr() << ": " << err.reason.AsCStr();

    const AttributeDefinition* def = schema.FindAttribute(StringCRC("partial"));
    ASSERT_NE(def, nullptr);
    EXPECT_FLOAT_EQ(def->minimum_value, -10.0f);
    EXPECT_FLOAT_EQ(def->maximum_value, FLT_MAX); // omitted -> unbounded
    EXPECT_FLOAT_EQ(def->default_value, -5.0f);
}

// ===========================================================================
// AC-3 / AC-4 — AttributeSet construction + zero-modifier resolution
// ===========================================================================

TEST(DiaAttribute, AttributeSet_CreateFromSchema_BaseValuesMatchDefaults)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("health")), 1000.0f);
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("strength")), 50.0f);
}

TEST(DiaAttribute, GetValue_NoModifiers_EqualsBaseValue)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), set.GetBaseValue(StringCRC("health")));
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1000.0f);
}

// ===========================================================================
// AC-5 / AC-6 / AC-7 / AC-8 — modifier resolution pipeline
// ===========================================================================

TEST(DiaAttribute, AddModifier_SingleAdd_ShiftsValueByExactAmount)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    ModifierHandle h = set.AddModifier(MakeModifier("buff", "health", ModifierOperation::Add, 50.0f));
    EXPECT_TRUE(h.IsValid());
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1050.0f);
}

TEST(DiaAttribute, AddModifier_TwoAdds_Sum)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    set.AddModifier(MakeModifier("buff1", "health", ModifierOperation::Add, 50.0f));
    set.AddModifier(MakeModifier("buff2", "health", ModifierOperation::Add, 25.0f));
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1075.0f);
}

TEST(DiaAttribute, AddModifier_MultipleMultiply_CombineMultiplicatively)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    // strength: base 50, headroom to 200 avoids clamp interference with the math being checked.
    set.AddModifier(MakeModifier("buff_str", "strength", ModifierOperation::Add, 10.0f));       // 50+10 = 60
    set.AddModifier(MakeModifier("mult1_str", "strength", ModifierOperation::Multiply, 2.0f));  // *2.0
    set.AddModifier(MakeModifier("mult2_str", "strength", ModifierOperation::Multiply, 1.5f));  // *1.5 (product 3.0)
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("strength")), 180.0f); // 60 * 3.0
}

TEST(DiaAttribute, AddModifier_MultiplyAppliesToBasePlusAddSum)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    set.AddModifier(MakeModifier("buff", "strength", ModifierOperation::Add, 10.0f));      // 50+10 = 60
    set.AddModifier(MakeModifier("mult", "strength", ModifierOperation::Multiply, 2.0f));  // *2.0 = 120
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("strength")), 120.0f);
}

TEST(DiaAttribute, AddModifier_OverrideReplacesResolvedValue_ClampAppliesAfter)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    set.AddModifier(MakeModifier("buff", "strength", ModifierOperation::Add, 10.0f));
    set.AddModifier(MakeModifier("mult", "strength", ModifierOperation::Multiply, 2.0f));
    // resolved so far (pre-Override) would be (50+10)*2 = 120; Override replaces it entirely.
    set.AddModifier(MakeModifier("ovr", "strength", ModifierOperation::Override, 75.0f));
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("strength")), 75.0f);

    // Override value itself is still clamped (AC-10) — clamp happens AFTER Override.
    set.AddModifier(MakeModifier("ovr_big", "health", ModifierOperation::Override, 999999.0f));
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 2000.0f); // maximum_value
}

#ifdef _DEBUG
TEST(SLOW_DiaAttribute, AddModifier_SecondOverrideOnSameAttribute_Asserts)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    set.AddModifier(MakeModifier("ovr1", "strength", ModifierOperation::Override, 100.0f));
    DIA_EXPECT_DEATH(set.AddModifier(MakeModifier("ovr2", "strength", ModifierOperation::Override, 50.0f)), "");
}

// Coverage gap: what actually happens on the code path AFTER that DIA_ASSERT(false, ...)
// fires — the assert does not early-return, so AddModifier falls through and adds the
// second Override anyway, and ResolveValue's modifier loop (no `break`) keeps overwriting
// overrideValue for every Override it iterates, so the LAST Override added wins.
//
// Proven by execution (not just documented in a comment) using the swap-g_pAssertFunc-
// for-a-recording-no-op precedent already established in
// Core/Threading/TestJobSystem.cpp and DiaMessageBus/TestDiaMessageBusCore.cpp — the
// established mechanism this codebase uses to continue execution past a
// DIA_ASSERT(false, ...) inside a (non-death) test instead of crashing on __debugbreak().
namespace
{
    void NoOpAssertHandler_SecondOverride(const char*, const char*, int, const char*, ...) {}
}

TEST(DiaAttribute, AddModifier_SecondOverrideOnSameAttribute_PastAssert_LastOverrideWins)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    auto* prevAssertFunc = Dia::Core::g_pAssertFunc;
    Dia::Core::g_pAssertFunc = NoOpAssertHandler_SecondOverride;

    set.AddModifier(MakeModifier("ovr1", "strength", ModifierOperation::Override, 100.0f));
    ModifierHandle h2 = set.AddModifier(MakeModifier("ovr2", "strength", ModifierOperation::Override, 50.0f));

    Dia::Core::g_pAssertFunc = prevAssertFunc;

    EXPECT_TRUE(h2.IsValid());
    EXPECT_EQ(set.GetModifierCountForAttribute(StringCRC("strength")), 2u);
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("strength")), 50.0f); // last Override iterated wins
}
#endif // _DEBUG

// ===========================================================================
// AC-10 — clamp
// ===========================================================================

TEST(DiaAttribute, GetValue_AlwaysClampedToSchemaRange)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    set.AddModifier(MakeModifier("bigadd", "health", ModifierOperation::Add, 5000.0f));
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 2000.0f); // maximum_value

    set.SetBaseValue(StringCRC("strength"), -500.0f);
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("strength")), 0.0f); // minimum_value
}

// ===========================================================================
// Modifier-stack-full rejection (kMaxModifiersPerAttribute cap) — see the
// AddModifier_StackFull_RejectsAndLogsWarning TEST_F near the end of this file
// for the non-death (returned-handle + logged-warning) coverage; this is the
// Debug-assert companion, matching the existing AC-5/AC-10 SLOW_ convention.
// ===========================================================================

#ifdef _DEBUG
TEST(SLOW_DiaAttribute, AddModifier_StackFull_AssertsInDebug)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    for (unsigned int i = 0; i < AttributeSet::kMaxModifiersPerAttribute; ++i)
    {
        char name[32];
        snprintf(name, sizeof(name), "buff_%02u", i);
        ModifierHandle h = set.AddModifier(MakeModifier(name, "health", ModifierOperation::Add, 1.0f));
        ASSERT_TRUE(h.IsValid()) << "index " << i;
    }

    DIA_EXPECT_DEATH(set.AddModifier(MakeModifier("overflow", "health", ModifierOperation::Add, 1.0f)), "");
}
#endif // _DEBUG

// ===========================================================================
// AC-11 / AC-12 — ModifierHandle add/remove
// ===========================================================================

TEST(DiaAttribute, RemoveModifier_RemovesExactlyThatModifier)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    ModifierHandle h1 = set.AddModifier(MakeModifier("buff1", "health", ModifierOperation::Add, 50.0f));
    ModifierHandle h2 = set.AddModifier(MakeModifier("buff2", "health", ModifierOperation::Add, 25.0f));
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1075.0f);

    set.RemoveModifier(h1);
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1025.0f);

    set.RemoveModifier(h2);
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1000.0f);
}

#ifdef _DEBUG
TEST(SLOW_DiaAttribute, RemoveModifier_DoubleRemove_Asserts)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    ModifierHandle h = set.AddModifier(MakeModifier("buff", "health", ModifierOperation::Add, 50.0f));
    set.RemoveModifier(h);
    DIA_EXPECT_DEATH(set.RemoveModifier(h), "");
}
#else
TEST(DiaAttribute, RemoveModifier_InvalidHandle_ReleaseNoOp_DoesNotAffectOtherModifiers)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    set.AddModifier(MakeModifier("buff", "health", ModifierOperation::Add, 50.0f));
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1050.0f);

    ModifierHandle invalid; // default-constructed handle is invalid
    set.RemoveModifier(invalid);

    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1050.0f);
}
#endif // _DEBUG

// ===========================================================================
// AC-13 — SetBaseValue
// ===========================================================================

TEST(DiaAttribute, SetBaseValue_UpdatesPipelineWithoutTouchingModifierStack)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    set.AddModifier(MakeModifier("buff", "health", ModifierOperation::Add, 50.0f));
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1050.0f);

    set.SetBaseValue(StringCRC("health"), 500.0f);
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("health")), 500.0f);
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 550.0f); // modifier is still applied
}

// ===========================================================================
// AC-14 — AttributeSetComponent wraps exactly one AttributeSet
// ===========================================================================

TEST(DiaAttribute, Component_WrapsExactlyOneAttributeSet_RegardlessOfAttributeCount)
{
    AttributeSetComponent comp;
    AttributeSchema schema = MakeDragonSchema(); // 2 attributes
    comp.InitializeFromSchema(schema);

    EXPECT_FLOAT_EQ(comp.GetAttributeSet().GetValue(StringCRC("health")), 1000.0f);
    EXPECT_FLOAT_EQ(comp.GetAttributeSet().GetValue(StringCRC("strength")), 50.0f);
}

TEST(DiaAttribute, Component_AttachedToEntity_RegistersExactlyOneComponentType)
{
    Dia::Entity::Domain domain;
    domain.RegisterPool(new Dia::Entity::ComponentPool<AttributeSetComponent>(AttributeSetComponent::kTypeId));

    Dia::Entity::Entity e = domain.CreateEntity();
    domain.QueueAddComponent<AttributeSetComponent>(e, Json::Value());
    domain.EndOfFrame();

    Dia::Core::Containers::DynamicArrayC<StringCRC, 32> typeIds;
    domain.GetComponentTypeIds(e, typeIds);
    EXPECT_EQ(typeIds.Size(), 1u);

    AttributeSetComponent* comp = domain.GetComponent<AttributeSetComponent>(e);
    ASSERT_NE(comp, nullptr);

    // Attach a multi-attribute schema — component count must stay at exactly one,
    // regardless of how many attributes the schema defines (not one component per attribute).
    AttributeSchema schema = MakeDragonSchema();
    comp->InitializeFromSchema(schema);

    domain.GetComponentTypeIds(e, typeIds);
    EXPECT_EQ(typeIds.Size(), 1u);
}

// Coverage gap: AttributeSetComponent's schema_name FIELD reflection wiring, exercised
// through the real JSON construction path (QueueAddComponent -> the loadFromJson thunk
// registered by DIA_COMPONENT_REGISTER -> DIA_SERIALIZE/DIA_FIELD(schema_name)) — as
// opposed to the existing test above, which passes an EMPTY json and populates the
// AttributeSet directly via InitializeFromSchema.
//
// DISCOVERED GAP (out of scope for this task's two assigned AttributeSet bugs — flagged,
// not fixed here): this does NOT currently populate schema_name. DIA_FIELD(schema_name)
// serializes via Dia::Reflect::JsonReadArchive/JsonWriteArchive's ReadValue/WriteValue
// (DiaCore/Reflect/JsonArchive.h), whose constexpr type dispatch has branches for bool,
// float, double, the integer types, char[N], C-arrays, DynamicArrayC, a "Dia String type"
// (detected via `s.AsCStr()`), and nested Serializable structs — but Dia::Core::StringCRC
// exposes AsChar(), not AsCStr() (see StringCRC.h), so it matches NONE of those branches
// and falls through to "Unknown types: silently ignored". DIA_FIELD_ENTRY's separate
// FieldKind::StringId metadata (ComponentMacros.h) suggests StringCRC field reflection was
// intended to work generically; this test documents that the DIA_SERIALIZE/JsonArchive
// path currently does not, pending a StringCRC branch being added to JsonArchive.h (a
// DiaCore/Reflect change, not an AttributeSet one — needs separate sign-off per the
// escalation rule).
TEST(DiaAttribute, Component_QueuedWithSchemaNameJson_ReflectionCurrentlyDoesNotPopulateStringCRCField)
{
    Dia::Entity::Domain domain;
    domain.RegisterPool(new Dia::Entity::ComponentPool<AttributeSetComponent>(AttributeSetComponent::kTypeId));

    Dia::Entity::Entity e = domain.CreateEntity();

    Json::Value cfg(Json::objectValue);
    cfg["schema_name"] = "dragon";
    domain.QueueAddComponent<AttributeSetComponent>(e, cfg);
    domain.EndOfFrame();

    AttributeSetComponent* comp = domain.GetComponent<AttributeSetComponent>(e);
    ASSERT_NE(comp, nullptr);

    // Documents current (gap) behavior — schema_name stays at its FIELD default rather
    // than being populated from the JSON config. If/when JsonArchive.h gains StringCRC
    // support, this assertion should be updated to EXPECT_TRUE(... == StringCRC("dragon")).
    EXPECT_TRUE(comp->schema_name == Dia::Core::StringCRC());
}

// ===========================================================================
// AC-15 — logging
// ===========================================================================

namespace
{
    class AttributeLogSink : public Dia::Observation::Log::ISink
    {
    public:
        static const unsigned int kMaxEntries = 128;

        AttributeLogSink()
            : mEntryCount(0)
        {
            SetLevelThreshold(Dia::Observation::Log::LogLevel::kDebug);
            SetChannelFilter(Dia::Core::StringCRC("Attribute"), true);
        }

        void OnLogEntry(const Dia::Observation::Log::LogEntry& entry) override
        {
            if (mEntryCount < kMaxEntries)
                mEntries[mEntryCount++] = entry;
        }

        const char* GetName() const override { return "AttributeLogSink"; }

        unsigned int CountByLevel(Dia::Observation::Log::LogLevel level) const
        {
            unsigned int count = 0;
            for (unsigned int i = 0; i < mEntryCount; ++i)
            {
                if (mEntries[i].level == level) ++count;
            }
            return count;
        }

        void Clear() { mEntryCount = 0; }

    private:
        Dia::Observation::Log::LogEntry mEntries[kMaxEntries];
        unsigned int mEntryCount;
    };
}

class DiaAttributeLoggingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mSink.Clear();
        Dia::Observation::Log::Logger::Instance().RegisterThreadBuffer();
        Dia::Observation::Log::Logger::Instance().RegisterSink(&mSink);
    }

    void TearDown() override
    {
        Dia::Observation::Log::Logger::Instance().UnregisterSink(&mSink);
        Dia::Observation::Log::Logger::Instance().UnregisterThreadBuffer();
    }

    void FlushLogs()
    {
        Dia::Observation::Log::Logger::Instance().FlushSync();
    }

    AttributeLogSink mSink;
};

TEST_F(DiaAttributeLoggingTest, SchemaLoad_LogsInfoExactlyOnce)
{
    AttributeSchema schema = MakeDragonSchema();
    (void)schema;
    FlushLogs();
    EXPECT_EQ(mSink.CountByLevel(Dia::Observation::Log::LogLevel::kInfo), 1u);
}

// FromValueSchema returns the failure through SchemaError (callers log); a rejected schema must not log the success line.
TEST_F(DiaAttributeLoggingTest, SchemaLoad_RangeViolation_ReportsErrorAndDoesNotLogSuccess)
{
    AttributeSchema schema;
    SchemaError err;
    EXPECT_FALSE(TryMakeSingleAttributeSchema(0.0f, 100.0f, 500.0f, schema, err));
    FlushLogs();
    EXPECT_EQ(mSink.CountByLevel(Dia::Observation::Log::LogLevel::kInfo), 0u);
    EXPECT_STREQ(err.key.AsCStr(), "health");
    EXPECT_GT(strlen(err.reason.AsCStr()), 0u);
}

TEST_F(DiaAttributeLoggingTest, AddModifier_LogsInfoExactlyOnce)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);
    FlushLogs();
    mSink.Clear();

    set.AddModifier(MakeModifier("buff", "health", ModifierOperation::Add, 10.0f));
    FlushLogs();
    EXPECT_EQ(mSink.CountByLevel(Dia::Observation::Log::LogLevel::kInfo), 1u);
}

TEST_F(DiaAttributeLoggingTest, RemoveModifier_LogsInfoExactlyOnce)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);
    ModifierHandle h = set.AddModifier(MakeModifier("buff", "health", ModifierOperation::Add, 10.0f));
    FlushLogs();
    mSink.Clear();

    set.RemoveModifier(h);
    FlushLogs();
    EXPECT_EQ(mSink.CountByLevel(Dia::Observation::Log::LogLevel::kInfo), 1u);
}

// ===========================================================================
// Coverage gap — modifier-stack-full rejection (non-death companion to
// SLOW_DiaAttribute.AddModifier_StackFull_AssertsInDebug above). Uses the
// swap-g_pAssertFunc precedent (Core/Threading/TestJobSystem.cpp,
// DiaMessageBus/TestDiaMessageBusCore.cpp) to observe both the returned handle
// and the logged warning without crashing on the Debug assert.
// ===========================================================================

namespace
{
    int gStackFullAssertCount = 0;
    void RecordingAssertHandler_StackFull(const char*, const char*, int, const char*, ...)
    {
        ++gStackFullAssertCount;
    }
}

TEST_F(DiaAttributeLoggingTest, AddModifier_StackFull_RejectsAndLogsWarning)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    for (unsigned int i = 0; i < AttributeSet::kMaxModifiersPerAttribute; ++i)
    {
        char name[32];
        snprintf(name, sizeof(name), "buff_%02u", i);
        ModifierHandle h = set.AddModifier(MakeModifier(name, "health", ModifierOperation::Add, 1.0f));
        ASSERT_TRUE(h.IsValid()) << "index " << i;
    }

    auto* prevAssertFunc = Dia::Core::g_pAssertFunc;
    Dia::Core::g_pAssertFunc = RecordingAssertHandler_StackFull;
    gStackFullAssertCount = 0;

    FlushLogs();
    mSink.Clear();

    ModifierHandle overflow = set.AddModifier(MakeModifier("overflow", "health", ModifierOperation::Add, 1.0f));

    FlushLogs();
    Dia::Core::g_pAssertFunc = prevAssertFunc;

    EXPECT_FALSE(overflow.IsValid());
#ifdef DEBUG   // DIA_ASSERT compiles out in Release
    EXPECT_GT(gStackFullAssertCount, 0) << "AddModifier's DIA_ASSERT must fire when the stack is already full";
#endif
    EXPECT_GE(mSink.CountByLevel(Dia::Observation::Log::LogLevel::kWarning), 1u);
    EXPECT_EQ(set.GetModifierCountForAttribute(StringCRC("health")), AttributeSet::kMaxModifiersPerAttribute);
}

// ===========================================================================
// Coverage gap — FromValueSchema ignores entries that are not kind: parameter
// and still loads the parameter entries around them.
// ===========================================================================

TEST_F(DiaAttributeLoggingTest, SchemaLoad_NonParameterEntries_Ignored_OthersStillLoad)
{
    const std::string text = MakeSidecarText({ SidecarMetric("some.metric"), SidecarFloat("health", 0, 2000, 1000), SidecarMetric("other.metric") }, nullptr, "partial_bad");

    FlushLogs();
    mSink.Clear();

    AttributeSchema schema = LoadSchemaOrDie(text.c_str());

    FlushLogs();

    EXPECT_TRUE(schema.IsValid());
    EXPECT_EQ(schema.GetAttributeCount(), 1u); // only the parameter entry loaded

    const AttributeDefinition* health = schema.FindAttribute(StringCRC("health"));
    ASSERT_NE(health, nullptr);
    EXPECT_FLOAT_EQ(health->default_value, 1000.0f);
}

// ===========================================================================
// Bug fix #2 — a self-referencing conditional modifier (a modifier attached to
// attribute X, gated on a when_condition that itself reads attribute X back
// through AttributeAccessorBridge into this same AttributeSet) must not cause
// unbounded recursion. ResolveValue's reentrancy guard breaks the cycle instead.
//
// RED for this bug was NOT established by executing the unguarded code path —
// doing so is a real unbounded stack overflow (undefined, non-deterministic
// crash signature), not something safe to run in an automated test binary, per
// the task's explicit instruction. RED is established by call-chain reasoning:
// ResolveValue -> ConditionExpr::Evaluate -> ConditionRegistry::GetFloat ->
// BridgedAttributeAccessor<Index> -> AttributeSet::GetValueByIndex ->
// ResolveValue (same slot) -> ... recurses with no base case prior to this fix.
//
// This test proves GREEN: with the guard in place, the reentrant call is
// caught at the first recursion (one controlled, recorded assert — not an
// uncontrolled stack overflow) and the outer resolution completes with a
// finite, schema-clamped value instead of hanging or crashing.
// ===========================================================================

namespace
{
    int gReentrancyGuardAssertCount = 0;
    void RecordingAssertHandler_ReentrancyGuard(const char*, const char*, int, const char*, ...)
    {
        ++gReentrancyGuardAssertCount;
    }
}

TEST_F(DiaAttributeLoggingTest, ResolveValue_SelfReferencingConditionalModifier_ReentrancyGuardBreaksCycle)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    // PRECONDITION (see AttributeAccessorBridge.h): registry's data pointer IS the very
    // AttributeSet being bridged.
    Dia::Condition::ConditionRegistry registry(&set);
    AttributeAccessorBridge::RegisterAccessors(registry, set, StringCRC("actor"));
    set.SetConditionRegistry(&registry);

    // Gated on the SAME attribute ("health") the modifier is attached to — the
    // self-reference that used to recurse without bound.
    const char* whenCondition = R"({"op":">=","slot":"actor","field":"health","value":500})";

    auto* prevAssertFunc = Dia::Core::g_pAssertFunc;
    Dia::Core::g_pAssertFunc = RecordingAssertHandler_ReentrancyGuard;
    gReentrancyGuardAssertCount = 0;

    FlushLogs();
    mSink.Clear();

    // AddModifier's own post-add ResolveValue call ("after" snapshot, taken once the new
    // conditional modifier is already in the slot's array) is what first triggers the cycle.
    ModifierHandle h = set.AddModifier(
        MakeConditionalModifier("self_ref", "health", ModifierOperation::Add, 10.0f, whenCondition));

    const float value = set.GetValue(StringCRC("health")); // triggers the cycle again, independently

    FlushLogs();
    Dia::Core::g_pAssertFunc = prevAssertFunc;

    EXPECT_TRUE(h.IsValid());
#ifdef DEBUG   // DIA_ASSERT compiles out in Release
    EXPECT_GT(gReentrancyGuardAssertCount, 0) << "reentrancy guard must assert when the cycle is detected";
#endif
    EXPECT_GE(mSink.CountByLevel(Dia::Observation::Log::LogLevel::kWarning), 1u);

    // No hang, no crash, and the returned value is finite and within the schema's clamp
    // range — a "sane" value, not proof of any particular arithmetic result (the inner
    // guarded call's base_value fallback feeds back into the very condition being
    // evaluated, which is an inherently ambiguous case; "does not hang/crash and stays
    // in range" is the actual contract this fix provides).
    EXPECT_TRUE(std::isfinite(value));
    EXPECT_GE(value, set.GetMinimumValueByIndex(0));
    EXPECT_LE(value, set.GetMaximumValueByIndex(0));
}
