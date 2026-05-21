#include <gtest/gtest.h>
#include "DiaCore/Reflect/Archive.h"
#include "DiaCore/Reflect/SerializeResult.h"

using namespace Dia::Reflect;

// ---------------------------------------------------------
// Mock archives for Archive concept tests
// ---------------------------------------------------------
struct MockWriteArchive {
    bool IsReading() const { return false; }
    bool IsWriting() const { return true; }
};

struct MockReadArchive {
    bool IsReading() const { return true; }
    bool IsWriting() const { return false; }
};

struct BadArchive {}; // missing IsReading/IsWriting

static_assert(Archive<MockWriteArchive>, "MockWriteArchive must satisfy Archive");
static_assert(Archive<MockReadArchive>,  "MockReadArchive must satisfy Archive");
static_assert(!Archive<BadArchive>,      "BadArchive must not satisfy Archive");

// ---------------------------------------------------------
// Archive concept
// ---------------------------------------------------------

TEST(ArchiveConcept, MockWriteArchiveSatisfiesConcept) {
    // Compile-time check is in the static_assert above.
    // Runtime: confirm IsReading/IsWriting return the expected values.
    MockWriteArchive ar;
    EXPECT_FALSE(ar.IsReading());
    EXPECT_TRUE(ar.IsWriting());
}

TEST(ArchiveConcept, MockReadArchiveSatisfiesConcept) {
    MockReadArchive ar;
    EXPECT_TRUE(ar.IsReading());
    EXPECT_FALSE(ar.IsWriting());
}

// ---------------------------------------------------------
// NamedField
// ---------------------------------------------------------

TEST(NamedField, ConstructionViaHelperStoresNameCRC) {
    int value = 42;
    auto field = named("health", value);
    EXPECT_EQ(field.name, Dia::Core::StringCRC("health"));
}

TEST(NamedField, ConstructionViaHelperStoresValueRef) {
    int value = 99;
    auto field = named("score", value);
    EXPECT_EQ(field.value, 99);

    value = 7;
    EXPECT_EQ(field.value, 7); // confirms it is a reference, not a copy
}

TEST(NamedField, NotRequiredByDefault) {
    float f = 1.0f;
    auto field = named("speed", f);
    EXPECT_FALSE(field.required);
}

TEST(NamedField, RequiredSetsFlag) {
    double d = 3.14;
    auto field = named("pi", d);
    field.Required();
    EXPECT_TRUE(field.required);
}

TEST(NamedField, RequiredIsChainable) {
    bool b = true;
    auto field = named("flag", b);
    // Required() must return a reference to the same NamedField
    auto& ref = field.Required();
    EXPECT_TRUE(ref.required);
    EXPECT_EQ(&ref, &field);
}

TEST(NamedField, DeducesIntType) {
    int v = 0;
    NamedField<int> field = named("x", v);
    EXPECT_EQ(field.name, Dia::Core::StringCRC("x"));
}

TEST(NamedField, DeducesFloatType) {
    float v = 0.0f;
    NamedField<float> field = named("y", v);
    EXPECT_EQ(field.name, Dia::Core::StringCRC("y"));
}

TEST(NamedField, DeducesBoolType) {
    bool v = false;
    NamedField<bool> field = named("active", v);
    EXPECT_EQ(field.name, Dia::Core::StringCRC("active"));
}

TEST(NamedField, DeducesDoubleType) {
    double v = 0.0;
    NamedField<double> field = named("ratio", v);
    EXPECT_EQ(field.name, Dia::Core::StringCRC("ratio"));
}

TEST(NamedField, FieldNameCRCMatchesStringCRC) {
    int v = 0;
    const char* fieldName = "velocity";
    auto field = named(fieldName, v);
    Dia::Core::StringCRC expected(fieldName);
    EXPECT_EQ(field.name, expected);
}

// ---------------------------------------------------------
// OwnedPtrField
// ---------------------------------------------------------

TEST(OwnedPtrField, ConstructionViaHelperStoresNameCRC) {
    int* ptr = nullptr;
    auto field = owned("child", ptr);
    EXPECT_EQ(field.name, Dia::Core::StringCRC("child"));
}

TEST(OwnedPtrField, ConstructionViaHelperStoresPtrRef) {
    int value = 10;
    int* ptr = &value;
    auto field = owned("node", ptr);
    EXPECT_EQ(field.ptr, &value);

    int other = 20;
    ptr = &other;
    EXPECT_EQ(field.ptr, &other); // confirms it is a reference to the pointer
}

TEST(OwnedPtrField, FieldNameCRCMatchesStringCRC) {
    int* ptr = nullptr;
    const char* fieldName = "inventory";
    auto field = owned(fieldName, ptr);
    Dia::Core::StringCRC expected(fieldName);
    EXPECT_EQ(field.name, expected);
}

// ---------------------------------------------------------
// RefIdField
// ---------------------------------------------------------

TEST(RefIdField, ConstructionViaHelperStoresNameCRC) {
    int id = 0;
    auto field = refId("enemyId", id);
    EXPECT_EQ(field.name, Dia::Core::StringCRC("enemyId"));
}

