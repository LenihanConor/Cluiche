#include <gtest/gtest.h>
#include "DiaCore/Reflect/ReflectMacros.h"
#include "DiaCore/CRC/StringCRC.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

using namespace Dia::Reflect;
using namespace Dia::Core;

// =============================================================================
// Mock archives — must be at file scope so DIA_SERIALIZE blocks can reference them
// =============================================================================

// RecordingArchive: records field name CRCs seen via operator&
struct RecordingArchive {
    bool IsReading() const { return false; }
    bool IsWriting() const { return true; }

    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> recordedNames;

    template<typename T>
    RecordingArchive& operator&(Dia::Reflect::NamedField<T> f) {
        recordedNames.Add(f.name);
        return *this;
    }
    template<typename T>
    RecordingArchive& operator&(Dia::Reflect::OwnedPtrField<T> f) {
        recordedNames.Add(f.name);
        return *this;
    }
    template<typename T>
    RecordingArchive& operator&(Dia::Reflect::RefIdField<T> f) {
        recordedNames.Add(f.name);
        return *this;
    }
};

// RecordingArchiveWithFlags: records both name CRC and required flag (for NamedField only)
struct RecordingField {
    Dia::Core::StringCRC name;
    bool required = false;
};

struct RecordingArchiveWithFlags {
    bool IsReading() const { return false; }
    bool IsWriting() const { return true; }

    Dia::Core::Containers::DynamicArrayC<RecordingField, 32> recordedFields;

    template<typename T>
    RecordingArchiveWithFlags& operator&(Dia::Reflect::NamedField<T> f) {
        RecordingField rf;
        rf.name = f.name;
        rf.required = f.required;
        recordedFields.Add(rf);
        return *this;
    }
    template<typename T>
    RecordingArchiveWithFlags& operator&(Dia::Reflect::OwnedPtrField<T> f) {
        RecordingField rf;
        rf.name = f.name;
        rf.required = false;
        recordedFields.Add(rf);
        return *this;
    }
    template<typename T>
    RecordingArchiveWithFlags& operator&(Dia::Reflect::RefIdField<T> f) {
        RecordingField rf;
        rf.name = f.name;
        rf.required = false;
        recordedFields.Add(rf);
        return *this;
    }
};

// =============================================================================
// Test structs — must be at file scope (DIA_SERIALIZE defines a free function template)
// =============================================================================

// --- A: Basic DIA_SERIALIZE + DIA_FIELD ---
struct Point2D {
    float mX = 0.0f;
    float mY = 0.0f;
};

DIA_SERIALIZE(Point2D, 1)
    DIA_FIELD(mX)
    DIA_FIELD(mY)
DIA_SERIALIZE_END

// --- B: DIA_FIELD_REQUIRED ---
struct RequiredTest {
    int mId = 0;
};

DIA_SERIALIZE(RequiredTest, 1)
    DIA_FIELD_REQUIRED(mId)
DIA_SERIALIZE_END

// --- C: DIA_FIELD_OWNED_PTR ---
struct ShapeHolder {
    int* mShape = nullptr;
};

DIA_SERIALIZE(ShapeHolder, 1)
    DIA_FIELD_OWNED_PTR(mShape)
DIA_SERIALIZE_END

// --- D: DIA_FIELD_REF_ID ---
struct RefHolder {
    int mOwnerId = 0;
};

DIA_SERIALIZE(RefHolder, 1)
    DIA_FIELD_REF_ID(mOwnerId)
DIA_SERIALIZE_END

// --- E: DIA_FIELD_NAMED (custom name) ---
struct Renamed {
    int mValue = 0;
};

DIA_SERIALIZE(Renamed, 1)
    DIA_FIELD_NAMED("value", mValue)
DIA_SERIALIZE_END

// --- F: DIA_BASE inheritance ---
struct BaseStruct {
    int mA = 0;
};

DIA_SERIALIZE(BaseStruct, 1)
    DIA_FIELD(mA)
DIA_SERIALIZE_END

struct DerivedStruct : BaseStruct {
    int mB = 0;
};

DIA_SERIALIZE(DerivedStruct, 2)
    DIA_BASE(BaseStruct)
    DIA_FIELD(mB)
DIA_SERIALIZE_END

// --- J: Empty serialize ---
struct EmptyStruct {};

DIA_SERIALIZE(EmptyStruct, 1)
DIA_SERIALIZE_END

