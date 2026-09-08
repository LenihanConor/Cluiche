#include <gtest/gtest.h>
// Domain.h pulls in EntityRef.inl transitively (via Domain.inl)
#include <DiaEntity/Domain.h>
#include <DiaEntity/EntityRef.h>
#include <DiaEntity/ComponentPool.h>
#include <DiaCore/Reflect/JsonArchive.h>

// Test components — BpTransform and BpHealth are in DiaEntityTest namespace.
#include "BpTransformComponent.h"

using namespace Dia::Entity;
using DiaEntityTest::BpTransform;
using DiaEntityTest::BpHealth;

// ---------------------------------------------------------------------------
// Test 1: Default-constructed EntityRef holds an invalid entity handle.
// ---------------------------------------------------------------------------
TEST(DiaEntityRef, DefaultConstructedIsInvalid) {
    EntityRef<BpTransform> ref;
    EXPECT_FALSE(ref.IsValid());
    EXPECT_FALSE(ref.entity.IsValid());
}

// ---------------------------------------------------------------------------
// Test 2: Resolve returns false when the ref holds Entity::Invalid().
// ---------------------------------------------------------------------------
TEST(DiaEntityRef, ResolveReturnsFalseForInvalidRef) {
    Domain domain;
    domain.RegisterPool(new ComponentPool<BpTransform>(BpTransform::GetStaticTypeId()));

    EntityRef<BpTransform> ref;  // default — invalid entity
    EXPECT_FALSE(ref.Resolve(domain));
}

// ---------------------------------------------------------------------------
// Test 3: Resolve returns true when the entity exists and has the component.
// ---------------------------------------------------------------------------
TEST(DiaEntityRef, ResolveReturnsTrueWhenComponentPresent) {
    Domain domain;
    domain.RegisterPool(new ComponentPool<BpTransform>(BpTransform::GetStaticTypeId()));

    Entity e = domain.CreateEntity();
    domain.QueueAddComponent<BpTransform>(e, Json::Value());
    domain.EndOfFrame();

    EntityRef<BpTransform> ref;
    ref.entity = e;
    EXPECT_TRUE(ref.Resolve(domain));
}

// ---------------------------------------------------------------------------
// Test 4: Resolve returns false when entity is alive but has no component.
// ---------------------------------------------------------------------------
TEST(DiaEntityRef, ResolveReturnsFalseWhenComponentAbsent) {
    Domain domain;
    domain.RegisterPool(new ComponentPool<BpTransform>(BpTransform::GetStaticTypeId()));

    Entity e = domain.CreateEntity();
    // No QueueAddComponent — entity is alive but BpTransform not attached.

    EntityRef<BpTransform> ref;
    ref.entity = e;
    EXPECT_FALSE(ref.Resolve(domain));
}

// ---------------------------------------------------------------------------
// Test 5: Resolve returns false after entity is destroyed.
// ---------------------------------------------------------------------------
TEST(DiaEntityRef, ResolveReturnsFalseForDestroyedEntity) {
    Domain domain;
    domain.RegisterPool(new ComponentPool<BpTransform>(BpTransform::GetStaticTypeId()));

    Entity e = domain.CreateEntity();
    domain.QueueAddComponent<BpTransform>(e, Json::Value());
    domain.EndOfFrame();

    EntityRef<BpTransform> ref;
    ref.entity = e;
    ASSERT_TRUE(ref.Resolve(domain));  // sanity: alive with component

    domain.QueueDestroy(e);
    domain.EndOfFrame();

    EXPECT_FALSE(ref.Resolve(domain));
}

// ---------------------------------------------------------------------------
// Test 6: Serialize round-trip — write a valid entity index, read it back.
// ---------------------------------------------------------------------------
TEST(DiaEntityRef, SerializeRoundTripValidEntity) {
    EntityRef<BpTransform> refOut;
    refOut.entity = Entity(42u, 3u);  // index=42, gen=3

    // Write
    Dia::Reflect::JsonWriteArchive writeAr;
    serialize(writeAr, refOut, 0u);
    Json::Value json = writeAr.GetRoot();

    ASSERT_TRUE(json.isMember("entityIndex"));
    EXPECT_EQ(json["entityIndex"].asUInt(), 42u);

    // Read back
    EntityRef<BpTransform> refIn;
    Dia::Reflect::JsonReadArchive readAr(json);
    serialize(readAr, refIn, 0u);

    EXPECT_TRUE(refIn.IsValid());
    EXPECT_EQ(refIn.entity.GetIndex(), 42u);
    // Generation is 1 (placeholder convention)
    EXPECT_EQ(refIn.entity.GetGeneration(), 1u);
}

// ---------------------------------------------------------------------------
// Test 7: Serialize invalid EntityRef writes 0xFFFFFFFF and reads back invalid.
// ---------------------------------------------------------------------------
TEST(DiaEntityRef, SerializeInvalidEntityRefRoundTrip) {
    EntityRef<BpTransform> refOut;  // default — invalid

    Dia::Reflect::JsonWriteArchive writeAr;
    serialize(writeAr, refOut, 0u);
    Json::Value json = writeAr.GetRoot();

    ASSERT_TRUE(json.isMember("entityIndex"));
    EXPECT_EQ(json["entityIndex"].asUInt(), 0xFFFFFFFFu);

    // Read back — should restore to invalid
    EntityRef<BpTransform> refIn;
    refIn.entity = Entity(5u, 1u);  // pre-load a non-invalid value
    Dia::Reflect::JsonReadArchive readAr(json);
    serialize(readAr, refIn, 0u);

    EXPECT_FALSE(refIn.IsValid());
}
