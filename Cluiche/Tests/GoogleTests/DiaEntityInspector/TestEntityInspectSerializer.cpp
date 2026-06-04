// Unit tests for EntityInspectSerializer.
//
// Uses a real Domain instance (which implements IEntityInspectable) so that
// no mocking is required. Components are registered using the same
// DIA_COMPONENT / DIA_SERIALIZE / DIA_COMPONENT_REGISTER pattern as the
// rest of the diaentitytemplate test suite.

#include <gtest/gtest.h>
#include <DiaEntityInspector/EntityInspectSerializer.h>
#include <diaentitytemplate/Domain.h>
#include <diaentitytemplate/ComponentPool.h>
#include <diaentitytemplate/ComponentTypeDesc.h>
#include <diaentitytemplate/ComponentRegistry.h>
#include <diaentitytemplate/IComponent.h>
#include <diaentitytemplate/ComponentMacros.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

using namespace Dia::Entity;
using namespace Dia::EntityInspector;

// ===========================================================================
// Test component local to this translation unit
// ===========================================================================

namespace DiaEntityInspectorSerializerTest {

    class InspComp : public Dia::Entity::IComponent {
        DIA_COMPONENT(InspComp, "insp-comp", 1)
        FIELD(float,   x, 0.0f)
        FIELD(float,   y, 0.0f)
        FIELD(int32_t, hp, 10)
    };

} // namespace DiaEntityInspectorSerializerTest

DIA_SERIALIZE(DiaEntityInspectorSerializerTest::InspComp,
              DiaEntityInspectorSerializerTest::InspComp::kVersion)
    DIA_FIELD(x)
    DIA_FIELD(y)
    DIA_FIELD(hp)
DIA_SERIALIZE_END

namespace DiaEntityInspectorSerializerTest {

static Dia::Entity::FieldDesc s_InspComp_fields[] = {
    DIA_FIELD_ENTRY(float,   x,  InspComp)
    DIA_FIELD_ENTRY(float,   y,  InspComp)
    DIA_FIELD_ENTRY(int32_t, hp, InspComp)
};

DIA_COMPONENT_REGISTER(InspComp, "insp-comp", false, false,
    s_InspComp_fields, DIA_ARRAY_COUNT(s_InspComp_fields),
    nullptr, 0,
    nullptr, 0)

} // namespace DiaEntityInspectorSerializerTest

// ===========================================================================
// Fixture
// ===========================================================================

class EntityInspectSerializerTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mDomain.RegisterPool(
            new ComponentPool<DiaEntityInspectorSerializerTest::InspComp>(
                DiaEntityInspectorSerializerTest::InspComp::kTypeId));
    }

    // Create a live entity and flush the mutation queue.
    Entity MakeLiveEntity(const char* name = "test_entity")
    {
        Entity e = mDomain.CreateEntity(name);
        return e;
    }

    // Create a live entity with InspComp attached, flush mutations.
    Entity MakeLiveEntityWithComp(const char* name = "test_entity")
    {
        Entity e = mDomain.CreateEntity(name);
        mDomain.QueueAddComponent<DiaEntityInspectorSerializerTest::InspComp>(e, Json::Value());
        mDomain.EndOfFrame();
        return e;
    }

    Domain mDomain;
};

// ===========================================================================
// Test 1: SerializeEntityInspect_InvalidEntity_ReturnsEmptyObject
// Passing Entity::Invalid() must return a JSON object with no members.
// ===========================================================================

TEST_F(EntityInspectSerializerTest, SerializeEntityInspect_InvalidEntity_ReturnsEmptyObject)
{
    Json::Value result = SerializeEntityInspect(mDomain, Entity::Invalid());
    EXPECT_TRUE(result.isObject());
    EXPECT_EQ(result.size(), 0u);
}

// ===========================================================================
// Test 2: SerializeEntityInspect_LiveEntity_HasEntitySection
// A live entity's inspect payload must contain an "entity" member.
// ===========================================================================

