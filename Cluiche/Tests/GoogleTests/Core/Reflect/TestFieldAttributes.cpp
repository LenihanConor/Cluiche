#include <gtest/gtest.h>
#include "DiaCore/Reflect/Reflect.h"
#include "DiaCore/CRC/StringCRC.h"

using namespace Dia::Reflect;
using namespace Dia::Core;

// =============================================================================
// Test types
// =============================================================================

struct Mass {
    float mValue  = 1.0f;
    int   mTextureId = 0;
};

DIA_SERIALIZE(Mass, 1)
    DIA_FIELD(mValue)
    DIA_FIELD(mTextureId)
DIA_SERIALIZE_END

// Register field attributes for Mass
DIA_ATTR_REQUIRED(Mass, mValue)
DIA_ATTR_RANGE(Mass, mValue, 0.0f, 1000.0f)
DIA_ATTR_ASSET_REF(Mass, mTextureId, DiaTexture)

// Second struct with the same field name — should be independent in the registry
struct Velocity {
    float mValue = 0.0f;
};

DIA_SERIALIZE(Velocity, 1)
    DIA_FIELD(mValue)
DIA_SERIALIZE_END

// Velocity.mValue only gets Required (no Range)
DIA_ATTR_REQUIRED(Velocity, mValue)

// =============================================================================
// Helpers
// =============================================================================

static uint32_t TypeCrc(const char* name)  { return StringCRC(name).Value(); }
static uint32_t FieldCrc(const char* name) { return StringCRC(name).Value(); }

// =============================================================================
// Tests
// =============================================================================

// A: GetAttributes returns correct count for Mass.mValue (Required + Range = 2)
TEST(FieldAttributeRegistry, GetAttributesCountForMassValue) {
    const FieldAttribute* attrs[8];
    unsigned int count = FieldAttributeRegistry::Instance().GetAttributes(
        TypeCrc("Mass"), FieldCrc("mValue"), attrs, 8);
    EXPECT_EQ(count, 2u);
}

// B: FindAttribute<RequiredAttribute> returns non-null for Mass.mValue
TEST(FieldAttributeRegistry, FindRequiredOnMassValue) {
    const RequiredAttribute* attr = FieldAttributeRegistry::Instance().FindAttribute<RequiredAttribute>(
        TypeCrc("Mass"), FieldCrc("mValue"));
    ASSERT_NE(attr, nullptr);
    EXPECT_STREQ(attr->GetKind(), "Required");
}

// C: FindAttribute<RangeAttribute<float>> returns non-null for Mass.mValue, correct min/max
TEST(FieldAttributeRegistry, FindRangeOnMassValue) {
    const RangeAttribute<float>* attr = FieldAttributeRegistry::Instance().FindAttribute<RangeAttribute<float>>(
        TypeCrc("Mass"), FieldCrc("mValue"));
    ASSERT_NE(attr, nullptr);
    EXPECT_FLOAT_EQ(attr->minValue, 0.0f);
    EXPECT_FLOAT_EQ(attr->maxValue, 1000.0f);
    EXPECT_STREQ(attr->GetKind(), "Range");
}

// D: FindAttribute<AssetRefAttribute> returns non-null for Mass.mTextureId, targetTypeId matches
TEST(FieldAttributeRegistry, FindAssetRefOnMassTextureId) {
    const AssetRefAttribute* attr = FieldAttributeRegistry::Instance().FindAttribute<AssetRefAttribute>(
        TypeCrc("Mass"), FieldCrc("mTextureId"));
    ASSERT_NE(attr, nullptr);
    EXPECT_EQ(attr->targetTypeId.Value(), StringCRC("DiaTexture").Value());
    EXPECT_STREQ(attr->GetKind(), "AssetRef");
}

// E: FindAttribute returns nullptr for an unregistered (type, field) pair
TEST(FieldAttributeRegistry, FindAttributeUnregisteredReturnsNull) {
    const RequiredAttribute* attr = FieldAttributeRegistry::Instance().FindAttribute<RequiredAttribute>(
        TypeCrc("NonExistentType"), FieldCrc("someField"));
    EXPECT_EQ(attr, nullptr);
}

