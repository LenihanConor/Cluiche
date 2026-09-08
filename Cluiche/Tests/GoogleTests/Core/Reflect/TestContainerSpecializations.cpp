#include <gtest/gtest.h>
#include "DiaCore/Reflect/Reflect.h"
#include "DiaCore/Json/external/json/json.h"
#include "DiaCore/Containers/Arrays/DynamicArrayC.h"

using namespace Dia::Reflect;
using namespace Dia::Core::Containers;

// =============================================================================
// Test structs — must be at file scope (DIA_SERIALIZE defines free function)
// =============================================================================

struct ItemCS {
    int   mId    = 0;
    float mValue = 0.0f;
};

DIA_SERIALIZE(ItemCS, 1)
    DIA_FIELD(mId)
    DIA_FIELD(mValue)
DIA_SERIALIZE_END

struct WithStaticArray {
    int   mCounts[4]  = {0, 0, 0, 0};
    float mWeights[3] = {0.0f, 0.0f, 0.0f};
};

DIA_SERIALIZE(WithStaticArray, 1)
    DIA_FIELD(mCounts)
    DIA_FIELD(mWeights)
DIA_SERIALIZE_END

struct WithStaticStructArray {
    ItemCS mItems[3];
};

DIA_SERIALIZE(WithStaticStructArray, 1)
    DIA_FIELD(mItems)
DIA_SERIALIZE_END

struct WithDynArray {
    DynamicArrayC<int,    8> mInts;
    DynamicArrayC<ItemCS, 4> mItems;
};

DIA_SERIALIZE(WithDynArray, 1)
    DIA_FIELD(mInts)
    DIA_FIELD(mItems)
DIA_SERIALIZE_END

// =============================================================================
// Helpers
// =============================================================================

template<typename T>
Json::Value WriteToJson(T& obj) {
    JsonWriteArchive ar;
    serialize(ar, obj, 0u);
    return ar.GetRoot();
}

template<typename T>
void ReadFromJson(const Json::Value& root, T& obj) {
    JsonReadArchive ar(root);
    serialize(ar, obj, 0u);
}

template<typename T>
void BinaryRoundTrip(const T& src, T& dst) {
    BinaryWriteArchive war;
    war.WriteVersion(0u);
    T& mutableSrc = const_cast<T&>(src);
    serialize(war, mutableSrc, 0u);

    BinaryReadArchive rar(war.GetData(), war.GetSize());
    rar.ReadVersion();
    serialize(rar, dst, 0u);
}

// =============================================================================
// A) Static int array round-trip JSON
// =============================================================================

TEST(ContainerSpecializations_Json, StaticIntArray_RoundTrip) {
    WithStaticArray src;
    src.mCounts[0] = 1; src.mCounts[1] = 2; src.mCounts[2] = 3; src.mCounts[3] = 4;

    Json::Value root = WriteToJson(src);

    // JSON should be an array under "mCounts"
    ASSERT_TRUE(root.isMember("mCounts"));
    ASSERT_TRUE(root["mCounts"].isArray());
    EXPECT_EQ(root["mCounts"].size(), 4u);

    WithStaticArray dst;
    ReadFromJson(root, dst);

    EXPECT_EQ(dst.mCounts[0], 1);
    EXPECT_EQ(dst.mCounts[1], 2);
    EXPECT_EQ(dst.mCounts[2], 3);
    EXPECT_EQ(dst.mCounts[3], 4);
}

// =============================================================================
// B) Static float array round-trip JSON
// =============================================================================

TEST(ContainerSpecializations_Json, StaticFloatArray_RoundTrip) {
    WithStaticArray src;
    src.mWeights[0] = 1.5f; src.mWeights[1] = 2.5f; src.mWeights[2] = 3.0f;

    Json::Value root = WriteToJson(src);

    ASSERT_TRUE(root.isMember("mWeights"));
    ASSERT_TRUE(root["mWeights"].isArray());
    EXPECT_EQ(root["mWeights"].size(), 3u);

    WithStaticArray dst;
    ReadFromJson(root, dst);

    EXPECT_NEAR(dst.mWeights[0], 1.5f, 0.0001f);
    EXPECT_NEAR(dst.mWeights[1], 2.5f, 0.0001f);
    EXPECT_NEAR(dst.mWeights[2], 3.0f, 0.0001f);
}

// =============================================================================
// C) DynamicArrayC<int> round-trip JSON
// =============================================================================

TEST(ContainerSpecializations_Json, DynArrayInt_RoundTrip) {
    WithDynArray src;
    src.mInts.Add(10);
    src.mInts.Add(20);
    src.mInts.Add(30);

    Json::Value root = WriteToJson(src);

    ASSERT_TRUE(root.isMember("mInts"));
    ASSERT_TRUE(root["mInts"].isArray());
    EXPECT_EQ(root["mInts"].size(), 3u);

    WithDynArray dst;
    ReadFromJson(root, dst);

    EXPECT_EQ(dst.mInts.Size(), 3u);
    EXPECT_EQ(dst.mInts.At(0), 10);
    EXPECT_EQ(dst.mInts.At(1), 20);
    EXPECT_EQ(dst.mInts.At(2), 30);
}

