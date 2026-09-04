// Suite: DiaAttributeChangeNotifications
// Covers AC-1..AC-5 from the Change Notifications feature (Task 3) of the DiaAttribute spec.
// See docs/specs/applications/dia/systems/diaattribute/change-notifications.md

#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <DiaAttribute/AttributeSchema.h>
#include <DiaAttribute/AttributeSet.h>
#include <DiaAttribute/IAttributeObserver.h>
#include <DiaAttribute/AttributeObserverSubject.h>

#include <DiaEntity/Entity.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <string.h>

using namespace Dia::Attribute;
using Dia::Core::StringCRC;

// ===========================================================================
// Helpers
// ===========================================================================

static Json::Value ParseJson(const char* str)
{
    Json::Value root;
    Json::Reader reader;
    reader.parse(str, root);
    return root;
}

static AttributeSchema MakeDragonSchema()
{
    const char* json =
        "{ \"schema_name\": \"dragon\","
        "  \"attributes\": ["
        "    { \"attribute_name\": \"health\",   \"minimum_value\": 0.0, \"maximum_value\": 2000.0, \"default_value\": 1000.0 },"
        "    { \"attribute_name\": \"strength\", \"minimum_value\": 0.0, \"maximum_value\": 200.0,  \"default_value\": 50.0 }"
        "  ] }";
    return AttributeSchema::LoadFromJsonValue(ParseJson(json));
}

static AttributeModifier MakeModifier(const char* name, const char* attr, ModifierOperation op, float value)
{
    AttributeModifier mod{};
    mod.modifier_name     = StringCRC(name);
    mod.attribute_name    = StringCRC(attr);
    mod.operation         = op;
    mod.value             = value;
    mod.when_condition[0] = '\0';
    return mod;
}

// ===========================================================================
// Recording observer — fixed-size arrays + counts, mirroring AttributeLogSink's
// style in TestDiaAttributeCore.cpp (no STL containers).
// ===========================================================================

namespace
{
    class RecordingObserver : public IAttributeObserver
    {
    public:
        static const unsigned int kMaxEvents = 64;

        RecordingObserver()
            : mChangedCount(0)
            , mMaxCount(0)
            , mMinCount(0)
        {}

        void OnAttributeChanged(const AttributeChangedEvent& e) override
        {
            if (mChangedCount < kMaxEvents)
                mChangedEvents[mChangedCount++] = e;
        }

        void OnAttributeReachedMaximum(Dia::Entity::Entity entity, Dia::Core::StringCRC attribute_name) override
        {
            if (mMaxCount < kMaxEvents)
            {
                mMaxEntities[mMaxCount]   = entity;
                mMaxAttributes[mMaxCount] = attribute_name;
                ++mMaxCount;
            }
        }

        void OnAttributeReachedMinimum(Dia::Entity::Entity entity, Dia::Core::StringCRC attribute_name) override
        {
            if (mMinCount < kMaxEvents)
            {
                mMinEntities[mMinCount]   = entity;
                mMinAttributes[mMinCount] = attribute_name;
                ++mMinCount;
            }
        }

        AttributeChangedEvent mChangedEvents[kMaxEvents];
        unsigned int          mChangedCount;

        Dia::Entity::Entity   mMaxEntities[kMaxEvents];
        Dia::Core::StringCRC  mMaxAttributes[kMaxEvents];
        unsigned int          mMaxCount;

        Dia::Entity::Entity   mMinEntities[kMaxEvents];
        Dia::Core::StringCRC  mMinAttributes[kMaxEvents];
        unsigned int          mMinCount;
    };
}

// ===========================================================================
// AC-1 — OnAttributeChanged fires exactly once per value-changing mutation,
// with correct old_value/new_value, for SetBaseValue / AddModifier / RemoveModifier
// ===========================================================================

TEST(DiaAttributeChangeNotifications, SetBaseValue_ChangesValue_FiresOnce_WithCorrectOldAndNew)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    RecordingObserver observer;
    set.GetObserverSubject().Subscribe(&observer);

    set.SetBaseValue(StringCRC("health"), 500.0f);

    ASSERT_EQ(observer.mChangedCount, 1u);
    EXPECT_FLOAT_EQ(observer.mChangedEvents[0].old_value, 1000.0f);
    EXPECT_FLOAT_EQ(observer.mChangedEvents[0].new_value, 500.0f);
    EXPECT_EQ(observer.mChangedEvents[0].attribute_name, StringCRC("health"));
}