// F: FindAttribute returns nullptr when asking for the wrong attribute type
//    Mass.mTextureId only has AssetRef; looking for Required should return null
TEST(FieldAttributeRegistry, FindWrongAttributeTypeReturnsNull) {
    const RequiredAttribute* attr = FieldAttributeRegistry::Instance().FindAttribute<RequiredAttribute>(
        TypeCrc("Mass"), FieldCrc("mTextureId"));
    EXPECT_EQ(attr, nullptr);
}

// G: Same type+field can have multiple attributes (Mass.mValue has both Required and Range)
TEST(FieldAttributeRegistry, MultipleAttributesOnSameField) {
    const RequiredAttribute*    req   = FieldAttributeRegistry::Instance().FindAttribute<RequiredAttribute>(
        TypeCrc("Mass"), FieldCrc("mValue"));
    const RangeAttribute<float>* range = FieldAttributeRegistry::Instance().FindAttribute<RangeAttribute<float>>(
        TypeCrc("Mass"), FieldCrc("mValue"));

    EXPECT_NE(req,   nullptr);
    EXPECT_NE(range, nullptr);
}

// H: Different types with the same field name are independent in the registry
//    Mass.mValue has Required+Range (count=2); Velocity.mValue has only Required (count=1)
TEST(FieldAttributeRegistry, SameFieldNameDifferentTypesAreIndependent) {
    const FieldAttribute* attrsM[8];
    unsigned int countM = FieldAttributeRegistry::Instance().GetAttributes(
        TypeCrc("Mass"), FieldCrc("mValue"), attrsM, 8);

    const FieldAttribute* attrsV[8];
    unsigned int countV = FieldAttributeRegistry::Instance().GetAttributes(
        TypeCrc("Velocity"), FieldCrc("mValue"), attrsV, 8);

    // Mass.mValue: Required + Range
    EXPECT_EQ(countM, 2u);
    // Velocity.mValue: Required only
    EXPECT_EQ(countV, 1u);
}

// I: RangeAttribute.clamp defaults to false
TEST(FieldAttributeRegistry, RangeAttributeClampDefaultsFalse) {
    const RangeAttribute<float>* attr = FieldAttributeRegistry::Instance().FindAttribute<RangeAttribute<float>>(
        TypeCrc("Mass"), FieldCrc("mValue"));
    ASSERT_NE(attr, nullptr);
    EXPECT_FALSE(attr->clamp);
}

// J: DIA_ATTR_RANGE macro produces RangeAttribute with correct min/max
TEST(FieldAttributeRegistry, DiaAttrRangeProducesCorrectMinMax) {
    const RangeAttribute<float>* attr = FieldAttributeRegistry::Instance().FindAttribute<RangeAttribute<float>>(
        TypeCrc("Mass"), FieldCrc("mValue"));
    ASSERT_NE(attr, nullptr);
    EXPECT_FLOAT_EQ(attr->minValue, 0.0f);
    EXPECT_FLOAT_EQ(attr->maxValue, 1000.0f);
}

// K: All three attribute kinds are registered before any test runs (static init order)
//    Verified by running a query that touches all three
TEST(FieldAttributeRegistry, AllAttributeKindsRegisteredAtStaticInit) {
    const RequiredAttribute*    req   = FieldAttributeRegistry::Instance().FindAttribute<RequiredAttribute>(
        TypeCrc("Mass"), FieldCrc("mValue"));
    const RangeAttribute<float>* range = FieldAttributeRegistry::Instance().FindAttribute<RangeAttribute<float>>(
        TypeCrc("Mass"), FieldCrc("mValue"));
    const AssetRefAttribute*    aref  = FieldAttributeRegistry::Instance().FindAttribute<AssetRefAttribute>(
        TypeCrc("Mass"), FieldCrc("mTextureId"));

    EXPECT_NE(req,   nullptr);
    EXPECT_NE(range, nullptr);
    EXPECT_NE(aref,  nullptr);
}