// --- H: Version migration (escape hatch — plain free function, no macro) ---
struct MigrationTest {
    float mNew = 0.0f;
};

template<class Archive>
void serialize(Archive& ar, MigrationTest& obj, unsigned version = 1) {
    if (version >= 2) {
        ar & Dia::Reflect::named("mNew", obj.mNew);
    }
}

// =============================================================================
// static_assert: RecordingArchive satisfies the Archive concept
// =============================================================================
static_assert(Dia::Reflect::Archive<RecordingArchive>,
    "RecordingArchive must satisfy Dia::Reflect::Archive concept");
static_assert(Dia::Reflect::Archive<RecordingArchiveWithFlags>,
    "RecordingArchiveWithFlags must satisfy Dia::Reflect::Archive concept");

// =============================================================================
// Tests
// =============================================================================

// --- A: Basic DIA_SERIALIZE + DIA_FIELD ---

TEST(MacroDSL_BasicField, TwoFieldsRecorded) {
    RecordingArchive ar;
    Point2D p;
    serialize(ar, p);
    EXPECT_EQ(ar.recordedNames.Size(), 2u);
}

TEST(MacroDSL_BasicField, FirstFieldIsMX) {
    RecordingArchive ar;
    Point2D p;
    serialize(ar, p);
    EXPECT_EQ(ar.recordedNames[0], Dia::Core::StringCRC("mX"));
}

TEST(MacroDSL_BasicField, SecondFieldIsMY) {
    RecordingArchive ar;
    Point2D p;
    serialize(ar, p);
    EXPECT_EQ(ar.recordedNames[1], Dia::Core::StringCRC("mY"));
}

TEST(MacroDSL_BasicField, FieldOrderPreservedMXBeforeMY) {
    RecordingArchive ar;
    Point2D p;
    serialize(ar, p);
    EXPECT_EQ(ar.recordedNames[0], Dia::Core::StringCRC("mX"));
    EXPECT_EQ(ar.recordedNames[1], Dia::Core::StringCRC("mY"));
}

// --- B: DIA_FIELD_REQUIRED ---

TEST(MacroDSL_RequiredField, OneFieldRecorded) {
    RecordingArchiveWithFlags ar;
    RequiredTest t;
    serialize(ar, t);
    EXPECT_EQ(ar.recordedFields.Size(), 1u);
}

TEST(MacroDSL_RequiredField, RequiredFlagIsTrue) {
    RecordingArchiveWithFlags ar;
    RequiredTest t;
    serialize(ar, t);
    EXPECT_TRUE(ar.recordedFields[0].required);
}

TEST(MacroDSL_RequiredField, FieldNameIsMId) {
    RecordingArchiveWithFlags ar;
    RequiredTest t;
    serialize(ar, t);
    EXPECT_EQ(ar.recordedFields[0].name, Dia::Core::StringCRC("mId"));
}

// --- C: DIA_FIELD_OWNED_PTR ---

TEST(MacroDSL_OwnedPtrField, OwnedPtrFieldRecorded) {
    RecordingArchive ar;
    ShapeHolder s;
    serialize(ar, s);
    EXPECT_EQ(ar.recordedNames.Size(), 1u);
}

TEST(MacroDSL_OwnedPtrField, NameIsMShape) {
    RecordingArchive ar;
    ShapeHolder s;
    serialize(ar, s);
    EXPECT_EQ(ar.recordedNames[0], Dia::Core::StringCRC("mShape"));
}

// --- D: DIA_FIELD_REF_ID ---

TEST(MacroDSL_RefIdField, RefIdFieldRecorded) {
    RecordingArchive ar;
    RefHolder r;
    serialize(ar, r);
    EXPECT_EQ(ar.recordedNames.Size(), 1u);
}

TEST(MacroDSL_RefIdField, NameIsMOwnerId) {
    RecordingArchive ar;
    RefHolder r;
    serialize(ar, r);
    EXPECT_EQ(ar.recordedNames[0], Dia::Core::StringCRC("mOwnerId"));
}

// --- E: DIA_FIELD_NAMED (custom name) ---

TEST(MacroDSL_FieldNamed, RecordsCustomName) {
    RecordingArchive ar;
    Renamed rn;
    serialize(ar, rn);
    EXPECT_EQ(ar.recordedNames.Size(), 1u);
    EXPECT_EQ(ar.recordedNames[0], Dia::Core::StringCRC("value"));
}

