#include <gtest/gtest.h>
#include "DiaCore/Reflect/ReflectMacros.h"
#include "DiaCore/Reflect/BinaryArchive.h"
#include "DiaCore/Reflect/SerializeResult.h"
#include "DiaCore/CRC/StringCRC.h"

using namespace Dia::Reflect;

// =============================================================================
// Test structs — must be at file scope (DIA_SERIALIZE defines a free function
// template and free functions cannot be defined inside a function body).
// =============================================================================

struct BVec2 {
    float mX = 0.0f;
    float mY = 0.0f;
};

DIA_SERIALIZE(BVec2, 1)
    DIA_FIELD(mX)
    DIA_FIELD(mY)
DIA_SERIALIZE_END

struct BParticle {
    BVec2 mPos{1.0f, 2.0f};
    float mMass   = 1.0f;
    bool  mActive = true;
    int   mId     = 42;
};

DIA_SERIALIZE(BParticle, 1)
    DIA_FIELD(mPos)
    DIA_FIELD(mMass)
    DIA_FIELD(mActive)
    DIA_FIELD(mId)
DIA_SERIALIZE_END

struct BWithRequired {
    int   mId    = 0;
    float mValue = 0.0f;
};

DIA_SERIALIZE(BWithRequired, 1)
    DIA_FIELD_REQUIRED(mId)
    DIA_FIELD(mValue)
DIA_SERIALIZE_END

struct BAllPrimitives {
    bool           mBool   = false;
    int            mInt    = 0;
    unsigned int   mUInt   = 0u;
    float          mFloat  = 0.0f;
    double         mDouble = 0.0;
    char           mChar   = 0;
    unsigned char  mUChar  = 0;
    short          mShort  = 0;
    unsigned short mUShort = 0;
    int64_t        mInt64  = 0;
    uint64_t       mUInt64 = 0u;
};

DIA_SERIALIZE(BAllPrimitives, 1)
    DIA_FIELD(mBool)
    DIA_FIELD(mInt)
    DIA_FIELD(mUInt)
    DIA_FIELD(mFloat)
    DIA_FIELD(mDouble)
    DIA_FIELD(mChar)
    DIA_FIELD(mUChar)
    DIA_FIELD(mShort)
    DIA_FIELD(mUShort)
    DIA_FIELD(mInt64)
    DIA_FIELD(mUInt64)
DIA_SERIALIZE_END

// Three-level nesting for depth test
struct BLevel3 { int mZ = 99; };
DIA_SERIALIZE(BLevel3, 1) DIA_FIELD(mZ) DIA_SERIALIZE_END

struct BLevel2 { BLevel3 mInner{}; };
DIA_SERIALIZE(BLevel2, 1) DIA_FIELD(mInner) DIA_SERIALIZE_END

struct BLevel1 { BLevel2 mMid{}; };
DIA_SERIALIZE(BLevel1, 1) DIA_FIELD(mMid) DIA_SERIALIZE_END

// =============================================================================
// Helpers
// =============================================================================

// Write a struct to a BinaryWriteArchive and return a heap-allocated copy of
// the bytes together with the byte count.  Caller must delete[] the returned
// pointer.
template<typename T>
static void WriteToBinary(const T& obj,
                          const uint8_t*& outPtr,
                          uint32_t&       outSize)
{
    static BinaryWriteArchive ar;  // static so we can take a stable pointer
    ar = BinaryWriteArchive{};
    ar.WriteVersion(1u);
    T& mutableObj = const_cast<T&>(obj);
    serialize(ar, mutableObj, 1u);
    outPtr  = ar.GetData();
    outSize = ar.GetSize();
}

// Helper: write, then read back into dst
template<typename T>
static BinaryReadArchive WriteAndRead(const T& src, T& dst)
{
    const uint8_t* data = nullptr;
    uint32_t       size = 0u;
    WriteToBinary(src, data, size);

    BinaryReadArchive rAr(data, size);
    rAr.ReadVersion();
    serialize(rAr, dst, 1u);
    return rAr;  // return by value so caller can inspect result
}

// =============================================================================
// A) Round-trip all arithmetic types
// =============================================================================

TEST(BinaryArchive_RoundTrip, Bool) {
    BAllPrimitives src; src.mBool = true;
    BAllPrimitives dst; dst.mBool = false;
    WriteAndRead(src, dst);
    EXPECT_EQ(dst.mBool, true);
}