// L: FieldAttributeRegistry has room for at least 256 entries
TEST(FieldAttributeRegistry, RegistryCapacityAtLeast256) {
    EXPECT_GE(FieldAttributeRegistry::kMaxEntries, 256u);
}

// =============================================================================
// RangeAttribute enforcement tests
// =============================================================================

struct ClampedValue {
    float mSpeed = 50.0f;
};

DIA_SERIALIZE(ClampedValue, 1)
    DIA_FIELD_RANGED(mSpeed)
DIA_SERIALIZE_END

DIA_ATTR_RANGE_CLAMPED(ClampedValue, mSpeed, 0.0f, 100.0f)

struct ErrorOnRange {
    int mLevel = 5;
};

DIA_SERIALIZE(ErrorOnRange, 1)
    DIA_FIELD_RANGED(mLevel)
DIA_SERIALIZE_END

DIA_ATTR_RANGE(ErrorOnRange, mLevel, 1, 10)

TEST(RangeEnforcement, ValueAboveMax_Clamped) {
    ClampedValue src;
    src.mSpeed = 200.0f;

    JsonWriteArchive wAr;
    serialize(wAr, src, 1u);

    ClampedValue dst;
    JsonReadArchive rAr(wAr.GetRoot());
    serialize(rAr, dst, 1u);

    EXPECT_FLOAT_EQ(dst.mSpeed, 100.0f);
    EXPECT_TRUE(rAr.GetResult().IsOk());
}

TEST(RangeEnforcement, ValueBelowMin_Clamped) {
    ClampedValue src;
    src.mSpeed = -50.0f;

    JsonWriteArchive wAr;
    serialize(wAr, src, 1u);

    ClampedValue dst;
    JsonReadArchive rAr(wAr.GetRoot());
    serialize(rAr, dst, 1u);

    EXPECT_FLOAT_EQ(dst.mSpeed, 0.0f);
    EXPECT_TRUE(rAr.GetResult().IsOk());
}

TEST(RangeEnforcement, ValueInRange_Unchanged) {
    ClampedValue src;
    src.mSpeed = 50.0f;

    JsonWriteArchive wAr;
    serialize(wAr, src, 1u);

    ClampedValue dst;
    JsonReadArchive rAr(wAr.GetRoot());
    serialize(rAr, dst, 1u);

    EXPECT_FLOAT_EQ(dst.mSpeed, 50.0f);
    EXPECT_TRUE(rAr.GetResult().IsOk());
}

TEST(RangeEnforcement, ValueOutOfRange_ErrorMode_ReportsViolation) {
    ErrorOnRange src;
    src.mLevel = 20;

    JsonWriteArchive wAr;
    serialize(wAr, src, 1u);

    ErrorOnRange dst;
    JsonReadArchive rAr(wAr.GetRoot());
    serialize(rAr, dst, 1u);

    EXPECT_TRUE(rAr.GetResult().HasErrors());
    EXPECT_EQ(rAr.GetResult().GetError(0).kind, SerializeErrorKind::RangeViolation);
    EXPECT_EQ(dst.mLevel, 20); // not clamped — just reported
}

TEST(RangeEnforcement, ValueInRange_ErrorMode_NoError) {
    ErrorOnRange src;
    src.mLevel = 5;

    JsonWriteArchive wAr;
    serialize(wAr, src, 1u);

    ErrorOnRange dst;
    JsonReadArchive rAr(wAr.GetRoot());
    serialize(rAr, dst, 1u);

    EXPECT_TRUE(rAr.GetResult().IsOk());
    EXPECT_EQ(dst.mLevel, 5);
}

// =============================================================================
// DescribeAttribute + generic GetRange tests
// =============================================================================

struct Described {
    float mWeight = 1.0f;
    float mDrag   = 0.0f;
    int   mCount  = 0;
    double mScale = 1.0;
};