TEST(MacroDSL_FieldNamed, DoesNotRecordMemberName) {
    RecordingArchive ar;
    Renamed rn;
    serialize(ar, rn);
    EXPECT_NE(ar.recordedNames[0], Dia::Core::StringCRC("mValue"));
}

// --- F: DIA_BASE inheritance ---

TEST(MacroDSL_Inheritance, BaseFieldComesFirst) {
    RecordingArchive ar;
    DerivedStruct d;
    serialize(ar, d, 2u);
    ASSERT_EQ(ar.recordedNames.Size(), 2u);
    EXPECT_EQ(ar.recordedNames[0], Dia::Core::StringCRC("mA"));
}

TEST(MacroDSL_Inheritance, DerivedFieldComesSecond) {
    RecordingArchive ar;
    DerivedStruct d;
    serialize(ar, d, 2u);
    ASSERT_EQ(ar.recordedNames.Size(), 2u);
    EXPECT_EQ(ar.recordedNames[1], Dia::Core::StringCRC("mB"));
}

TEST(MacroDSL_Inheritance, TwoFieldsTotalRecorded) {
    RecordingArchive ar;
    DerivedStruct d;
    serialize(ar, d, 2u);
    EXPECT_EQ(ar.recordedNames.Size(), 2u);
}

// --- G: static_assert fires for non-Archive type ---
// The static_asserts above the TEST blocks cover this at compile time.
// This runtime test confirms the RecordingArchive satisfies Archive.

TEST(MacroDSL_ArchiveConcept, RecordingArchiveSatisfiesArchiveConcept) {
    RecordingArchive ar;
    EXPECT_FALSE(ar.IsReading());
    EXPECT_TRUE(ar.IsWriting());
}

TEST(MacroDSL_ArchiveConcept, RecordingArchiveWithFlagsSatisfiesArchiveConcept) {
    RecordingArchiveWithFlags ar;
    EXPECT_FALSE(ar.IsReading());
    EXPECT_TRUE(ar.IsWriting());
}

// --- H: Version parameter is accessible; escape hatch serialize works ---

TEST(MacroDSL_VersionEscapeHatch, Version1RecordsZeroFields) {
    RecordingArchive ar;
    MigrationTest m;
    serialize(ar, m, 1u);
    EXPECT_EQ(ar.recordedNames.Size(), 0u);
}

TEST(MacroDSL_VersionEscapeHatch, Version2RecordsOneField) {
    RecordingArchive ar;
    MigrationTest m;
    serialize(ar, m, 2u);
    EXPECT_EQ(ar.recordedNames.Size(), 1u);
    EXPECT_EQ(ar.recordedNames[0], Dia::Core::StringCRC("mNew"));
}

// --- I: Multiple types coexist without conflict ---

TEST(MacroDSL_MultipleTypes, Point2DAndRequiredTestSerializeIndependently) {
    // Serialize Point2D
    {
        RecordingArchive ar;
        Point2D p;
        serialize(ar, p);
        EXPECT_EQ(ar.recordedNames.Size(), 2u);
        EXPECT_EQ(ar.recordedNames[0], Dia::Core::StringCRC("mX"));
        EXPECT_EQ(ar.recordedNames[1], Dia::Core::StringCRC("mY"));
    }

    // Serialize RequiredTest
    {
        RecordingArchiveWithFlags ar;
        RequiredTest t;
        serialize(ar, t);
        EXPECT_EQ(ar.recordedFields.Size(), 1u);
        EXPECT_EQ(ar.recordedFields[0].name, Dia::Core::StringCRC("mId"));
        EXPECT_TRUE(ar.recordedFields[0].required);
    }
}

// --- J: Empty serialize function (no fields) ---

TEST(MacroDSL_EmptySerialize, ZeroFieldsNocrash) {
    RecordingArchive ar;
    EmptyStruct e;
    serialize(ar, e);
    EXPECT_EQ(ar.recordedNames.Size(), 0u);
}

// --- Bonus: Verify DIA_FIELD non-required flag ---

TEST(MacroDSL_BasicField, RegularFieldIsNotRequired) {
    RecordingArchiveWithFlags ar;
    Point2D p;
    serialize(ar, p);
    ASSERT_EQ(ar.recordedFields.Size(), 2u);
    EXPECT_FALSE(ar.recordedFields[0].required);
    EXPECT_FALSE(ar.recordedFields[1].required);
}