TEST(BinaryArchive_RoundTrip, Int) {
    BAllPrimitives src; src.mInt = -12345;
    BAllPrimitives dst; dst.mInt = 0;
    WriteAndRead(src, dst);
    EXPECT_EQ(dst.mInt, -12345);
}

TEST(BinaryArchive_RoundTrip, UnsignedInt) {
    BAllPrimitives src; src.mUInt = 99999u;
    BAllPrimitives dst; dst.mUInt = 0u;
    WriteAndRead(src, dst);
    EXPECT_EQ(dst.mUInt, 99999u);
}

TEST(BinaryArchive_RoundTrip, Float) {
    BAllPrimitives src; src.mFloat = 1.5f;
    BAllPrimitives dst; dst.mFloat = 0.0f;
    WriteAndRead(src, dst);
    EXPECT_NEAR(dst.mFloat, 1.5f, 0.0001f);
}

TEST(BinaryArchive_RoundTrip, Double) {
    BAllPrimitives src; src.mDouble = 2.718281828;
    BAllPrimitives dst; dst.mDouble = 0.0;
    WriteAndRead(src, dst);
    EXPECT_NEAR(dst.mDouble, 2.718281828, 1e-9);
}

TEST(BinaryArchive_RoundTrip, Char) {
    BAllPrimitives src; src.mChar = 'Z';
    BAllPrimitives dst; dst.mChar = 0;
    WriteAndRead(src, dst);
    EXPECT_EQ(dst.mChar, 'Z');
}

TEST(BinaryArchive_RoundTrip, UnsignedChar) {
    BAllPrimitives src; src.mUChar = 200;
    BAllPrimitives dst; dst.mUChar = 0;
    WriteAndRead(src, dst);
    EXPECT_EQ(dst.mUChar, static_cast<unsigned char>(200));
}

TEST(BinaryArchive_RoundTrip, Short) {
    BAllPrimitives src; src.mShort = -300;
    BAllPrimitives dst; dst.mShort = 0;
    WriteAndRead(src, dst);
    EXPECT_EQ(dst.mShort, static_cast<short>(-300));
}

TEST(BinaryArchive_RoundTrip, UnsignedShort) {
    BAllPrimitives src; src.mUShort = 500;
    BAllPrimitives dst; dst.mUShort = 0;
    WriteAndRead(src, dst);
    EXPECT_EQ(dst.mUShort, static_cast<unsigned short>(500));
}

// =============================================================================
// M) int64_t and uint64_t round-trip
// =============================================================================

TEST(BinaryArchive_RoundTrip, Int64) {
    BAllPrimitives src; src.mInt64 = -9000000000LL;
    BAllPrimitives dst; dst.mInt64 = 0;
    WriteAndRead(src, dst);
    EXPECT_EQ(dst.mInt64, -9000000000LL);
}

TEST(BinaryArchive_RoundTrip, UInt64) {
    BAllPrimitives src; src.mUInt64 = 18000000000ULL;
    BAllPrimitives dst; dst.mUInt64 = 0u;
    WriteAndRead(src, dst);
    EXPECT_EQ(dst.mUInt64, 18000000000ULL);
}

// =============================================================================
// B) Round-trip nested struct (BVec2 inside BParticle)
// =============================================================================

TEST(BinaryArchive_RoundTrip, NestedStruct_ValuesRoundTrip) {
    BParticle src;
    src.mPos.mX = 3.0f;
    src.mPos.mY = 4.0f;
    src.mMass   = 2.5f;
    src.mActive = false;
    src.mId     = 7;

    BParticle dst;
    dst.mPos.mX = 0.0f;
    dst.mPos.mY = 0.0f;
    dst.mMass   = 0.0f;
    dst.mActive = true;
    dst.mId     = 0;

    WriteAndRead(src, dst);

    EXPECT_NEAR(dst.mPos.mX, 3.0f, 0.0001f);
    EXPECT_NEAR(dst.mPos.mY, 4.0f, 0.0001f);
    EXPECT_NEAR(dst.mMass,   2.5f, 0.0001f);
    EXPECT_EQ(dst.mActive, false);
    EXPECT_EQ(dst.mId, 7);
}

// =============================================================================
// C) Version header written correctly — first 2 bytes equal the version
// =============================================================================

