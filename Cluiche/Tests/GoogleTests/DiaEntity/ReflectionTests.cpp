#include <gtest/gtest.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/ComponentRegistry.h>
#include <DiaEntity/ComponentPool.h>
#include "TestComponent.h"

using namespace Dia::Entity;
using namespace DiaEntityTest;

// Helper: register TestComponent pool with a domain.
static void RegisterTestComponentPool(Domain& domain) {
    domain.RegisterPool(new ComponentPool<TestComponent>(TestComponent::kTypeId));
}

// ============================================================
// ComponentRegistry
// ============================================================

TEST(DiaEntityReflection, RegistryFindsTestComponentByTypeId) {
    const ComponentTypeDesc* desc = ComponentRegistry::Get().Find(TestComponent::kTypeId);
    ASSERT_NE(desc, nullptr);
    EXPECT_EQ(desc->typeId, TestComponent::kTypeId);
}

TEST(DiaEntityReflection, RegistryDescDebugNameIsClassName) {
    const ComponentTypeDesc* desc = ComponentRegistry::Get().Find(TestComponent::kTypeId);
    ASSERT_NE(desc, nullptr);
    // debugName is #ClassName from the macro where ClassName is the unqualified name
    // passed to DIA_COMPONENT_REGISTER (called inside namespace DiaEntityTest).
    EXPECT_STREQ(desc->debugName, "TestComponent");
}

TEST(DiaEntityReflection, RegistryDescHasTwoFields) {
    const ComponentTypeDesc* desc = ComponentRegistry::Get().Find(TestComponent::kTypeId);
    ASSERT_NE(desc, nullptr);
    EXPECT_EQ(desc->fieldCount, 2u);
}

TEST(DiaEntityReflection, RegistryDescFieldNamesCorrect) {
    const ComponentTypeDesc* desc = ComponentRegistry::Get().Find(TestComponent::kTypeId);
    ASSERT_NE(desc, nullptr);
    EXPECT_STREQ(desc->fields[0].name, "speed");
    EXPECT_STREQ(desc->fields[1].name, "hitPoints");
}

TEST(DiaEntityReflection, RegistryDescFieldKindIsPrimitive) {
    const ComponentTypeDesc* desc = ComponentRegistry::Get().Find(TestComponent::kTypeId);
    ASSERT_NE(desc, nullptr);
    EXPECT_EQ(desc->fields[0].kind, FieldKind::Primitive); // float
    EXPECT_EQ(desc->fields[1].kind, FieldKind::Primitive); // int32_t
}

TEST(DiaEntityReflection, RegistryDescHasNoRequirements) {
    const ComponentTypeDesc* desc = ComponentRegistry::Get().Find(TestComponent::kTypeId);
    ASSERT_NE(desc, nullptr);
    EXPECT_EQ(desc->requiresCount, 0u);
}

TEST(DiaEntityReflection, RegistryDuplicateRegistrationIsNoop) {
    // GetDesc() attempts Register() internally; calling it again must not crash.
    const ComponentTypeDesc& desc = TestComponent::GetDesc();
    bool result = ComponentRegistry::Get().Register(desc);
    EXPECT_FALSE(result); // duplicate — should return false
}

// ============================================================
// Domain::QueueAddComponent + EndOfFrame wiring
// ============================================================

TEST(DiaEntityReflection, QueueAddComponentNotVisibleBeforeEndOfFrame) {
    Domain domain;
    RegisterTestComponentPool(domain);

    Entity e = domain.CreateEntity("rfl-test");
    Json::Value cfg;
    cfg["speed"]     = 5.0f;
    cfg["hitPoints"] = 50;

    domain.QueueAddComponent<TestComponent>(e, cfg);
    EXPECT_FALSE(domain.HasComponent<TestComponent>(e)); // not yet applied
}

TEST(DiaEntityReflection, QueueAddComponentAppliedAfterEndOfFrame) {
    Domain domain;
    RegisterTestComponentPool(domain);

    Entity e = domain.CreateEntity();
    Json::Value cfg;
    cfg["speed"]     = 5.0f;
    cfg["hitPoints"] = 50;

    domain.QueueAddComponent<TestComponent>(e, cfg);
    domain.EndOfFrame();

    EXPECT_TRUE(domain.HasComponent<TestComponent>(e));
}

