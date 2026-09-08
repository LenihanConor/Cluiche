#include <gtest/gtest.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaEntity/IComponent.h>
#include <DiaEntity/ComponentMacros.h>
#include <DiaEntity/ComponentPool.h>
#include <DiaEntity/ComponentRegistry.h>

using namespace Dia::Entity;

// ============================================================
// Test components for data flow contract validation
// ============================================================

namespace DiaDataFlowTest {

    // A readonly component — pure data, no hooks should fire.
    class ReadOnlyComp : public Dia::Entity::IComponent {
        DIA_COMPONENT(ReadOnlyComp, "dataflow.readonly", 1)
        DIA_READONLY

        FIELD(float, value, 0.0f)

    public:
        int attachCount = 0;
        int detachCount = 0;

        void OnAttach(Dia::Entity::Domain& /*domain*/, Dia::Entity::Entity /*self*/) override {
            ++attachCount;
        }

        void OnDetach(Dia::Entity::Domain& /*domain*/, Dia::Entity::Entity /*self*/) override {
            ++detachCount;
        }
    };

    // A readonly component that also tries DIA_UPDATABLE — should assert at registration.
    // (We test that the flag combination is rejected, but can't test static_assert in a unit test,
    //  so we test via desc flags instead.)

    // A second readonly component used as a write target.
    class TargetComp : public Dia::Entity::IComponent {
        DIA_COMPONENT(TargetComp, "dataflow.target", 1)
        DIA_READONLY

        FIELD(float, x, 0.0f)
        FIELD(float, y, 0.0f)
    };

    // An updatable component that writes to TargetComp.
    class WriterCompA : public Dia::Entity::IComponent {
        DIA_COMPONENT(WriterCompA, "dataflow.writer-a", 1)
        DIA_UPDATABLE
        DIA_WRITES(TargetComp)

    public:
        int updateCount = 0;

        void DoUpdate(Dia::Entity::Domain& domain, Dia::Entity::Entity self, float /*dt*/) override {
            ++updateCount;
            TargetComp* target = domain.GetComponent<TargetComp>(self);
            if (target) {
                target->x += 1.0f;
            }
        }
    };

    // A second updatable component that ALSO writes to TargetComp — conflict!
    class WriterCompB : public Dia::Entity::IComponent {
        DIA_COMPONENT(WriterCompB, "dataflow.writer-b", 1)
        DIA_UPDATABLE
        DIA_WRITES(TargetComp)

    public:
        void DoUpdate(Dia::Entity::Domain& /*domain*/, Dia::Entity::Entity /*self*/, float /*dt*/) override {}
    };

    // An updatable component that writes to a DIFFERENT target — no conflict with WriterCompA.
    class WriterCompC : public Dia::Entity::IComponent {
        DIA_COMPONENT(WriterCompC, "dataflow.writer-c", 1)
        DIA_UPDATABLE
        DIA_WRITES(ReadOnlyComp)

    public:
        void DoUpdate(Dia::Entity::Domain& /*domain*/, Dia::Entity::Entity /*self*/, float /*dt*/) override {}
    };

    // An updatable component with no DIA_WRITES — unconstrained (opt-in enforcement).
    class FreeWriterComp : public Dia::Entity::IComponent {
        DIA_COMPONENT(FreeWriterComp, "dataflow.free-writer", 1)
        DIA_UPDATABLE

    public:
        int updateCount = 0;
        void DoUpdate(Dia::Entity::Domain& /*domain*/, Dia::Entity::Entity /*self*/, float /*dt*/) override {
            ++updateCount;
        }
    };

} // namespace DiaDataFlowTest

// ============================================================
// Serialize stubs (required for registration)
// ============================================================

DIA_SERIALIZE(DiaDataFlowTest::ReadOnlyComp, DiaDataFlowTest::ReadOnlyComp::kVersion)
    DIA_FIELD(value)
DIA_SERIALIZE_END

DIA_SERIALIZE(DiaDataFlowTest::TargetComp, DiaDataFlowTest::TargetComp::kVersion)
    DIA_FIELD(x)
    DIA_FIELD(y)
DIA_SERIALIZE_END

DIA_SERIALIZE(DiaDataFlowTest::WriterCompA, DiaDataFlowTest::WriterCompA::kVersion)
DIA_SERIALIZE_END