TEST(RefIdField, ConstructionViaHelperStoresIdRef) {
    int id = 5;
    auto field = refId("targetId", id);
    EXPECT_EQ(field.id, 5);

    id = 99;
    EXPECT_EQ(field.id, 99); // confirms reference semantics
}

TEST(RefIdField, FieldNameCRCMatchesStringCRC) {
    unsigned int id = 0;
    const char* fieldName = "spawnPoint";
    auto field = refId(fieldName, id);
    Dia::Core::StringCRC expected(fieldName);
    EXPECT_EQ(field.name, expected);
}

// ---------------------------------------------------------
// SerializeResult — default state
// ---------------------------------------------------------

TEST(SerializeResult, DefaultConstructedIsOk) {
    SerializeResult result;
    EXPECT_TRUE(result.IsOk());
}

TEST(SerializeResult, DefaultConstructedHasNoErrors) {
    SerializeResult result;
    EXPECT_FALSE(result.HasErrors());
}

TEST(SerializeResult, DefaultConstructedErrorCountIsZero) {
    SerializeResult result;
    EXPECT_EQ(result.ErrorCount(), 0u);
}

// ---------------------------------------------------------
// SerializeResult — AddError
// ---------------------------------------------------------

TEST(SerializeResult, AddErrorMakesHasErrorsTrue) {
    SerializeResult result;
    result.AddError(SerializeErrorKind::TypeMismatch, Dia::Core::StringCRC("health"), "bad type");
    EXPECT_TRUE(result.HasErrors());
    EXPECT_FALSE(result.IsOk());
}

TEST(SerializeResult, AddErrorStoresKind) {
    SerializeResult result;
    result.AddError(SerializeErrorKind::RequiredFieldMissing, Dia::Core::StringCRC("name"), "missing");
    EXPECT_EQ(result.GetError(0).kind, SerializeErrorKind::RequiredFieldMissing);
}

TEST(SerializeResult, AddErrorStoresFieldName) {
    SerializeResult result;
    Dia::Core::StringCRC expectedName("damage");
    result.AddError(SerializeErrorKind::TypeMismatch, expectedName, "mismatch");
    EXPECT_EQ(result.GetError(0).fieldName, expectedName);
}

TEST(SerializeResult, AddErrorStoresMessage) {
    SerializeResult result;
    result.AddError(SerializeErrorKind::CapacityExceeded, Dia::Core::StringCRC("items"), "array full");
    // Verify message is stored (compare as C-string via AsCStr())
    EXPECT_STREQ(result.GetError(0).message.AsCStr(), "array full");
}

TEST(SerializeResult, ErrorCountIncrementsOnEachAdd) {
    SerializeResult result;
    EXPECT_EQ(result.ErrorCount(), 0u);

    result.AddError(SerializeErrorKind::TypeMismatch, Dia::Core::StringCRC("a"), "e1");
    EXPECT_EQ(result.ErrorCount(), 1u);

    result.AddError(SerializeErrorKind::RequiredFieldMissing, Dia::Core::StringCRC("b"), "e2");
    EXPECT_EQ(result.ErrorCount(), 2u);

    result.AddError(SerializeErrorKind::CapacityExceeded, Dia::Core::StringCRC("c"), "e3");
    EXPECT_EQ(result.ErrorCount(), 3u);
}

TEST(SerializeResult, AddErrorWhenFullDoesNotCrash) {
    SerializeResult result;
    // Fill to capacity
    for (unsigned int i = 0; i < SerializeResult::kMaxErrors; ++i) {
        result.AddError(SerializeErrorKind::TypeMismatch, Dia::Core::StringCRC("field"), "overflow test");
    }
    EXPECT_EQ(result.ErrorCount(), SerializeResult::kMaxErrors);

    // One more — must not crash and must not exceed capacity
    result.AddError(SerializeErrorKind::TypeMismatch, Dia::Core::StringCRC("extra"), "dropped");
    EXPECT_EQ(result.ErrorCount(), SerializeResult::kMaxErrors);
}

// ---------------------------------------------------------
// SerializeResult — Clear
// ---------------------------------------------------------

TEST(SerializeResult, ClearResetsToOk) {
    SerializeResult result;
    result.AddError(SerializeErrorKind::TypeMismatch, Dia::Core::StringCRC("x"), "err");
    ASSERT_TRUE(result.HasErrors());

    result.Clear();

    EXPECT_TRUE(result.IsOk());
    EXPECT_FALSE(result.HasErrors());
    EXPECT_EQ(result.ErrorCount(), 0u);
}

TEST(SerializeResult, IsOkAndHasErrorsToggleCorrectly) {
    SerializeResult result;

    EXPECT_TRUE(result.IsOk());
    EXPECT_FALSE(result.HasErrors());

    result.AddError(SerializeErrorKind::UnknownPolymorphicType, Dia::Core::StringCRC("type"), "unknown");

    EXPECT_FALSE(result.IsOk());
    EXPECT_TRUE(result.HasErrors());

    result.Clear();

    EXPECT_TRUE(result.IsOk());
    EXPECT_FALSE(result.HasErrors());
}