TEST_F(EntityInspectSerializerTest, SerializeEntityInspect_LiveEntity_HasEntitySection)
{
    Entity e = MakeLiveEntity();
    Json::Value result = SerializeEntityInspect(mDomain, e);
    EXPECT_TRUE(result.isMember("entity"));
    EXPECT_TRUE(result["entity"].isObject());
}

// ===========================================================================
// Test 3: SerializeEntityInspect_LiveEntity_IndexMatchesEntity
// The "entity.index" in the JSON must equal the entity's actual index.
// ===========================================================================

TEST_F(EntityInspectSerializerTest, SerializeEntityInspect_LiveEntity_IndexMatchesEntity)
{
    Entity e = MakeLiveEntity();
    Json::Value result = SerializeEntityInspect(mDomain, e);
    ASSERT_TRUE(result.isMember("entity"));
    EXPECT_EQ(static_cast<uint32_t>(result["entity"]["index"].asInt()), e.GetIndex());
}

// ===========================================================================
// Test 4: SerializeEntityInspect_LiveEntity_HasComponentsArray
// The payload must contain a "components" array.
// ===========================================================================

TEST_F(EntityInspectSerializerTest, SerializeEntityInspect_LiveEntity_HasComponentsArray)
{
    Entity e = MakeLiveEntity();
    Json::Value result = SerializeEntityInspect(mDomain, e);
    EXPECT_TRUE(result.isMember("components"));
    EXPECT_TRUE(result["components"].isArray());
}

// ===========================================================================
// Test 5: SerializeEntityInspect_LiveEntity_HasHierarchySection
// The payload must contain a "hierarchy" member.
// ===========================================================================

TEST_F(EntityInspectSerializerTest, SerializeEntityInspect_LiveEntity_HasHierarchySection)
{
    Entity e = MakeLiveEntity();
    Json::Value result = SerializeEntityInspect(mDomain, e);
    EXPECT_TRUE(result.isMember("hierarchy"));
    EXPECT_TRUE(result["hierarchy"].isObject());
}

// ===========================================================================
// Test 6: SerializeEntityInspect_LiveEntity_HasQueriesArray
// The payload must contain a "queries" array.
// ===========================================================================

TEST_F(EntityInspectSerializerTest, SerializeEntityInspect_LiveEntity_HasQueriesArray)
{
    Entity e = MakeLiveEntity();
    Json::Value result = SerializeEntityInspect(mDomain, e);
    EXPECT_TRUE(result.isMember("queries"));
    EXPECT_TRUE(result["queries"].isArray());
}

// ===========================================================================
// Test 7: SerializeEntityInspect_LiveEntity_HasMailboxLogArray
// The payload must contain a "mailbox_log" array.
// ===========================================================================

TEST_F(EntityInspectSerializerTest, SerializeEntityInspect_LiveEntity_HasMailboxLogArray)
{
    Entity e = MakeLiveEntity();
    Json::Value result = SerializeEntityInspect(mDomain, e);
    EXPECT_TRUE(result.isMember("mailbox_log"));
    EXPECT_TRUE(result["mailbox_log"].isArray());
}

// ===========================================================================
// Test 8: SerializeEntityList_EmptyDomain_ReturnsEmptyArray
// A domain with no entities must produce an empty JSON array.
// ===========================================================================

TEST_F(EntityInspectSerializerTest, SerializeEntityList_EmptyDomain_ReturnsEmptyArray)
{
    Json::Value result = SerializeEntityList(mDomain);
    EXPECT_TRUE(result.isArray());
    EXPECT_EQ(result.size(), 0u);
}

// ===========================================================================
// Test 9: SerializeEntityList_OneEntity_ReturnsList
// Creating 1 entity must produce an array with exactly 1 entry.
// ===========================================================================