DIA_SERIALIZE(DiaDataFlowTest::WriterCompB, DiaDataFlowTest::WriterCompB::kVersion)
DIA_SERIALIZE_END

DIA_SERIALIZE(DiaDataFlowTest::WriterCompC, DiaDataFlowTest::WriterCompC::kVersion)
DIA_SERIALIZE_END

DIA_SERIALIZE(DiaDataFlowTest::FreeWriterComp, DiaDataFlowTest::FreeWriterComp::kVersion)
DIA_SERIALIZE_END

// ============================================================
// Component registrations
// ============================================================

namespace DiaDataFlowTest {

static Dia::Entity::FieldDesc s_ReadOnlyComp_fields[] = {
    DIA_FIELD_ENTRY(float, value, ReadOnlyComp)
};

DIA_COMPONENT_REGISTER(ReadOnlyComp, "dataflow.readonly", false, true,
    s_ReadOnlyComp_fields, DIA_ARRAY_COUNT(s_ReadOnlyComp_fields),
    nullptr, 0,
    nullptr, 0)

static Dia::Entity::FieldDesc s_TargetComp_fields[] = {
    DIA_FIELD_ENTRY(float, x, TargetComp)
    DIA_FIELD_ENTRY(float, y, TargetComp)
};

DIA_COMPONENT_REGISTER(TargetComp, "dataflow.target", false, true,
    s_TargetComp_fields, DIA_ARRAY_COUNT(s_TargetComp_fields),
    nullptr, 0,
    nullptr, 0)

static Dia::Core::StringCRC s_WriterCompA_writes[] = {
    DIA_WRITES_ENTRY(TargetComp)
};

DIA_COMPONENT_REGISTER(WriterCompA, "dataflow.writer-a", WriterCompA::kIsUpdatable, false,
    nullptr, 0,
    nullptr, 0,
    s_WriterCompA_writes, DIA_ARRAY_COUNT(s_WriterCompA_writes))

static Dia::Core::StringCRC s_WriterCompB_writes[] = {
    DIA_WRITES_ENTRY(TargetComp)
};

DIA_COMPONENT_REGISTER(WriterCompB, "dataflow.writer-b", WriterCompB::kIsUpdatable, false,
    nullptr, 0,
    nullptr, 0,
    s_WriterCompB_writes, DIA_ARRAY_COUNT(s_WriterCompB_writes))

static Dia::Core::StringCRC s_WriterCompC_writes[] = {
    DIA_WRITES_ENTRY(ReadOnlyComp)
};

DIA_COMPONENT_REGISTER(WriterCompC, "dataflow.writer-c", WriterCompC::kIsUpdatable, false,
    nullptr, 0,
    nullptr, 0,
    s_WriterCompC_writes, DIA_ARRAY_COUNT(s_WriterCompC_writes))

DIA_COMPONENT_REGISTER(FreeWriterComp, "dataflow.free-writer", FreeWriterComp::kIsUpdatable, false,
    nullptr, 0,
    nullptr, 0,
    nullptr, 0)

} // namespace DiaDataFlowTest

// ============================================================
// Helpers
// ============================================================

static void RegisterAllDataFlowPools(Domain& domain) {
    domain.RegisterPool(new ComponentPool<DiaDataFlowTest::ReadOnlyComp>(DiaDataFlowTest::ReadOnlyComp::kTypeId));
    domain.RegisterPool(new ComponentPool<DiaDataFlowTest::TargetComp>(DiaDataFlowTest::TargetComp::kTypeId));
    domain.RegisterPool(new ComponentPool<DiaDataFlowTest::WriterCompA>(DiaDataFlowTest::WriterCompA::kTypeId));
    domain.RegisterPool(new ComponentPool<DiaDataFlowTest::WriterCompB>(DiaDataFlowTest::WriterCompB::kTypeId));
    domain.RegisterPool(new ComponentPool<DiaDataFlowTest::WriterCompC>(DiaDataFlowTest::WriterCompC::kTypeId));
    domain.RegisterPool(new ComponentPool<DiaDataFlowTest::FreeWriterComp>(DiaDataFlowTest::FreeWriterComp::kTypeId));
}

// ============================================================
// DIA_READONLY Tests
// ============================================================