DIA_ATTR_DESCRIBE(Described, mWeight, "Weight", "kg", "How heavy the body is.")
DIA_ATTR_RANGE(Described, mWeight, 0.0f, 10.0f)
DIA_ATTR_REQUIRED(Described, mWeight)
DIA_ATTR_DESCRIBE_HINT(Described, mDrag, "Drag", "", "Air resistance.", "Above 1 the body stalls.")
DIA_ATTR_RANGE(Described, mCount, -5, 5)
DIA_ATTR_RANGE(Described, mScale, 0.5, 2.5)

TEST(DescribeAttribute, DescribeRegisteredAlongsideRangeAndRequired) {
    auto& reg = FieldAttributeRegistry::Instance();
    const DescribeAttribute* d = reg.FindAttribute<DescribeAttribute>(TypeCrc("Described"), FieldCrc("mWeight"));
    ASSERT_NE(d, nullptr);
    EXPECT_STREQ(d->GetKind(), "Describe");
    EXPECT_STREQ(d->label, "Weight");
    EXPECT_STREQ(d->unit, "kg");
    EXPECT_STREQ(d->description, "How heavy the body is.");
    EXPECT_EQ(d->hint, nullptr);
    EXPECT_NE(reg.FindAttribute<RangeAttribute<float>>(TypeCrc("Described"), FieldCrc("mWeight")), nullptr);
    EXPECT_NE(reg.FindAttribute<RequiredAttribute>(TypeCrc("Described"), FieldCrc("mWeight")), nullptr);
}

TEST(DescribeAttribute, DescribeHintSetsHint) {
    const DescribeAttribute* d = FieldAttributeRegistry::Instance().FindAttribute<DescribeAttribute>(
        TypeCrc("Described"), FieldCrc("mDrag"));
    ASSERT_NE(d, nullptr);
    EXPECT_STREQ(d->label, "Drag");
    EXPECT_STREQ(d->unit, "");
    EXPECT_STREQ(d->description, "Air resistance.");
    EXPECT_STREQ(d->hint, "Above 1 the body stalls.");
}

TEST(FieldAttributeGetRange, ReturnsRangeForArithmeticTypes) {
    auto& reg = FieldAttributeRegistry::Instance();
    double lo = 0.0, hi = 0.0;

    const FieldAttribute* f = reg.FindAttribute<RangeAttribute<float>>(TypeCrc("Described"), FieldCrc("mWeight"));
    ASSERT_NE(f, nullptr);
    EXPECT_TRUE(f->GetRange(lo, hi));
    EXPECT_DOUBLE_EQ(lo, 0.0);
    EXPECT_DOUBLE_EQ(hi, 10.0);

    const FieldAttribute* i = reg.FindAttribute<RangeAttribute<int>>(TypeCrc("Described"), FieldCrc("mCount"));
    ASSERT_NE(i, nullptr);
    EXPECT_TRUE(i->GetRange(lo, hi));
    EXPECT_DOUBLE_EQ(lo, -5.0);
    EXPECT_DOUBLE_EQ(hi, 5.0);

    const FieldAttribute* d = reg.FindAttribute<RangeAttribute<double>>(TypeCrc("Described"), FieldCrc("mScale"));
    ASSERT_NE(d, nullptr);
    EXPECT_TRUE(d->GetRange(lo, hi));
    EXPECT_DOUBLE_EQ(lo, 0.5);
    EXPECT_DOUBLE_EQ(hi, 2.5);
}

TEST(FieldAttributeGetRange, ReturnsFalseForNonRangeAttributes) {
    double lo = 7.0, hi = 8.0;
    RequiredAttribute req;
    DescribeAttribute desc;
    AssetRefAttribute aref("DiaTexture");
    EXPECT_FALSE(req.GetRange(lo, hi));
    EXPECT_FALSE(desc.GetRange(lo, hi));
    EXPECT_FALSE(aref.GetRange(lo, hi));
    EXPECT_DOUBLE_EQ(lo, 7.0);
    EXPECT_DOUBLE_EQ(hi, 8.0);
}