TEST_F(EntityInspectSerializerTest, SerializeEntityList_OneEntity_ReturnsList)
{
    MakeLiveEntity("solo");
    Json::Value result = SerializeEntityList(mDomain);
    EXPECT_EQ(result.size(), 1u);
}

// ===========================================================================
// Test 10: SerializeEntityList_MultipleEntities_AllIncluded
// Creating 3 entities must produce an array with exactly 3 entries.
// ===========================================================================

TEST_F(EntityInspectSerializerTest, SerializeEntityList_MultipleEntities_AllIncluded)
{
    MakeLiveEntity("e1");
    MakeLiveEntity("e2");
    MakeLiveEntity("e3");
    Json::Value result = SerializeEntityList(mDomain);
    EXPECT_EQ(result.size(), 3u);
}

// ===========================================================================
// Test 11: SerializeEntityList_Entry_HasRequiredFields
// Each entry must have "index", "gen", and "component_count".
// ===========================================================================

TEST_F(EntityInspectSerializerTest, SerializeEntityList_Entry_HasRequiredFields)
{
    MakeLiveEntityWithComp("entity_with_comp");
    Json::Value result = SerializeEntityList(mDomain);
    ASSERT_EQ(result.size(), 1u);

    const Json::Value& entry = result[0];
    EXPECT_TRUE(entry.isMember("index"));
    EXPECT_TRUE(entry.isMember("gen"));
    EXPECT_TRUE(entry.isMember("component_count"));

    // The entity has 1 component.
    EXPECT_EQ(entry["component_count"].asInt(), 1);
}

// ===========================================================================
// Test 12: GetQueryCount_ReflectedInSerializedPayload
// After a Query<InspComp>() call and EndOfFrame, the queries array
// must be non-empty for an entity that satisfies the query signature.
// ===========================================================================

TEST_F(EntityInspectSerializerTest, GetQueryCount_ReflectedInSerializedPayload)
{
    Entity e = MakeLiveEntityWithComp("queried");

    // Issue a typed query — this registers the cache entry in the domain.
    auto view = mDomain.Query<DiaEntityInspectorSerializerTest::InspComp>();
    (void)view;

    // EndOfFrame rebuilds dirty caches.
    mDomain.EndOfFrame();

    // Verify the domain now reports at least one query.
    ASSERT_GE(mDomain.GetQueryCount(), 1u);

    Json::Value result = SerializeEntityInspect(mDomain, e);
    ASSERT_TRUE(result.isMember("queries"));
    // At least one query entry should appear in the serialized payload.
    EXPECT_GE(result["queries"].size(), 1u);
}

// ===========================================================================
// Additional: components array is populated for entity with component
// ===========================================================================

TEST_F(EntityInspectSerializerTest, SerializeEntityInspect_WithComponent_ComponentsNonEmpty)
{
    Entity e = MakeLiveEntityWithComp();
    Json::Value result = SerializeEntityInspect(mDomain, e);
    ASSERT_TRUE(result.isMember("components"));
    EXPECT_GE(result["components"].size(), 1u);
}

// ===========================================================================
// Additional: components array is empty for entity with no components
// ===========================================================================

TEST_F(EntityInspectSerializerTest, SerializeEntityInspect_NoComponents_ComponentsEmpty)
{
    Entity e = MakeLiveEntity();
    Json::Value result = SerializeEntityInspect(mDomain, e);
    ASSERT_TRUE(result.isMember("components"));
    EXPECT_EQ(result["components"].size(), 0u);
}

// ===========================================================================
// Additional: component entry has type_id_crc and type_name fields
// ===========================================================================

TEST_F(EntityInspectSerializerTest, SerializeEntityInspect_ComponentEntry_HasTypeFields)
{
    Entity e = MakeLiveEntityWithComp();
    Json::Value result = SerializeEntityInspect(mDomain, e);
    ASSERT_GE(result["components"].size(), 1u);

    const Json::Value& comp = result["components"][0];
    EXPECT_TRUE(comp.isMember("type_id_crc"));
    EXPECT_TRUE(comp.isMember("type_name"));
    EXPECT_TRUE(comp.isMember("fields"));
    EXPECT_TRUE(comp["fields"].isArray());
}