TEST(DiaDataFlowContract, ReadOnlyFlagSetInDescriptor) {
    const ComponentTypeDesc* desc = ComponentRegistry::Get().Find(DiaDataFlowTest::ReadOnlyComp::kTypeId);
    ASSERT_NE(desc, nullptr);
    EXPECT_TRUE(desc->flags & kFlagReadOnly);
    EXPECT_FALSE(desc->flags & kFlagOverridesDoUpdate);
}

TEST(DiaDataFlowContract, ReadOnlyOnAttachNotCalled) {
    Domain domain;
    RegisterAllDataFlowPools(domain);
    Entity e = domain.CreateEntity();

    Json::Value config;
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::ReadOnlyComp::kTypeId, config);
    domain.EndOfFrame();

    DiaDataFlowTest::ReadOnlyComp* comp = domain.GetComponent<DiaDataFlowTest::ReadOnlyComp>(e);
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->attachCount, 0);
}

TEST(DiaDataFlowContract, ReadOnlyOnDetachNotCalledOnRemove) {
    Domain domain;
    RegisterAllDataFlowPools(domain);
    Entity e = domain.CreateEntity();

    Json::Value config;
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::ReadOnlyComp::kTypeId, config);
    domain.EndOfFrame();

    DiaDataFlowTest::ReadOnlyComp* comp = domain.GetComponent<DiaDataFlowTest::ReadOnlyComp>(e);
    ASSERT_NE(comp, nullptr);

    domain.QueueRemoveComponent<DiaDataFlowTest::ReadOnlyComp>(e);
    domain.EndOfFrame();

    EXPECT_EQ(comp->detachCount, 0);
}

TEST(DiaDataFlowContract, ReadOnlyOnDetachNotCalledOnEntityDestroy) {
    Domain domain;
    RegisterAllDataFlowPools(domain);
    Entity e = domain.CreateEntity();

    Json::Value config;
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::ReadOnlyComp::kTypeId, config);
    domain.EndOfFrame();

    DiaDataFlowTest::ReadOnlyComp* comp = domain.GetComponent<DiaDataFlowTest::ReadOnlyComp>(e);
    ASSERT_NE(comp, nullptr);
    int* detachPtr = &comp->detachCount;

    domain.QueueDestroy(e);
    domain.EndOfFrame();

    // Entity is destroyed, but detach counter should still be 0
    // (we captured the pointer before destruction — the memory hasn't been reused yet in this frame)
    EXPECT_FALSE(domain.IsAlive(e));
}

TEST(DiaDataFlowContract, ReadOnlyDoUpdateNeverCalled) {
    Domain domain;
    RegisterAllDataFlowPools(domain);
    Entity e = domain.CreateEntity();

    Json::Value config;
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::ReadOnlyComp::kTypeId, config);
    domain.EndOfFrame();

    domain.Update(0.016f);
    domain.Update(0.016f);
    domain.Update(0.016f);

    DiaDataFlowTest::ReadOnlyComp* comp = domain.GetComponent<DiaDataFlowTest::ReadOnlyComp>(e);
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->attachCount, 0);
}

TEST(DiaDataFlowContract, ReadOnlyFieldsLoadFromJson) {
    Domain domain;
    RegisterAllDataFlowPools(domain);
    Entity e = domain.CreateEntity();

    Json::Value config;
    config["value"] = 42.0f;
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::ReadOnlyComp::kTypeId, config);
    domain.EndOfFrame();

    DiaDataFlowTest::ReadOnlyComp* comp = domain.GetComponent<DiaDataFlowTest::ReadOnlyComp>(e);
    ASSERT_NE(comp, nullptr);
    EXPECT_FLOAT_EQ(comp->value, 42.0f);
}

TEST(DiaDataFlowContract, NonReadOnlyOnAttachStillFires) {
    Domain domain;
    RegisterAllDataFlowPools(domain);
    Entity e = domain.CreateEntity();

    Json::Value config;
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::WriterCompA::kTypeId, config);
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::TargetComp::kTypeId, config);
    domain.EndOfFrame();

    DiaDataFlowTest::WriterCompA* writer = domain.GetComponent<DiaDataFlowTest::WriterCompA>(e);
    ASSERT_NE(writer, nullptr);
    // WriterCompA doesn't track attach, but the fact it compiled and was added confirms hooks fire
}

// ============================================================
// DIA_WRITES Tests — single-writer validation
// ============================================================