// =============================================================================
// D) DynamicArrayC<ItemCS> (nested struct elements) round-trip JSON
// =============================================================================

TEST(ContainerSpecializations_Json, DynArrayNestedStruct_RoundTrip) {
    WithDynArray src;
    ItemCS a; a.mId = 1; a.mValue = 1.1f;
    ItemCS b; b.mId = 2; b.mValue = 2.2f;
    src.mItems.Add(a);
    src.mItems.Add(b);

    Json::Value root = WriteToJson(src);

    ASSERT_TRUE(root.isMember("mItems"));
    ASSERT_TRUE(root["mItems"].isArray());
    EXPECT_EQ(root["mItems"].size(), 2u);

    WithDynArray dst;
    ReadFromJson(root, dst);

    EXPECT_EQ(dst.mItems.Size(), 2u);
    EXPECT_EQ(dst.mItems.At(0).mId, 1);
    EXPECT_NEAR(dst.mItems.At(0).mValue, 1.1f, 0.0001f);
    EXPECT_EQ(dst.mItems.At(1).mId, 2);
    EXPECT_NEAR(dst.mItems.At(1).mValue, 2.2f, 0.0001f);
}

// =============================================================================
// E) Static array round-trip Binary
// =============================================================================

TEST(ContainerSpecializations_Binary, StaticIntArray_RoundTrip) {
    WithStaticArray src;
    src.mCounts[0] = 5; src.mCounts[1] = 6; src.mCounts[2] = 7; src.mCounts[3] = 8;

    WithStaticArray dst;
    BinaryRoundTrip(src, dst);

    EXPECT_EQ(dst.mCounts[0], 5);
    EXPECT_EQ(dst.mCounts[1], 6);
    EXPECT_EQ(dst.mCounts[2], 7);
    EXPECT_EQ(dst.mCounts[3], 8);
}

// =============================================================================
// F) DynamicArrayC round-trip Binary
// =============================================================================

TEST(ContainerSpecializations_Binary, DynArrayInt_RoundTrip) {
    WithDynArray src;
    src.mInts.Add(100);
    src.mInts.Add(200);

    WithDynArray dst;
    BinaryRoundTrip(src, dst);

    EXPECT_EQ(dst.mInts.Size(), 2u);
    EXPECT_EQ(dst.mInts.At(0), 100);
    EXPECT_EQ(dst.mInts.At(1), 200);
}

// =============================================================================
// G) Read JSON array with fewer elements than static array size
//    → remaining elements keep their C++ defaults
// =============================================================================

TEST(ContainerSpecializations_Json, StaticArray_FewerElementsInJson_KeepsDefaults) {
    // Build a JSON node with only 2 elements for a 4-element array
    Json::Value root(Json::objectValue);
    root["mCounts"] = Json::Value(Json::arrayValue);
    root["mCounts"].append(Json::Value(99));
    root["mCounts"].append(Json::Value(88));
    // mCounts[2] and mCounts[3] intentionally absent

    WithStaticArray dst;
    dst.mCounts[2] = 77; // set a non-zero default to verify it's preserved
    dst.mCounts[3] = 66;

    ReadFromJson(root, dst);

    EXPECT_EQ(dst.mCounts[0], 99);
    EXPECT_EQ(dst.mCounts[1], 88);
    EXPECT_EQ(dst.mCounts[2], 77); // preserved — not overwritten
    EXPECT_EQ(dst.mCounts[3], 66); // preserved — not overwritten
}

// =============================================================================
// H) Read JSON array with more elements than DynamicArrayC capacity
//    → truncated, no crash
// =============================================================================

TEST(ContainerSpecializations_Json, DynArray_MoreElementsThanCapacity_Truncated) {
    // mInts has capacity 8; write 10 elements in JSON
    Json::Value root(Json::objectValue);
    root["mInts"] = Json::Value(Json::arrayValue);
    for (int i = 0; i < 10; ++i) {
        root["mInts"].append(Json::Value(i));
    }
    root["mItems"] = Json::Value(Json::arrayValue); // empty

    WithDynArray dst;
    ReadFromJson(root, dst);

    // Should have at most capacity (8), no crash
    EXPECT_LE(dst.mInts.Size(), 8u);
    EXPECT_EQ(dst.mInts.Size(), 8u); // exactly 8 accepted, 2 dropped
    EXPECT_EQ(dst.mInts.At(0), 0);
    EXPECT_EQ(dst.mInts.At(7), 7);
}

// =============================================================================
// I) Empty DynamicArrayC round-trip JSON (Size()==0 → empty array)
// =============================================================================

TEST(ContainerSpecializations_Json, DynArray_Empty_RoundTrip) {
    WithDynArray src;
    // mInts is empty by default

    Json::Value root = WriteToJson(src);

    ASSERT_TRUE(root.isMember("mInts"));
    ASSERT_TRUE(root["mInts"].isArray());
    EXPECT_EQ(root["mInts"].size(), 0u);

    WithDynArray dst;
    dst.mInts.Add(42); // pre-populate; should be cleared
    ReadFromJson(root, dst);

    EXPECT_EQ(dst.mInts.Size(), 0u);
}

