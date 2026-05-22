// =============================================================================
// TestDiaRigidBody2DSerializers.cpp
// Tests for serialize() free functions defined in DiaRigidBody2DSerializers.h.
//
// Covers: ResponseConfig, ConstraintSolverConfig, WorldDef
// Archives: JSON round-trip, binary round-trip, missing-field tolerance.
// Note: WorldDef::broadPhase is a non-owning ptr — verified it is NOT written.
// =============================================================================

#include <gtest/gtest.h>
#include "DiaRigidBody2D/DiaRigidBody2DSerializers.h"
#include "DiaCore/Reflect/JsonArchive.h"
#include "DiaCore/Reflect/BinaryArchive.h"
#include "DiaCore/Json/external/json/json.h"

using namespace Dia::Reflect;
using namespace Dia::RigidBody2D;
using namespace Dia::Maths;

// =============================================================================
// Helpers
// =============================================================================

template<typename T>
static Json::Value WriteToJson(T& obj) {
    JsonWriteArchive ar;
    serialize(ar, obj);
    return ar.GetRoot();
}

template<typename T>
static void ReadFromJson(const Json::Value& root, T& obj) {
    JsonReadArchive ar(root);
    serialize(ar, obj);
}

template<typename T>
static void WriteAndReadBinary(const T& src, T& dst) {
    BinaryWriteArchive wAr;
    wAr.WriteVersion(1u);
    T& mutableSrc = const_cast<T&>(src);
    serialize(wAr, mutableSrc, 1u);

    BinaryReadArchive rAr(wAr.GetData(), wAr.GetSize());
    rAr.ReadVersion();
    serialize(rAr, dst, 1u);
}

// =============================================================================
// ResponseConfig
// =============================================================================

TEST(DiaRigidBody2DSerializer_ResponseConfig, JsonRoundTrip) {
    ResponseConfig src;
    src.baumgarteSlop           = 0.02f;
    src.baumgarteFactor         = 0.3f;
    src.restitutionVelocitySlop = 1.0f;

    Json::Value root = WriteToJson(src);

    ResponseConfig dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.baumgarteSlop,           0.02f);
    EXPECT_FLOAT_EQ(dst.baumgarteFactor,          0.3f);
    EXPECT_FLOAT_EQ(dst.restitutionVelocitySlop,  1.0f);
}

TEST(DiaRigidBody2DSerializer_ResponseConfig, BinaryRoundTrip) {
    ResponseConfig src;
    src.baumgarteSlop   = 0.005f;
    src.baumgarteFactor = 0.15f;

    ResponseConfig dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.baumgarteSlop,   0.005f);
    EXPECT_FLOAT_EQ(dst.baumgarteFactor, 0.15f);
}

TEST(DiaRigidBody2DSerializer_ResponseConfig, MissingFieldKeepsDefault) {
    Json::Value partial;
    partial["baumgarteFactor"] = 0.5f;

    ResponseConfig dst;
    ReadFromJson(partial, dst);

    EXPECT_FLOAT_EQ(dst.baumgarteFactor,  0.5f);
    EXPECT_FLOAT_EQ(dst.baumgarteSlop,    0.01f);  // default preserved
}

// =============================================================================
// ConstraintSolverConfig
// =============================================================================

TEST(DiaRigidBody2DSerializer_ConstraintSolverConfig, JsonRoundTrip) {
    ConstraintSolverConfig src;
    src.iterations = 20;

    Json::Value root = WriteToJson(src);

    ConstraintSolverConfig dst;
    ReadFromJson(root, dst);

    EXPECT_EQ(dst.iterations, 20);
}

TEST(DiaRigidBody2DSerializer_ConstraintSolverConfig, BinaryRoundTrip) {
    ConstraintSolverConfig src;
    src.iterations = 5;

    ConstraintSolverConfig dst;
    WriteAndReadBinary(src, dst);

    EXPECT_EQ(dst.iterations, 5);
}

// =============================================================================
// WorldDef
// =============================================================================

TEST(DiaRigidBody2DSerializer_WorldDef, JsonRoundTrip) {
    WorldDef src;
    src.gravity               = Vector2D(0.0f, -9.81f);
    src.fixedTimestep         = 1.0f / 120.0f;
    src.maxSubSteps           = 4;
    src.sleepLinearThreshold  = 0.02f;
    src.sleepAngularThreshold = 0.02f;
    src.sleepTimeThreshold    = 1.0f;
    src.responseConfig.baumgarteFactor   = 0.25f;
    src.constraintConfig.iterations      = 15;

    Json::Value root = WriteToJson(src);

    WorldDef dst;
    ReadFromJson(root, dst);

    EXPECT_FLOAT_EQ(dst.gravity.x,              0.0f);
    EXPECT_FLOAT_EQ(dst.gravity.y,             -9.81f);
    EXPECT_FLOAT_EQ(dst.fixedTimestep,          1.0f / 120.0f);
    EXPECT_EQ(dst.maxSubSteps,                  4);
    EXPECT_FLOAT_EQ(dst.sleepLinearThreshold,   0.02f);
    EXPECT_FLOAT_EQ(dst.sleepAngularThreshold,  0.02f);
    EXPECT_FLOAT_EQ(dst.sleepTimeThreshold,     1.0f);
    EXPECT_FLOAT_EQ(dst.responseConfig.baumgarteFactor,  0.25f);
    EXPECT_EQ(dst.constraintConfig.iterations,           15);
    // broadPhase must not be touched by deserialization
    EXPECT_EQ(dst.broadPhase, nullptr);
}

TEST(DiaRigidBody2DSerializer_WorldDef, BinaryRoundTrip) {
    WorldDef src;
    src.gravity       = Vector2D(0.0f, -5.0f);
    src.maxSubSteps   = 2;

    WorldDef dst;
    WriteAndReadBinary(src, dst);

    EXPECT_FLOAT_EQ(dst.gravity.y,   -5.0f);
    EXPECT_EQ(dst.maxSubSteps,         2);
    EXPECT_EQ(dst.broadPhase,          nullptr);
}

TEST(DiaRigidBody2DSerializer_WorldDef, BroadPhaseNotSerialised) {
    // Writes a WorldDef with a non-null broadPhase sentinel. The JSON output
    // must not contain a "broadPhase" key — it is a non-owning ptr.
    int sentinel = 42;
    WorldDef src;
    src.broadPhase = reinterpret_cast<Dia::Geometry2D::ISpatialStructure<Dia::RigidBody2D::Body2DBase*>*>(&sentinel);

    Json::Value root = WriteToJson(src);
    EXPECT_TRUE(root["broadPhase"].isNull());

    WorldDef dst;
    ReadFromJson(root, dst);
    EXPECT_EQ(dst.broadPhase, nullptr);
}

TEST(DiaRigidBody2DSerializer_WorldDef, MissingFieldKeepsDefault) {
    Json::Value partial;
    partial["maxSubSteps"] = 16;

    WorldDef dst;
    ReadFromJson(partial, dst);

    EXPECT_EQ(dst.maxSubSteps, 16);
    EXPECT_FLOAT_EQ(dst.fixedTimestep, 1.0f / 60.0f);  // default preserved
    EXPECT_FLOAT_EQ(dst.gravity.y,    -9.81f);           // default preserved
}