TEST(BinaryArchive_Format, VersionHeaderIsFirst2Bytes) {
    BVec2 v;
    v.mX = 1.0f;
    v.mY = 2.0f;

    BinaryWriteArchive wAr;
    wAr.WriteVersion(3u);
    serialize(wAr, v, 1u);

    const uint8_t* data = wAr.GetData();
    ASSERT_GE(wAr.GetSize(), 2u);

    uint16_t readBack = 0u;
    memcpy(&readBack, data, 2u);
    EXPECT_EQ(readBack, static_cast<uint16_t>(3u));
}

// =============================================================================
// D) Field CRC matches StringCRC("mX").Value()
// =============================================================================

TEST(BinaryArchive_Format, FieldCrcMatchesStringCRC) {
    BVec2 v;
    v.mX = 1.0f;
    v.mY = 0.0f;

    BinaryWriteArchive wAr;
    wAr.WriteVersion(1u);
    serialize(wAr, v, 1u);

    // Layout after 2-byte version:
    //   [4] CRC of first field
    //   [4] data size
    //   [4] float value
    const uint8_t* data = wAr.GetData();
    ASSERT_GE(wAr.GetSize(), 2u + 4u);

    uint32_t writtenCrc = 0u;
    memcpy(&writtenCrc, data + 2u, 4u);

    const uint32_t expectedCrc = Dia::Core::StringCRC("mX").Value();
    EXPECT_EQ(writtenCrc, expectedCrc);
}

// =============================================================================
// E) Data-size field equals sizeof(float) for a float field
// =============================================================================

TEST(BinaryArchive_Format, DataSizeFieldMatchesSizeofFloat) {
    BVec2 v;
    v.mX = 7.0f;
    v.mY = 0.0f;

    BinaryWriteArchive wAr;
    wAr.WriteVersion(1u);
    serialize(wAr, v, 1u);

    const uint8_t* data = wAr.GetData();
    // Byte layout: [2 version][4 crc][4 size][4 float value] ...
    ASSERT_GE(wAr.GetSize(), 2u + 4u + 4u);

    uint32_t dataSize = 0u;
    memcpy(&dataSize, data + 2u + 4u, 4u);
    EXPECT_EQ(dataSize, static_cast<uint32_t>(sizeof(float)));
}

// =============================================================================
// F) Unknown extra field — reader skips it, known fields read correctly
// =============================================================================

TEST(BinaryArchive_ForwardCompat, UnknownFieldSkipped) {
    // Build a blob manually that looks like it came from a newer writer that
    // added an extra "mExtra" float field before "mX" and "mY".
    const uint32_t crcExtra = Dia::Core::StringCRC("mExtra").Value();
    const uint32_t crcX     = Dia::Core::StringCRC("mX").Value();
    const uint32_t crcY     = Dia::Core::StringCRC("mY").Value();

    const float extraVal = 999.0f;
    const float xVal     = 5.0f;
    const float yVal     = 6.0f;

    // Build: [version=1][crcExtra][4][999.f][crcX][4][5.f][crcY][4][6.f]
    uint8_t blob[2u + 3u * (4u + 4u + 4u)];
    uint32_t pos = 0u;

    uint16_t version = 1u;
    memcpy(blob + pos, &version, 2u); pos += 2u;

    auto writeField = [&](uint32_t crc, float val) {
        uint32_t sz = sizeof(float);
        memcpy(blob + pos, &crc, 4u); pos += 4u;
        memcpy(blob + pos, &sz,  4u); pos += 4u;
        memcpy(blob + pos, &val, 4u); pos += 4u;
    };

    writeField(crcExtra, extraVal);
    writeField(crcX,     xVal);
    writeField(crcY,     yVal);

    BinaryReadArchive rAr(blob, static_cast<uint32_t>(pos));
    rAr.ReadVersion();

    BVec2 dst;
    dst.mX = 0.0f;
    dst.mY = 0.0f;
    serialize(rAr, dst, 1u);

    EXPECT_TRUE(rAr.GetResult().IsOk());
    EXPECT_NEAR(dst.mX, 5.0f, 0.0001f);
    EXPECT_NEAR(dst.mY, 6.0f, 0.0001f);
}

// =============================================================================
// G) Required field present — SerializeResult is Ok
// =============================================================================