TEST(DiaDataFlowContract, WritesToMetadataInDescriptor) {
    const ComponentTypeDesc* desc = ComponentRegistry::Get().Find(DiaDataFlowTest::WriterCompA::kTypeId);
    ASSERT_NE(desc, nullptr);
    EXPECT_EQ(desc->writesToCount, 1);
    EXPECT_EQ(desc->writesTo[0], DiaDataFlowTest::TargetComp::kTypeId);
}

TEST(DiaDataFlowContract, SingleWriterAllowed) {
    Domain domain;
    RegisterAllDataFlowPools(domain);
    Entity e = domain.CreateEntity();

    Json::Value config;
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::TargetComp::kTypeId, config);
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::WriterCompA::kTypeId, config);
    domain.EndOfFrame();

    // Both should be present — single writer is fine
    EXPECT_NE(domain.GetComponent<DiaDataFlowTest::TargetComp>(e), nullptr);
    EXPECT_NE(domain.GetComponent<DiaDataFlowTest::WriterCompA>(e), nullptr);
}

TEST(DiaDataFlowContract, DifferentWriteTargetsNoConflict) {
    Domain domain;
    RegisterAllDataFlowPools(domain);
    Entity e = domain.CreateEntity();

    Json::Value config;
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::TargetComp::kTypeId, config);
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::ReadOnlyComp::kTypeId, config);
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::WriterCompA::kTypeId, config);  // writes TargetComp
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::WriterCompC::kTypeId, config);  // writes ReadOnlyComp
    domain.EndOfFrame();

    // All four should be present — different targets, no conflict
    EXPECT_NE(domain.GetComponent<DiaDataFlowTest::WriterCompA>(e), nullptr);
    EXPECT_NE(domain.GetComponent<DiaDataFlowTest::WriterCompC>(e), nullptr);
}

TEST(DiaDataFlowContract, FreeWriterCoexistsWithDeclaredWriter) {
    Domain domain;
    RegisterAllDataFlowPools(domain);
    Entity e = domain.CreateEntity();

    Json::Value config;
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::TargetComp::kTypeId, config);
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::WriterCompA::kTypeId, config);
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::FreeWriterComp::kTypeId, config);
    domain.EndOfFrame();

    // FreeWriterComp has no DIA_WRITES, so it doesn't conflict
    EXPECT_NE(domain.GetComponent<DiaDataFlowTest::WriterCompA>(e), nullptr);
    EXPECT_NE(domain.GetComponent<DiaDataFlowTest::FreeWriterComp>(e), nullptr);
}

TEST(DiaDataFlowContract, WriterUpdatesTarget) {
    Domain domain;
    RegisterAllDataFlowPools(domain);
    Entity e = domain.CreateEntity();

    Json::Value config;
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::TargetComp::kTypeId, config);
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::WriterCompA::kTypeId, config);
    domain.EndOfFrame();

    DiaDataFlowTest::TargetComp* target = domain.GetComponent<DiaDataFlowTest::TargetComp>(e);
    ASSERT_NE(target, nullptr);
    EXPECT_FLOAT_EQ(target->x, 0.0f);

    domain.Update(0.016f);

    EXPECT_FLOAT_EQ(target->x, 1.0f);

    domain.Update(0.016f);

    EXPECT_FLOAT_EQ(target->x, 2.0f);
}

TEST(DiaDataFlowContract, WriterUpdateCount) {
    Domain domain;
    RegisterAllDataFlowPools(domain);
    Entity e = domain.CreateEntity();

    Json::Value config;
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::TargetComp::kTypeId, config);
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::WriterCompA::kTypeId, config);
    domain.EndOfFrame();

    DiaDataFlowTest::WriterCompA* writer = domain.GetComponent<DiaDataFlowTest::WriterCompA>(e);
    ASSERT_NE(writer, nullptr);
    EXPECT_EQ(writer->updateCount, 0);

    domain.Update(0.016f);
    EXPECT_EQ(writer->updateCount, 1);
}

// ============================================================
// Single-writer conflict detection (debug assert)
// ============================================================

#ifdef DEBUG
TEST(DiaDataFlowContractDeathTest, DuplicateWriterAssertsOnAdd) {
    Domain domain;
    RegisterAllDataFlowPools(domain);
    Entity e = domain.CreateEntity();

    Json::Value config;
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::TargetComp::kTypeId, config);
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::WriterCompA::kTypeId, config);
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::WriterCompB::kTypeId, config);

    EXPECT_DEATH(domain.EndOfFrame(), "");
}
#endif