// =============================================================================
// J) DIA_FIELD works directly for both array types (no special macro needed)
//    (Covered by A-F above — this test makes it explicit as a doc test)
// =============================================================================

TEST(ContainerSpecializations_Json, DiaField_WorksForArrayTypes_NoSpecialMacro) {
    // Verify the structs using DIA_FIELD for arrays compile and function correctly
    WithStaticArray sa;
    sa.mCounts[0] = 42;
    Json::Value root = WriteToJson(sa);
    EXPECT_EQ(root["mCounts"][0].asInt(), 42);

    WithDynArray da;
    da.mInts.Add(7);
    root = WriteToJson(da);
    EXPECT_EQ(root["mInts"][0].asInt(), 7);
}

// =============================================================================
// K) Missing array field in JSON → keeps defaults (array untouched)
// =============================================================================

TEST(ContainerSpecializations_Json, MissingArrayField_KeepsDefaults) {
    Json::Value root(Json::objectValue);
    // No "mCounts" or "mWeights" keys

    WithStaticArray dst;
    dst.mCounts[0] = 11;
    dst.mCounts[1] = 22;

    ReadFromJson(root, dst);

    // Should be unchanged
    EXPECT_EQ(dst.mCounts[0], 11);
    EXPECT_EQ(dst.mCounts[1], 22);
}

// =============================================================================
// L) Static array of nested struct items round-trip JSON
// =============================================================================

TEST(ContainerSpecializations_Json, StaticStructArray_RoundTrip) {
    WithStaticStructArray src;
    src.mItems[0].mId = 10; src.mItems[0].mValue = 1.0f;
    src.mItems[1].mId = 20; src.mItems[1].mValue = 2.0f;
    src.mItems[2].mId = 30; src.mItems[2].mValue = 3.0f;

    Json::Value root = WriteToJson(src);

    ASSERT_TRUE(root.isMember("mItems"));
    ASSERT_TRUE(root["mItems"].isArray());
    EXPECT_EQ(root["mItems"].size(), 3u);

    WithStaticStructArray dst;
    ReadFromJson(root, dst);

    EXPECT_EQ(dst.mItems[0].mId, 10);
    EXPECT_NEAR(dst.mItems[0].mValue, 1.0f, 0.0001f);
    EXPECT_EQ(dst.mItems[1].mId, 20);
    EXPECT_NEAR(dst.mItems[1].mValue, 2.0f, 0.0001f);
    EXPECT_EQ(dst.mItems[2].mId, 30);
    EXPECT_NEAR(dst.mItems[2].mValue, 3.0f, 0.0001f);
}

// =============================================================================
// M) DynamicArrayC of nested struct round-trip Binary
// =============================================================================

TEST(ContainerSpecializations_Binary, DynArrayNestedStruct_RoundTrip) {
    WithDynArray src;
    ItemCS x; x.mId = 7; x.mValue = 7.7f;
    ItemCS y; y.mId = 8; y.mValue = 8.8f;
    ItemCS z; z.mId = 9; z.mValue = 9.9f;
    src.mItems.Add(x);
    src.mItems.Add(y);
    src.mItems.Add(z);

    WithDynArray dst;
    BinaryRoundTrip(src, dst);

    EXPECT_EQ(dst.mItems.Size(), 3u);
    EXPECT_EQ(dst.mItems.At(0).mId, 7);
    EXPECT_NEAR(dst.mItems.At(0).mValue, 7.7f, 0.0001f);
    EXPECT_EQ(dst.mItems.At(1).mId, 8);
    EXPECT_NEAR(dst.mItems.At(1).mValue, 8.8f, 0.0001f);
    EXPECT_EQ(dst.mItems.At(2).mId, 9);
    EXPECT_NEAR(dst.mItems.At(2).mValue, 9.9f, 0.0001f);
}

// =============================================================================
// N) Empty DynamicArrayC round-trip Binary (count=0, no crash)
// =============================================================================

TEST(ContainerSpecializations_Binary, DynArray_Empty_RoundTrip) {
    WithDynArray src;
    // mInts and mItems are empty

    WithDynArray dst;
    dst.mInts.Add(99); // pre-populate; should be cleared

    BinaryRoundTrip(src, dst);

    EXPECT_EQ(dst.mInts.Size(), 0u);
    EXPECT_EQ(dst.mItems.Size(), 0u);
}

// =============================================================================
// O) Static float array round-trip Binary
// =============================================================================

TEST(ContainerSpecializations_Binary, StaticFloatArray_RoundTrip) {
    WithStaticArray src;
    src.mWeights[0] = 0.25f;
    src.mWeights[1] = 0.50f;
    src.mWeights[2] = 0.75f;

    WithStaticArray dst;
    BinaryRoundTrip(src, dst);

    EXPECT_NEAR(dst.mWeights[0], 0.25f, 0.0001f);
    EXPECT_NEAR(dst.mWeights[1], 0.50f, 0.0001f);
    EXPECT_NEAR(dst.mWeights[2], 0.75f, 0.0001f);
}