TEST(DiaAttributeChangeNotifications, AddModifier_ChangesValue_FiresOnce_WithCorrectOldAndNew)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    RecordingObserver observer;
    set.GetObserverSubject().Subscribe(&observer);

    set.AddModifier(MakeModifier("buff", "health", ModifierOperation::Add, 50.0f));

    ASSERT_EQ(observer.mChangedCount, 1u);
    EXPECT_FLOAT_EQ(observer.mChangedEvents[0].old_value, 1000.0f);
    EXPECT_FLOAT_EQ(observer.mChangedEvents[0].new_value, 1050.0f);
}

TEST(DiaAttributeChangeNotifications, RemoveModifier_ChangesValue_FiresOnce_WithCorrectOldAndNew)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    ModifierHandle h = set.AddModifier(MakeModifier("buff", "health", ModifierOperation::Add, 50.0f));

    RecordingObserver observer;
    set.GetObserverSubject().Subscribe(&observer);

    set.RemoveModifier(h);

    ASSERT_EQ(observer.mChangedCount, 1u);
    EXPECT_FLOAT_EQ(observer.mChangedEvents[0].old_value, 1050.0f);
    EXPECT_FLOAT_EQ(observer.mChangedEvents[0].new_value, 1000.0f);
}

// ===========================================================================
// AC-2 — a mutation that does NOT change the resolved value fires no event
// ===========================================================================

TEST(DiaAttributeChangeNotifications, SetBaseValue_ToCurrentValue_FiresNoEvent)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    RecordingObserver observer;
    set.GetObserverSubject().Subscribe(&observer);

    set.SetBaseValue(StringCRC("health"), 1000.0f); // same as default_value

    EXPECT_EQ(observer.mChangedCount, 0u);
}

TEST(DiaAttributeChangeNotifications, AddModifier_RejectedModifier_FiresNoEvent)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);
    // No SetConditionRegistry call — a conditional modifier is rejected via
    // DIA_LOG_WARNING only (no DIA_ASSERT), so this exercises the rejection path
    // without triggering an assert/breakpoint in debug builds.

    RecordingObserver observer;
    set.GetObserverSubject().Subscribe(&observer);

    AttributeModifier mod = MakeModifier("buff", "health", ModifierOperation::Add, 50.0f);
    strncpy_s(mod.when_condition, sizeof(mod.when_condition), R"({"op":"==","slot":"actor","field":"ready","value":true})", _TRUNCATE);

    ModifierHandle h = set.AddModifier(mod);

    EXPECT_FALSE(h.IsValid());
    EXPECT_EQ(observer.mChangedCount, 0u);
}

// ===========================================================================
// AC-3 — boundary events are edge-triggered, not level-triggered
// ===========================================================================

TEST(DiaAttributeChangeNotifications, AddModifier_PushesPastMaximum_FiresReachedMaximumOnce)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    RecordingObserver observer;
    set.GetObserverSubject().Subscribe(&observer);

    // health: default 1000, maximum 2000 — this pushes well past the clamp.
    set.AddModifier(MakeModifier("bigbuff", "health", ModifierOperation::Add, 5000.0f));

    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 2000.0f);
    EXPECT_EQ(observer.mMaxCount, 1u);
    EXPECT_EQ(observer.mMinCount, 0u);
}

TEST(DiaAttributeChangeNotifications, SecondMutation_StillAtMaximum_DoesNotRefireReachedMaximum)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    // Push to maximum first, before subscribing, so the transition-in already happened.
    set.AddModifier(MakeModifier("bigbuff", "health", ModifierOperation::Add, 5000.0f));
    ASSERT_FLOAT_EQ(set.GetValue(StringCRC("health")), 2000.0f);

    RecordingObserver observer;
    set.GetObserverSubject().Subscribe(&observer);

    // Still clamped to maximum after this — value is unchanged (2000 -> 2000), so
    // this should not fire OnAttributeChanged OR OnAttributeReachedMaximum again.
    set.AddModifier(MakeModifier("anotherbuff", "health", ModifierOperation::Add, 10.0f));

    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 2000.0f);
    EXPECT_EQ(observer.mChangedCount, 0u);
    EXPECT_EQ(observer.mMaxCount, 0u);
}

