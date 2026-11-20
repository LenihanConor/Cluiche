// Suite: DiaAttributeSetIndexAccess
// Parameter Files plan task 14: schema-ordered slot storage, O(1) by-index accessors and the
// atomic bulk base-value commit on AttributeSet.
// See docs/specs/applications/dia/systems/diaattribute/parameter-files.md

#include <gtest/gtest.h>

#include <DiaAttribute/AttributeSchema.h>
#include <DiaAttribute/AttributeSet.h>
#include <DiaAttribute/IAttributeObserver.h>
#include <DiaAttribute/AttributeObserverSubject.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <string.h>

using namespace Dia::Attribute;
using Dia::Core::StringCRC;

namespace
{
    AttributeSchema MakeSchema()
    {
        const char* json =
            "{ \"schema_name\": \"idx\","
            "  \"attributes\": ["
            "    { \"attribute_name\": \"zeta\",  \"minimum_value\": 0.0, \"maximum_value\": 100.0, \"default_value\": 10.0 },"
            "    { \"attribute_name\": \"alpha\", \"minimum_value\": 0.0, \"maximum_value\": 100.0, \"default_value\": 20.0 },"
            "    { \"attribute_name\": \"mid\",   \"minimum_value\": 0.0, \"maximum_value\": 100.0, \"default_value\": 30.0 },"
            "    { \"attribute_name\": \"beta\",  \"minimum_value\": 0.0, \"maximum_value\": 100.0, \"default_value\": 40.0 }"
            "  ] }";
        Json::Value root;
        Json::Reader reader;
        reader.parse(json, root);
        return AttributeSchema::LoadFromJsonValue(root);
    }

    const char* const kNames[4] = { "zeta", "alpha", "mid", "beta" };

    struct RecordingObserver : IAttributeObserver
    {
        struct Rec { StringCRC name; float oldV; float newV; float snapshot[4]; };
        AttributeSet* set = nullptr;
        Rec recs[16];
        unsigned int count = 0;

        void OnAttributeChanged(const AttributeChangedEvent& e) override
        {
            if (count >= 16) return;
            Rec& r = recs[count++];
            r.name = e.attribute_name; r.oldV = e.old_value; r.newV = e.new_value;
            for (unsigned int i = 0; i < 4; ++i) r.snapshot[i] = set->GetBaseValueByIndex(i);
        }
    };
}

TEST(DiaAttributeSetIndexAccess, GetBaseValueByIndex_ReturnsDefaultsInSchemaOrder)
{
    AttributeSchema schema = MakeSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);
    EXPECT_FLOAT_EQ(set.GetBaseValueByIndex(0), 10.0f);
    EXPECT_FLOAT_EQ(set.GetBaseValueByIndex(1), 20.0f);
    EXPECT_FLOAT_EQ(set.GetBaseValueByIndex(2), 30.0f);
    EXPECT_FLOAT_EQ(set.GetBaseValueByIndex(3), 40.0f);
}

TEST(DiaAttributeSetIndexAccess, SetBaseValueByIndex_UpdatesByNameAndIndex_AndIsSilent)
{
    AttributeSchema schema = MakeSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);
    RecordingObserver obs; obs.set = &set;
    set.GetObserverSubject().Subscribe(&obs);

    set.SetBaseValueByIndex(2, 77.0f);

    EXPECT_FLOAT_EQ(set.GetBaseValueByIndex(2), 77.0f);
    EXPECT_FLOAT_EQ(set.GetBaseValue(StringCRC("mid")), 77.0f);
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("mid")), 77.0f);
    EXPECT_FLOAT_EQ(set.GetBaseValueByIndex(1), 20.0f);
    EXPECT_EQ(obs.count, 0u);
}

