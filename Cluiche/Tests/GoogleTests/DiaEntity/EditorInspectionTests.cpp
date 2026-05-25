#include <gtest/gtest.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/ComponentPool.h>
#include <DiaEntity/IEntityInspectable.h>
#include "TestComponent.h"

using namespace Dia::Entity;
using namespace DiaEntityTest;

// Helper: create a Domain with TestComponent pool registered and one entity with the component.
static void RegisterTestPool(Domain& domain) {
    domain.RegisterPool(new ComponentPool<TestComponent>(TestComponent::kTypeId));
}

// =========================================================================
// 1. GetEntityCountReturnsCorrectCount
//    Create 3 entities; GetEntityCount() == 3.
// =========================================================================
TEST(DiaEntityEditorInspection, GetEntityCountReturnsCorrectCount) {
    Domain domain;

    Entity e1 = domain.CreateEntity("e1");
    Entity e2 = domain.CreateEntity("e2");
    Entity e3 = domain.CreateEntity("e3");
    (void)e1; (void)e2; (void)e3;

    EXPECT_EQ(domain.GetEntityCount(), 3u);
}

// =========================================================================
// 2. GetAllEntitiesReturnsAllLive
//    Create 2 entities; GetAllEntities returns both.
// =========================================================================
TEST(DiaEntityEditorInspection, GetAllEntitiesReturnsAllLive) {
    Domain domain;

    Entity e1 = domain.CreateEntity("e1");
    Entity e2 = domain.CreateEntity("e2");

    Dia::Core::Containers::DynamicArrayC<Entity, kMaxEntitiesPerDomain> out;
    domain.GetAllEntities(out);

    ASSERT_EQ(out.Size(), 2u);

    // Both returned entities must be valid and alive.
    bool foundE1 = false;
    bool foundE2 = false;
    for (uint32_t i = 0; i < out.Size(); ++i) {
        EXPECT_TRUE(domain.IsAlive(out[i]));
        if (out[i] == e1) foundE1 = true;
        if (out[i] == e2) foundE2 = true;
    }
    EXPECT_TRUE(foundE1);
    EXPECT_TRUE(foundE2);
}

// =========================================================================
// 3. GetComponentTypeIdsReturnsAttachedComponents
//    Add TestComponent; GetComponentTypeIds returns {kTypeId}.
// =========================================================================
TEST(DiaEntityEditorInspection, GetComponentTypeIdsReturnsAttachedComponents) {
    Domain domain;
    RegisterTestPool(domain);

    Entity e = domain.CreateEntity("e");
    domain.QueueAddComponent<TestComponent>(e, Json::Value());
    domain.EndOfFrame();

    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> typeIds;
    domain.GetComponentTypeIds(e, typeIds);

    ASSERT_EQ(typeIds.Size(), 1u);
    EXPECT_EQ(typeIds[0], TestComponent::kTypeId);
}

// =========================================================================
// 4. ReadFieldReturnsCorrectValue
//    Add TestComponent with speed=7.5; ReadField("speed") returns 7.5.
// =========================================================================
TEST(DiaEntityEditorInspection, ReadFieldReturnsCorrectValue) {
    Domain domain;
    RegisterTestPool(domain);

    Entity e = domain.CreateEntity("e");
    Json::Value cfg;
    cfg["speed"] = 7.5f;
    domain.QueueAddComponent<TestComponent>(e, cfg);
    domain.EndOfFrame();

    Json::Value result;
    bool ok = domain.ReadField(e, TestComponent::kTypeId, "speed", result);

    EXPECT_TRUE(ok);
    EXPECT_NEAR(result.asFloat(), 7.5f, 1e-5f);
}

// =========================================================================
// 5. ReadFieldReturnsFalseForUnknownField
//    ReadField("nonexistent") returns false.
// =========================================================================
TEST(DiaEntityEditorInspection, ReadFieldReturnsFalseForUnknownField) {
    Domain domain;
    RegisterTestPool(domain);

    Entity e = domain.CreateEntity("e");
    domain.QueueAddComponent<TestComponent>(e, Json::Value());
    domain.EndOfFrame();

    Json::Value result;
    bool ok = domain.ReadField(e, TestComponent::kTypeId, "nonexistent", result);

    EXPECT_FALSE(ok);
}

// =========================================================================
// 6. WriteFieldUpdatesValue
//    WriteField("speed", 9.9), then ReadField returns 9.9.
// =========================================================================
TEST(DiaEntityEditorInspection, WriteFieldUpdatesValue) {
    Domain domain;
    RegisterTestPool(domain);

    Entity e = domain.CreateEntity("e");
    domain.QueueAddComponent<TestComponent>(e, Json::Value());
    domain.EndOfFrame();

    Json::Value newVal(9.9f);
    bool writeOk = domain.WriteField(e, TestComponent::kTypeId, "speed", newVal);
    EXPECT_TRUE(writeOk);

    Json::Value readBack;
    bool readOk = domain.ReadField(e, TestComponent::kTypeId, "speed", readBack);
    EXPECT_TRUE(readOk);
    EXPECT_NEAR(readBack.asFloat(), 9.9f, 1e-4f);
}