TEST(BinaryArchive_RequiredField, PresentProducesNoError) {
    BWithRequired src; src.mId = 99; src.mValue = 3.14f;
    BWithRequired dst;
    auto rAr = WriteAndRead(src, dst);
    EXPECT_TRUE(rAr.GetResult().IsOk());
    EXPECT_EQ(dst.mId, 99);
}

// =============================================================================
// H) Required field missing — SerializeResult has 1 error
// =============================================================================

TEST(BinaryArchive_RequiredField, MissingProducesError) {
    // Build a blob that only contains mValue, not mId
    const uint32_t crcValue = Dia::Core::StringCRC("mValue").Value();
    const float    floatVal = 3.14f;

    uint8_t blob[2u + 4u + 4u + 4u];
    uint32_t pos = 0u;
    uint16_t version = 1u;
    memcpy(blob + pos, &version, 2u); pos += 2u;
    uint32_t sz = sizeof(float);
    memcpy(blob + pos, &crcValue, 4u); pos += 4u;
    memcpy(blob + pos, &sz,       4u); pos += 4u;
    memcpy(blob + pos, &floatVal, 4u); pos += 4u;

    BinaryReadArchive rAr(blob, static_cast<uint32_t>(pos));
    rAr.ReadVersion();
    BWithRequired obj;
    serialize(rAr, obj, 1u);

    EXPECT_FALSE(rAr.GetResult().IsOk());
    EXPECT_EQ(rAr.GetResult().ErrorCount(), 1u);
    EXPECT_EQ(rAr.GetResult().GetError(0).kind,
              SerializeErrorKind::RequiredFieldMissing);
    EXPECT_EQ(rAr.GetResult().GetError(0).fieldName,
              Dia::Core::StringCRC("mId"));
}

// =============================================================================
// I) Missing optional field keeps C++ default
// =============================================================================

TEST(BinaryArchive_OptionalField, MissingFieldKeepsDefault) {
    // Build blob with only mX — mY deliberately absent
    const uint32_t crcX = Dia::Core::StringCRC("mX").Value();
    const float    xVal = 9.0f;

    uint8_t blob[2u + 4u + 4u + 4u];
    uint32_t pos = 0u;
    uint16_t ver = 1u;
    memcpy(blob + pos, &ver,  2u); pos += 2u;
    uint32_t sz = sizeof(float);
    memcpy(blob + pos, &crcX, 4u); pos += 4u;
    memcpy(blob + pos, &sz,   4u); pos += 4u;
    memcpy(blob + pos, &xVal, 4u); pos += 4u;

    BVec2 dst;
    dst.mX = 0.0f;
    dst.mY = 7.7f;  // set a non-zero default to verify it is preserved

    BinaryReadArchive rAr(blob, static_cast<uint32_t>(pos));
    rAr.ReadVersion();
    serialize(rAr, dst, 1u);

    EXPECT_TRUE(rAr.GetResult().IsOk());
    EXPECT_NEAR(dst.mX, 9.0f, 0.0001f);
    EXPECT_NEAR(dst.mY, 7.7f, 0.0001f);  // unchanged
}

// =============================================================================
// J) Full round-trip: write BParticle, read back, all fields match
// =============================================================================

TEST(BinaryArchive_RoundTrip, FullParticleRoundTrip) {
    BParticle src;
    src.mPos.mX = 11.0f;
    src.mPos.mY = 22.0f;
    src.mMass   = 3.3f;
    src.mActive = false;
    src.mId     = 55;

    BParticle dst;   // default-constructed
    WriteAndRead(src, dst);

    EXPECT_NEAR(dst.mPos.mX, 11.0f, 0.0001f);
    EXPECT_NEAR(dst.mPos.mY, 22.0f, 0.0001f);
    EXPECT_NEAR(dst.mMass,    3.3f, 0.0001f);
    EXPECT_EQ(dst.mActive, false);
    EXPECT_EQ(dst.mId, 55);
}

// =============================================================================
// K) Binary size is deterministic — same struct written twice produces same size
// =============================================================================

TEST(BinaryArchive_Determinism, SameSizeTwice) {
    BParticle src;
    src.mPos.mX = 1.0f;
    src.mPos.mY = 2.0f;
    src.mMass   = 3.0f;
    src.mActive = true;
    src.mId     = 7;

    BinaryWriteArchive ar1;
    ar1.WriteVersion(1u);
    serialize(ar1, src, 1u);

    BinaryWriteArchive ar2;
    ar2.WriteVersion(1u);
    serialize(ar2, src, 1u);

    EXPECT_EQ(ar1.GetSize(), ar2.GetSize());
}