TEST(DiaAttributeSetIndexAccess, ReplaceBaseValues_CountMismatch_RejectedStateUnchanged)
{
    AttributeSchema schema = MakeSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);
    RecordingObserver obs; obs.set = &set;
    set.GetObserverSubject().Subscribe(&obs);

    float before[4];
    for (unsigned int i = 0; i < 4; ++i) before[i] = set.GetBaseValueByIndex(i);

    const float bad[5] = { 1, 2, 3, 4, 5 };
    EXPECT_FALSE(set.ReplaceBaseValues(bad, 5));
    EXPECT_FALSE(set.ReplaceBaseValues(bad, 3));

    float after[4];
    for (unsigned int i = 0; i < 4; ++i) after[i] = set.GetBaseValueByIndex(i);
    EXPECT_EQ(0, memcmp(before, after, sizeof(before)));
    EXPECT_EQ(obs.count, 0u);
}

TEST(DiaAttributeSetIndexAccess, ReplaceBaseValues_UnchangedValues_FireNothing)
{
    AttributeSchema schema = MakeSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);
    RecordingObserver obs; obs.set = &set;
    set.GetObserverSubject().Subscribe(&obs);

    const float same[4] = { 10.0f, 20.0f, 30.0f, 40.0f };
    EXPECT_TRUE(set.ReplaceBaseValues(same, 4));
    EXPECT_EQ(obs.count, 0u);
}

TEST(DiaAttributeSetIndexAccess, ReplaceBaseValues_ChangedValues_FireOncePerChange_AfterFullCommit)
{
    AttributeSchema schema = MakeSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);
    RecordingObserver obs; obs.set = &set;
    set.GetObserverSubject().Subscribe(&obs);

    const float next[4] = { 11.0f, 20.0f, 31.0f, 41.0f }; // index 1 unchanged
    EXPECT_TRUE(set.ReplaceBaseValues(next, 4));

    ASSERT_EQ(obs.count, 3u);
    EXPECT_TRUE(obs.recs[0].name == StringCRC("zeta"));
    EXPECT_TRUE(obs.recs[1].name == StringCRC("mid"));
    EXPECT_TRUE(obs.recs[2].name == StringCRC("beta"));
    EXPECT_FLOAT_EQ(obs.recs[0].oldV, 10.0f);
    EXPECT_FLOAT_EQ(obs.recs[0].newV, 11.0f);
    EXPECT_FLOAT_EQ(obs.recs[2].oldV, 40.0f);
    EXPECT_FLOAT_EQ(obs.recs[2].newV, 41.0f);
    for (unsigned int r = 0; r < 3; ++r)
        for (unsigned int i = 0; i < 4; ++i)
            EXPECT_FLOAT_EQ(obs.recs[r].snapshot[i], next[i]) << "rec " << r << " idx " << i;
}

TEST(DiaAttributeSetIndexAccess, BaseValueChanges_InvalidateResolvedValue_WithModifier)
{
    AttributeSchema schema = MakeSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    AttributeModifier mod{};
    mod.modifier_name  = StringCRC("plus5");
    mod.attribute_name = StringCRC("alpha");
    mod.operation      = ModifierOperation::Add;
    mod.value          = 5.0f;
    mod.when_condition[0] = '\0';
    (void)set.AddModifier(mod);

    EXPECT_FLOAT_EQ(set.GetValueByIndex(1), 25.0f);
    set.SetBaseValueByIndex(1, 50.0f);
    EXPECT_FLOAT_EQ(set.GetValueByIndex(1), 55.0f);

    const float next[4] = { 10.0f, 60.0f, 30.0f, 40.0f };
    EXPECT_TRUE(set.ReplaceBaseValues(next, 4));
    EXPECT_FLOAT_EQ(set.GetValueByIndex(1), 65.0f);
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("alpha")), 65.0f);
}

TEST(DiaAttributeSetIndexAccess, GetAttributeNameByIndex_ReturnsSchemaOrder_AndNameLookupWorks)
{
    AttributeSchema schema = MakeSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);
    ASSERT_EQ(set.GetAttributeCount(), 4u);
    for (unsigned int i = 0; i < 4; ++i)
    {
        EXPECT_TRUE(set.GetAttributeNameByIndex(i) == StringCRC(kNames[i])) << i;
        EXPECT_TRUE(set.HasAttribute(StringCRC(kNames[i])));
        EXPECT_FLOAT_EQ(set.GetValue(StringCRC(kNames[i])), set.GetValueByIndex(i));
    }
    EXPECT_FALSE(set.HasAttribute(StringCRC("nope")));
}