// =========================================================================
// 7. WriteFieldReturnsFalseForTypeMismatch
//    WriteField("speed", Json::Value("not-a-float")) returns false; value unchanged.
// =========================================================================
TEST(DiaEntityEditorInspection, WriteFieldReturnsFalseForTypeMismatch) {
    Domain domain;
    RegisterTestPool(domain);

    Entity e = domain.CreateEntity("e");
    Json::Value initCfg;
    initCfg["speed"] = 3.0f;
    domain.QueueAddComponent<TestComponent>(e, initCfg);
    domain.EndOfFrame();

    Json::Value badVal("not-a-float");
    bool writeOk = domain.WriteField(e, TestComponent::kTypeId, "speed", badVal);
    EXPECT_FALSE(writeOk);

    // Value must remain unchanged at 3.0.
    Json::Value readBack;
    bool readOk = domain.ReadField(e, TestComponent::kTypeId, "speed", readBack);
    EXPECT_TRUE(readOk);
    EXPECT_NEAR(readBack.asFloat(), 3.0f, 1e-5f);
}

// =========================================================================
// 8. WriteFieldReturnsFalseForUnknownField
//    WriteField("nonexistent", 1.0) returns false.
// =========================================================================
TEST(DiaEntityEditorInspection, WriteFieldReturnsFalseForUnknownField) {
    Domain domain;
    RegisterTestPool(domain);

    Entity e = domain.CreateEntity("e");
    domain.QueueAddComponent<TestComponent>(e, Json::Value());
    domain.EndOfFrame();

    Json::Value val(1.0);
    bool writeOk = domain.WriteField(e, TestComponent::kTypeId, "nonexistent", val);
    EXPECT_FALSE(writeOk);
}

// =========================================================================
// 9. WriteFieldReturnsFalseForDeadEntity
//    WriteField on Entity::Invalid() returns false.
// =========================================================================
TEST(DiaEntityEditorInspection, WriteFieldReturnsFalseForDeadEntity) {
    Domain domain;
    RegisterTestPool(domain);

    Entity invalid = Entity::Invalid();
    Json::Value val(5.0f);
    bool writeOk = domain.WriteField(invalid, TestComponent::kTypeId, "speed", val);
    EXPECT_FALSE(writeOk);
}

// =========================================================================
// 10. GetEntityCountAfterDestroyDecrements
//     Create then destroy an entity; count decrements.
// =========================================================================
TEST(DiaEntityEditorInspection, GetEntityCountAfterDestroyDecrements) {
    Domain domain;

    Entity e1 = domain.CreateEntity("e1");
    Entity e2 = domain.CreateEntity("e2");
    (void)e1;

    ASSERT_EQ(domain.GetEntityCount(), 2u);

    domain.QueueDestroy(e2);
    domain.EndOfFrame();

    EXPECT_EQ(domain.GetEntityCount(), 1u);
}

// =========================================================================
// 11. ReadFieldReturnsFalseForDeadEntity
//     ReadField on a destroyed entity returns false.
// =========================================================================
TEST(DiaEntityEditorInspection, ReadFieldReturnsFalseForDeadEntity) {
    Domain domain;
    RegisterTestPool(domain);

    Entity e = domain.CreateEntity("e");
    domain.QueueAddComponent<TestComponent>(e, Json::Value());
    domain.EndOfFrame();

    domain.QueueDestroy(e);
    domain.EndOfFrame();

    Json::Value result;
    bool ok = domain.ReadField(e, TestComponent::kTypeId, "speed", result);
    EXPECT_FALSE(ok);
}

// =========================================================================
// 12. GetComponentTypeIdsEmptyForEntityWithNoComponents
//     An entity with no components yields an empty list.
// =========================================================================
TEST(DiaEntityEditorInspection, GetComponentTypeIdsEmptyForEntityWithNoComponents) {
    Domain domain;
    RegisterTestPool(domain);

    Entity e = domain.CreateEntity("e");

    Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32> typeIds;
    domain.GetComponentTypeIds(e, typeIds);

    EXPECT_EQ(typeIds.Size(), 0u);
}

// =========================================================================
// 13. IEntityInspectablePointerAccessible
//     Domain can be accessed through IEntityInspectable* interface pointer.
// =========================================================================
TEST(DiaEntityEditorInspection, IEntityInspectablePointerAccessible) {
    Domain domain;
    RegisterTestPool(domain);

    Entity e = domain.CreateEntity("e");
    domain.QueueAddComponent<TestComponent>(e, Json::Value());
    domain.EndOfFrame();

    IEntityInspectable* inspectable = &domain;
    EXPECT_EQ(inspectable->GetEntityCount(), 1u);
}