// =============================================================================
// L) Zero values round-trip (not confused with "empty / missing")
// =============================================================================

TEST(BinaryArchive_RoundTrip, ZeroValues) {
    BAllPrimitives src;
    src.mBool   = false;
    src.mInt    = 0;
    src.mUInt   = 0u;
    src.mFloat  = 0.0f;
    src.mDouble = 0.0;

    BAllPrimitives dst;
    dst.mBool   = true;
    dst.mInt    = 99;
    dst.mUInt   = 99u;
    dst.mFloat  = 99.0f;
    dst.mDouble = 99.0;

    WriteAndRead(src, dst);

    EXPECT_EQ(dst.mBool, false);
    EXPECT_EQ(dst.mInt,  0);
    EXPECT_EQ(dst.mUInt, 0u);
    EXPECT_NEAR(dst.mFloat,  0.0f, 0.0001f);
    EXPECT_NEAR(dst.mDouble, 0.0,  1e-9);
}

// =============================================================================
// N) Write + read version header separately
// =============================================================================

TEST(BinaryArchive_VersionHeader, WriteAndReadBackVersion) {
    BinaryWriteArchive wAr;
    wAr.WriteVersion(2u);
    // No fields — just the header

    BinaryReadArchive rAr(wAr.GetData(), wAr.GetSize());
    uint16_t ver = rAr.ReadVersion();
    EXPECT_EQ(ver, static_cast<uint16_t>(2u));
}

// =============================================================================
// O) Maximum depth nesting (3 levels) — no crash
// =============================================================================

TEST(BinaryArchive_Nesting, ThreeLevelsDeepNoCrash) {
    BLevel1 src;
    src.mMid.mInner.mZ = 77;

    BLevel1 dst;
    dst.mMid.mInner.mZ = 0;

    WriteAndRead(src, dst);

    EXPECT_EQ(dst.mMid.mInner.mZ, 77);
}

// =============================================================================
// P) Two independent archives don't share state
// =============================================================================

TEST(BinaryArchive_Independence, TwoArchivesDontShareState) {
    BVec2 v1; v1.mX = 10.0f; v1.mY = 20.0f;
    BVec2 v2; v2.mX = 30.0f; v2.mY = 40.0f;

    BinaryWriteArchive wAr1;
    wAr1.WriteVersion(1u);
    serialize(wAr1, v1, 1u);

    BinaryWriteArchive wAr2;
    wAr2.WriteVersion(1u);
    serialize(wAr2, v2, 1u);

    // Each archive should have its own independent data
    EXPECT_EQ(wAr1.GetSize(), wAr2.GetSize());

    BVec2 dst1; dst1.mX = 0.0f; dst1.mY = 0.0f;
    BVec2 dst2; dst2.mX = 0.0f; dst2.mY = 0.0f;

    BinaryReadArchive rAr1(wAr1.GetData(), wAr1.GetSize());
    rAr1.ReadVersion();
    serialize(rAr1, dst1, 1u);

    BinaryReadArchive rAr2(wAr2.GetData(), wAr2.GetSize());
    rAr2.ReadVersion();
    serialize(rAr2, dst2, 1u);

    EXPECT_NEAR(dst1.mX, 10.0f, 0.0001f);
    EXPECT_NEAR(dst1.mY, 20.0f, 0.0001f);
    EXPECT_NEAR(dst2.mX, 30.0f, 0.0001f);
    EXPECT_NEAR(dst2.mY, 40.0f, 0.0001f);
}

// =============================================================================
// Archive concept: BinaryWriteArchive and BinaryReadArchive satisfy Archive
// =============================================================================

TEST(BinaryArchive_Concept, WriteArchiveSatisfiesConcept) {
    BinaryWriteArchive ar;
    EXPECT_FALSE(ar.IsReading());
    EXPECT_TRUE(ar.IsWriting());
}

TEST(BinaryArchive_Concept, ReadArchiveSatisfiesConcept) {
    uint8_t dummy[2] = {0, 0};
    BinaryReadArchive ar(dummy, 2u);
    EXPECT_TRUE(ar.IsReading());
    EXPECT_FALSE(ar.IsWriting());
}