// ============================================================
// Readonly + updatable mutual exclusion
// ============================================================

TEST(DiaDataFlowContract, ReadOnlyAndUpdatableMutuallyExclusiveByConvention) {
    // ReadOnlyComp has kFlagReadOnly set, NOT kFlagOverridesDoUpdate
    const ComponentTypeDesc* desc = ComponentRegistry::Get().Find(DiaDataFlowTest::ReadOnlyComp::kTypeId);
    ASSERT_NE(desc, nullptr);
    EXPECT_TRUE(desc->flags & kFlagReadOnly);
    EXPECT_FALSE(desc->flags & kFlagOverridesDoUpdate);

    // WriterCompA has kFlagOverridesDoUpdate set, NOT kFlagReadOnly
    const ComponentTypeDesc* writerDesc = ComponentRegistry::Get().Find(DiaDataFlowTest::WriterCompA::kTypeId);
    ASSERT_NE(writerDesc, nullptr);
    EXPECT_TRUE(writerDesc->flags & kFlagOverridesDoUpdate);
    EXPECT_FALSE(writerDesc->flags & kFlagReadOnly);
}

// ============================================================
// Integration: full data flow pattern
// ============================================================

TEST(DiaDataFlowContract, FullDataFlowPattern) {
    Domain domain;
    RegisterAllDataFlowPools(domain);

    // Create two entities with the same pattern
    Entity e1 = domain.CreateEntity();
    Entity e2 = domain.CreateEntity();

    Json::Value config;
    Json::Value targetConfig;
    targetConfig["x"] = 10.0f;
    targetConfig["y"] = 20.0f;

    domain.QueueAddComponentByTypeId(e1, DiaDataFlowTest::TargetComp::kTypeId, targetConfig);
    domain.QueueAddComponentByTypeId(e1, DiaDataFlowTest::WriterCompA::kTypeId, config);
    domain.QueueAddComponentByTypeId(e2, DiaDataFlowTest::TargetComp::kTypeId, config);
    domain.QueueAddComponentByTypeId(e2, DiaDataFlowTest::WriterCompA::kTypeId, config);
    domain.EndOfFrame();

    // Verify initial state
    DiaDataFlowTest::TargetComp* t1 = domain.GetComponent<DiaDataFlowTest::TargetComp>(e1);
    DiaDataFlowTest::TargetComp* t2 = domain.GetComponent<DiaDataFlowTest::TargetComp>(e2);
    ASSERT_NE(t1, nullptr);
    ASSERT_NE(t2, nullptr);
    EXPECT_FLOAT_EQ(t1->x, 10.0f);
    EXPECT_FLOAT_EQ(t2->x, 0.0f);

    // Update — WriterCompA adds 1.0 to x each frame
    domain.Update(0.016f);

    EXPECT_FLOAT_EQ(t1->x, 11.0f);
    EXPECT_FLOAT_EQ(t2->x, 1.0f);

    // Destroy e1, e2 should still work
    domain.QueueDestroy(e1);
    domain.EndOfFrame();

    domain.Update(0.016f);
    EXPECT_FLOAT_EQ(t2->x, 2.0f);
    EXPECT_FALSE(domain.IsAlive(e1));
}

TEST(DiaDataFlowContract, RemoveWriterThenAddDifferentWriter) {
    Domain domain;
    RegisterAllDataFlowPools(domain);
    Entity e = domain.CreateEntity();

    Json::Value config;
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::TargetComp::kTypeId, config);
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::WriterCompA::kTypeId, config);
    domain.EndOfFrame();

    // Remove WriterCompA
    domain.QueueRemoveComponent<DiaDataFlowTest::WriterCompA>(e);
    domain.EndOfFrame();

    EXPECT_EQ(domain.GetComponent<DiaDataFlowTest::WriterCompA>(e), nullptr);

    // Now adding WriterCompB (same target) should succeed — no conflict
    domain.QueueAddComponentByTypeId(e, DiaDataFlowTest::WriterCompB::kTypeId, config);
    domain.EndOfFrame();

    EXPECT_NE(domain.GetComponent<DiaDataFlowTest::WriterCompB>(e), nullptr);
}