// ===========================================================================
// Additional: component fields are serialized with name, kind, value
// ===========================================================================

TEST_F(EntityInspectSerializerTest, SerializeEntityInspect_ComponentFields_HaveRequiredKeys)
{
    Entity e = MakeLiveEntityWithComp();
    Json::Value result = SerializeEntityInspect(mDomain, e);

    const Json::Value& fields = result["components"][0]["fields"];
    ASSERT_GE(fields.size(), 1u);

    for (unsigned int i = 0; i < fields.size(); ++i)
    {
        EXPECT_TRUE(fields[i].isMember("name"));
        EXPECT_TRUE(fields[i].isMember("kind"));
        EXPECT_TRUE(fields[i].isMember("value"));
    }
}

// ===========================================================================
// Additional: hierarchy section has expected sub-fields
// ===========================================================================

TEST_F(EntityInspectSerializerTest, SerializeEntityInspect_Hierarchy_HasExpectedFields)
{
    Entity e = MakeLiveEntity();
    Json::Value result = SerializeEntityInspect(mDomain, e);
    ASSERT_TRUE(result.isMember("hierarchy"));

    const Json::Value& hier = result["hierarchy"];
    EXPECT_TRUE(hier.isMember("parent_index"));
    EXPECT_TRUE(hier.isMember("parent_gen"));
    EXPECT_TRUE(hier.isMember("child_count"));

    // No parent wired — should default to -1/0/0.
    EXPECT_EQ(hier["parent_index"].asInt(), -1);
    EXPECT_EQ(hier["parent_gen"].asInt(),    0);
    EXPECT_EQ(hier["child_count"].asInt(),   0);
}

// ===========================================================================
// Additional: entity generation is serialized and non-zero for live entity
// ===========================================================================

TEST_F(EntityInspectSerializerTest, SerializeEntityInspect_Entity_GenerationNonZero)
{
    Entity e = MakeLiveEntity();
    Json::Value result = SerializeEntityInspect(mDomain, e);
    ASSERT_TRUE(result["entity"].isMember("gen"));
    EXPECT_GT(result["entity"]["gen"].asInt(), 0);
}

// ===========================================================================
// Additional: SerializeEntityList entry index matches the entity's index
// ===========================================================================

TEST_F(EntityInspectSerializerTest, SerializeEntityList_EntryIndex_MatchesEntityIndex)
{
    Entity e = MakeLiveEntity("idx_check");
    Json::Value result = SerializeEntityList(mDomain);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(static_cast<uint32_t>(result[0]["index"].asInt()), e.GetIndex());
}

// ===========================================================================
// Additional: SerializeEntityList component_count is 0 for component-less entity
// ===========================================================================

TEST_F(EntityInspectSerializerTest, SerializeEntityList_NoComponents_ComponentCountZero)
{
    MakeLiveEntity("bare");
    Json::Value result = SerializeEntityList(mDomain);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0]["component_count"].asInt(), 0);
}

// ===========================================================================
// Additional: destroyed entity is absent from SerializeEntityList
// ===========================================================================

TEST_F(EntityInspectSerializerTest, SerializeEntityList_AfterDestroy_EntityAbsent)
{
    Entity e1 = MakeLiveEntity("keep");
    Entity e2 = MakeLiveEntity("destroy");
    (void)e1;

    mDomain.QueueDestroy(e2);
    mDomain.EndOfFrame();

    Json::Value result = SerializeEntityList(mDomain);
    EXPECT_EQ(result.size(), 1u);

    for (unsigned int i = 0; i < result.size(); ++i)
    {
        EXPECT_NE(static_cast<uint32_t>(result[i]["index"].asInt()), e2.GetIndex());
    }
}