TEST(DiaAttributeChangeNotifications, SetBaseValue_PushesPastMinimum_FiresReachedMinimumOnce)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    RecordingObserver observer;
    set.GetObserverSubject().Subscribe(&observer);

    set.SetBaseValue(StringCRC("strength"), -500.0f); // minimum is 0.0

    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("strength")), 0.0f);
    EXPECT_EQ(observer.mMinCount, 1u);
    EXPECT_EQ(observer.mMaxCount, 0u);
}

TEST(DiaAttributeChangeNotifications, SecondMutation_StillAtMinimum_DoesNotRefireReachedMinimum)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    set.SetBaseValue(StringCRC("strength"), -500.0f);
    ASSERT_FLOAT_EQ(set.GetValue(StringCRC("strength")), 0.0f);

    RecordingObserver observer;
    set.GetObserverSubject().Subscribe(&observer);

    set.SetBaseValue(StringCRC("strength"), -1000.0f); // still clamped to 0.0 -> no change, no re-fire

    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("strength")), 0.0f);
    EXPECT_EQ(observer.mChangedCount, 0u);
    EXPECT_EQ(observer.mMinCount, 0u);
}

// ===========================================================================
// AC-4 — multiple observers, Unsubscribe, and safe destruction with live subscribers
// ===========================================================================

TEST(DiaAttributeChangeNotifications, MultipleObservers_BothReceive_UnsubscribeOneStopsDelivery)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    RecordingObserver observerA;
    RecordingObserver observerB;
    set.GetObserverSubject().Subscribe(&observerA);
    set.GetObserverSubject().Subscribe(&observerB);

    set.SetBaseValue(StringCRC("health"), 500.0f);
    EXPECT_EQ(observerA.mChangedCount, 1u);
    EXPECT_EQ(observerB.mChangedCount, 1u);

    set.GetObserverSubject().Unsubscribe(&observerA);

    set.SetBaseValue(StringCRC("health"), 600.0f);
    EXPECT_EQ(observerA.mChangedCount, 1u); // unchanged — no longer subscribed
    EXPECT_EQ(observerB.mChangedCount, 2u); // still subscribed
}

TEST(DiaAttributeChangeNotifications, AttributeSet_GoesOutOfScope_WithLiveSubscriber_DoesNotCrash)
{
    RecordingObserver observer;

    {
        AttributeSchema schema = MakeDragonSchema();
        AttributeSet set = AttributeSet::CreateFromSchema(schema);
        set.GetObserverSubject().Subscribe(&observer);
        set.SetBaseValue(StringCRC("health"), 500.0f);
        EXPECT_EQ(observer.mChangedCount, 1u);
    } // set destructs here, with observer still subscribed — must not crash

    SUCCEED();
}

// ===========================================================================
// AC-5 — events are synchronous, delivered inline before the mutating call returns
// ===========================================================================

TEST(DiaAttributeChangeNotifications, OnAttributeChanged_ObservedSynchronously_DuringMutatingCall)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    class InlineCheckObserver : public IAttributeObserver
    {
    public:
        explicit InlineCheckObserver(AttributeSet* owner) : mOwner(owner), mSawUpdatedValueDuringCallback(false) {}

        void OnAttributeChanged(const AttributeChangedEvent& e) override
        {
            // At the moment this fires, the mutation must already be applied —
            // GetValue() should reflect the new value, not the old one.
            mSawUpdatedValueDuringCallback = (mOwner->GetValue(e.attribute_name) == e.new_value);
        }

        AttributeSet* mOwner;
        bool          mSawUpdatedValueDuringCallback;
    };

    InlineCheckObserver observer(&set);
    set.GetObserverSubject().Subscribe(&observer);

    set.SetBaseValue(StringCRC("health"), 500.0f);

    EXPECT_TRUE(observer.mSawUpdatedValueDuringCallback);
}

// ===========================================================================
// SetOwningEntity — carried through on events once set
// ===========================================================================

TEST(DiaAttributeChangeNotifications, SetOwningEntity_CarriedOnChangedEvent)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    Dia::Entity::Entity owner(7, 1);
    set.SetOwningEntity(owner);

    RecordingObserver observer;
    set.GetObserverSubject().Subscribe(&observer);

    set.SetBaseValue(StringCRC("health"), 500.0f);

    ASSERT_EQ(observer.mChangedCount, 1u);
    EXPECT_EQ(observer.mChangedEvents[0].entity, owner);
}