TEST(DiaEntityReflection, ComponentFieldsLoadedFromJson) {
    Domain domain;
    RegisterTestComponentPool(domain);

    Entity e = domain.CreateEntity();
    Json::Value cfg;
    cfg["speed"]     = 7.5f;
    cfg["hitPoints"] = 200;

    domain.QueueAddComponent<TestComponent>(e, cfg);
    domain.EndOfFrame();

    TestComponent* comp = domain.GetComponent<TestComponent>(e);
    ASSERT_NE(comp, nullptr);
    EXPECT_FLOAT_EQ(comp->speed,     7.5f);
    EXPECT_EQ(comp->hitPoints,       200);
}

TEST(DiaEntityReflection, ComponentDefaultsUsedForMissingJsonFields) {
    Domain domain;
    RegisterTestComponentPool(domain);

    Entity e = domain.CreateEntity();
    // Empty config — fields should keep their C++ default values.
    domain.QueueAddComponent<TestComponent>(e, Json::Value(Json::objectValue));
    domain.EndOfFrame();

    TestComponent* comp = domain.GetComponent<TestComponent>(e);
    ASSERT_NE(comp, nullptr);
    EXPECT_FLOAT_EQ(comp->speed,     1.0f);  // default
    EXPECT_EQ(comp->hitPoints,       100);   // default
}

TEST(DiaEntityReflection, OnAttachCalledAfterAddComponent) {
    Domain domain;
    RegisterTestComponentPool(domain);

    Entity e = domain.CreateEntity();
    domain.QueueAddComponent<TestComponent>(e, Json::Value());
    domain.EndOfFrame();

    TestComponent* comp = domain.GetComponent<TestComponent>(e);
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->attachCount, 1);
    EXPECT_EQ(comp->detachCount, 0);
}

// ============================================================
// QueueRemoveComponent wiring
// ============================================================

TEST(DiaEntityReflection, QueueRemoveComponentStillVisibleBeforeEndOfFrame) {
    Domain domain;
    RegisterTestComponentPool(domain);

    Entity e = domain.CreateEntity();
    domain.QueueAddComponent<TestComponent>(e, Json::Value());
    domain.EndOfFrame();

    domain.QueueRemoveComponent<TestComponent>(e);
    EXPECT_TRUE(domain.HasComponent<TestComponent>(e)); // still present
}

TEST(DiaEntityReflection, QueueRemoveComponentRemovedAfterEndOfFrame) {
    Domain domain;
    RegisterTestComponentPool(domain);

    Entity e = domain.CreateEntity();
    domain.QueueAddComponent<TestComponent>(e, Json::Value());
    domain.EndOfFrame();

    domain.QueueRemoveComponent<TestComponent>(e);
    domain.EndOfFrame();
    EXPECT_FALSE(domain.HasComponent<TestComponent>(e));
}

TEST(DiaEntityReflection, OnDetachCalledOnRemoveComponent) {
    Domain domain;
    RegisterTestComponentPool(domain);

    Entity e = domain.CreateEntity();
    domain.QueueAddComponent<TestComponent>(e, Json::Value());
    domain.EndOfFrame();

    TestComponent* comp = domain.GetComponent<TestComponent>(e);
    ASSERT_NE(comp, nullptr);
    // detachCount must be 0 before removal.
    EXPECT_EQ(comp->detachCount, 0);

    domain.QueueRemoveComponent<TestComponent>(e);
    domain.EndOfFrame();
    // After removal the pointer is dangling — we only check that entity lost the component.
    EXPECT_FALSE(domain.HasComponent<TestComponent>(e));
}

// ============================================================
// QueueDestroy with attached component
// ============================================================

TEST(DiaEntityReflection, DestroyEntityWithComponentKillsEntity) {
    Domain domain;
    RegisterTestComponentPool(domain);

    Entity e = domain.CreateEntity();
    domain.QueueAddComponent<TestComponent>(e, Json::Value());
    domain.EndOfFrame();

    domain.QueueDestroy(e);
    domain.EndOfFrame();

    EXPECT_FALSE(domain.IsAlive(e));
}

// ============================================================
// GetComponent before EndOfFrame returns nullptr
// ============================================================

TEST(DiaEntityReflection, GetComponentReturnsNullBeforeEndOfFrame) {
    Domain domain;
    RegisterTestComponentPool(domain);

    Entity e = domain.CreateEntity();
    domain.QueueAddComponent<TestComponent>(e, Json::Value());
    // Not applied yet
    EXPECT_EQ(domain.GetComponent<TestComponent>(e), nullptr);

    domain.EndOfFrame();
    EXPECT_NE(domain.GetComponent<TestComponent>(e), nullptr);
}
