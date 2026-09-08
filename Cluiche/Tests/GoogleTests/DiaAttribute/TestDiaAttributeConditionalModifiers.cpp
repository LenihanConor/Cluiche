// Suite: DiaAttributeConditionalModifiers
// Covers AC-1..AC-7 from the Conditional Modifiers feature (Task 2) of the DiaAttribute spec.
// See docs/specs/applications/dia/systems/diaattribute/conditional-modifiers.md

#include <gtest/gtest.h>

#if defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <DiaAttribute/AttributeSchema.h>
#include <DiaAttribute/AttributeSet.h>

#include <DiaCondition/ConditionExpr.h>
#include <DiaCondition/ConditionRegistry.h>

#include <DiaObservation/Log/Logger.h>
#include <DiaObservation/Log/ISink.h>
#include <DiaObservation/Log/LogEntry.h>
#include <DiaObservation/Log/LogLevel.h>

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

static AttributeModifier MakeConditionalModifier(const char* name, const char* attr, ModifierOperation op, float value, const char* whenConditionJson)
{
    AttributeModifier mod{};
    mod.modifier_name  = StringCRC(name);
    mod.attribute_name = StringCRC(attr);
    mod.operation       = op;
    mod.value           = value;
    if (whenConditionJson != nullptr)
    {
        strncpy_s(mod.when_condition, sizeof(mod.when_condition), whenConditionJson, _TRUNCATE);
    }
    else
    {
        mod.when_condition[0] = '\0';
    }
    return mod;
}

// Backing data for the ConditionRegistry accessors used across these tests.
// A single "actor" slot with a "health" float field and a "ready" bool field.
namespace
{
    struct ConditionTestState
    {
        float health = 0.0f;
        bool  ready  = false;
    };

    float ReadHealthAccessor(void* data) { return static_cast<ConditionTestState*>(data)->health; }
    bool  ReadReadyAccessor(void* data)  { return static_cast<ConditionTestState*>(data)->ready; }

    // Registers the "actor.health" (float) and "actor.ready" (bool) accessors against
    // an already-constructed registry. ConditionRegistry is not copyable/movable-safe
    // to return by value (implicit shallow copy would double-free its Impl*), so callers
    // construct the registry themselves and pass it in by reference to configure.
    void ConfigureTestRegistry(Dia::Condition::ConditionRegistry& registry)
    {
        registry.RegisterFloat(StringCRC("actor"), StringCRC("health"), &ReadHealthAccessor);
        registry.RegisterBool(StringCRC("actor"), StringCRC("ready"), &ReadReadyAccessor);
    }
}

// ===========================================================================
// AC-1 — empty when_condition always applies (no regression vs Core)
// ===========================================================================

TEST(DiaAttributeConditionalModifiers, AC1_EmptyWhenCondition_AlwaysApplies_NoRegression)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    // No SetConditionRegistry call at all — unconditional modifiers must still work.
    ModifierHandle h = set.AddModifier(MakeModifier("buff", "health", ModifierOperation::Add, 50.0f));
    EXPECT_TRUE(h.IsValid());
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1050.0f);
}

// ===========================================================================
// AC-2 — condition false: excluded from value, but modifier stays registered
// ===========================================================================

TEST(DiaAttributeConditionalModifiers, AC2_ConditionFalse_ExcludedFromValue_HandleValid)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    ConditionTestState state{};
    state.ready = false;
    Dia::Condition::ConditionRegistry registry(&state);
    ConfigureTestRegistry(registry);
    set.SetConditionRegistry(&registry);

    const char* whenCondition = R"({"op":"==","slot":"actor","field":"ready","value":true})";
    ModifierHandle h = set.AddModifier(MakeConditionalModifier("buff", "health", ModifierOperation::Add, 50.0f, whenCondition));

    EXPECT_TRUE(h.IsValid()); // added successfully — NOT rejected (condition being false is not a rejection reason)
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1000.0f); // contribution excluded
}

#ifdef _DEBUG
// Uses the existing Override-uniqueness check (which scans every entry in the modifier
// array regardless of condition state) as a "query" proving the false-conditioned modifier
// is still occupying a slot in the array — RemoveModifier was never called on it.
TEST(SLOW_DiaAttributeConditionalModifiers, AC2_ConditionFalse_ModifierStaysRegistered_OverrideCollisionStillDetectsIt)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    ConditionTestState state{};
    state.ready = false;
    Dia::Condition::ConditionRegistry registry(&state);
    ConfigureTestRegistry(registry);
    set.SetConditionRegistry(&registry);

    const char* whenCondition = R"({"op":"==","slot":"actor","field":"ready","value":true})";
    ModifierHandle h = set.AddModifier(MakeConditionalModifier("ovr1", "strength", ModifierOperation::Override, 999.0f, whenCondition));
    ASSERT_TRUE(h.IsValid());
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("strength")), 50.0f); // excluded — base unaffected

    // A second (unconditional) Override on the same attribute must still be detected as a
    // collision — proving the first Override modifier is still present in the array, just inactive.
    EXPECT_DEATH((void)set.AddModifier(MakeModifier("ovr2", "strength", ModifierOperation::Override, 1.0f)), "");
}
#endif // _DEBUG

// ===========================================================================
// AC-3 — condition flips false -> true between GetValue() calls, no re-registration
// ===========================================================================

TEST(DiaAttributeConditionalModifiers, AC3_ConditionFlipsFalseToTrue_NextResolveReflectsImmediately)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    ConditionTestState state{};
    state.ready = false;
    Dia::Condition::ConditionRegistry registry(&state);
    ConfigureTestRegistry(registry);
    set.SetConditionRegistry(&registry);

    const char* whenCondition = R"({"op":"==","slot":"actor","field":"ready","value":true})";
    ModifierHandle h = set.AddModifier(MakeConditionalModifier("buff", "health", ModifierOperation::Add, 50.0f, whenCondition));
    ASSERT_TRUE(h.IsValid());

    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1000.0f); // false -> excluded

    state.ready = true; // flip the underlying condition — no AddModifier/RemoveModifier calls

    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1050.0f); // true -> included, same handle
}

// ===========================================================================
// AC-4 — unresolvable accessor rejected at AddModifier time
// ===========================================================================

TEST(DiaAttributeConditionalModifiers, AC4_UnresolvableAccessor_RejectedAtAddModifier_InvalidHandle)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    ConditionTestState state{};
    Dia::Condition::ConditionRegistry registry(&state); // deliberately empty — no accessors registered
    set.SetConditionRegistry(&registry);

    const char* whenCondition = R"({"op":">=","slot":"unknown_slot","field":"missing_field","value":1})";
    ModifierHandle h = set.AddModifier(MakeConditionalModifier("buff", "health", ModifierOperation::Add, 50.0f, whenCondition));

    EXPECT_FALSE(h.IsValid());
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1000.0f); // modifier never added — base unaffected
}

// ===========================================================================
// AC-5 — malformed when_condition JSON rejected the same way as AC-4
// ===========================================================================

TEST(DiaAttributeConditionalModifiers, AC5_MalformedJson_RejectedAtAddModifier_InvalidHandle)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    ConditionTestState state{};
    Dia::Condition::ConditionRegistry registry(&state);
    ConfigureTestRegistry(registry);
    set.SetConditionRegistry(&registry);

    const char* malformedJson = "{ this is not valid json ";
    ModifierHandle h = set.AddModifier(MakeConditionalModifier("buff", "health", ModifierOperation::Add, 50.0f, malformedJson));

    EXPECT_FALSE(h.IsValid());
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1000.0f);
}

// ===========================================================================
// Conditional modifier added with no registry set at all — treated as rejection too
// ===========================================================================

TEST(DiaAttributeConditionalModifiers, NoConditionRegistrySet_ConditionalModifier_Rejected)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);
    // No SetConditionRegistry call — registry is nullptr.

    const char* whenCondition = R"({"op":"==","slot":"actor","field":"ready","value":true})";
    ModifierHandle h = set.AddModifier(MakeConditionalModifier("buff", "health", ModifierOperation::Add, 50.0f, whenCondition));

    EXPECT_FALSE(h.IsValid());
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), 1000.0f);
}

// ===========================================================================
// AC-7 — equip/unequip symmetry regardless of condition state at removal
// ===========================================================================

TEST(DiaAttributeConditionalModifiers, AC7_RemoveModifier_ConditionTrueAtRemoval_ReturnsToPreAddValue)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    ConditionTestState state{};
    state.ready = true;
    Dia::Condition::ConditionRegistry registry(&state);
    ConfigureTestRegistry(registry);
    set.SetConditionRegistry(&registry);

    const float preAddValue = set.GetValue(StringCRC("health"));

    const char* whenCondition = R"({"op":"==","slot":"actor","field":"ready","value":true})";
    ModifierHandle h = set.AddModifier(MakeConditionalModifier("buff", "health", ModifierOperation::Add, 50.0f, whenCondition));
    ASSERT_TRUE(h.IsValid());
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), preAddValue + 50.0f); // condition true -> applied

    set.RemoveModifier(h);
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), preAddValue);
}

TEST(DiaAttributeConditionalModifiers, AC7_RemoveModifier_ConditionFalseAtRemoval_ReturnsToPreAddValue)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    ConditionTestState state{};
    state.ready = false;
    Dia::Condition::ConditionRegistry registry(&state);
    ConfigureTestRegistry(registry);
    set.SetConditionRegistry(&registry);

    const float preAddValue = set.GetValue(StringCRC("health"));

    const char* whenCondition = R"({"op":"==","slot":"actor","field":"ready","value":true})";
    ModifierHandle h = set.AddModifier(MakeConditionalModifier("buff", "health", ModifierOperation::Add, 50.0f, whenCondition));
    ASSERT_TRUE(h.IsValid());
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), preAddValue); // condition false -> excluded, no change

    set.RemoveModifier(h); // remove while condition is still false
    EXPECT_FLOAT_EQ(set.GetValue(StringCRC("health")), preAddValue);
}

// ===========================================================================
// Destructor cleanup — a live (never-removed) CONDITIONAL modifier's heap-owned
// ConditionExpr (ModifierEntry::parsed_condition) must be deleted by ~AttributeSet
// without crashing. Mirrors AttributeSet_GoesOutOfScope_WithLiveSubscriber_DoesNotCrash
// in TestDiaAttributeChangeNotifications.cpp, which covers the observer-subscription
// side of destruction but not this separate per-modifier cleanup loop.
// ===========================================================================

TEST(DiaAttributeConditionalModifiers, AttributeSet_GoesOutOfScope_WithLiveConditionalModifier_DoesNotCrash)
{
    ConditionTestState state{};
    state.ready = true; // condition currently true — modifier is live and active, not dormant

    {
        Dia::Condition::ConditionRegistry registry(&state);
        ConfigureTestRegistry(registry);
        AttributeSchema schema = MakeDragonSchema();
        AttributeSet set = AttributeSet::CreateFromSchema(schema);
        set.SetConditionRegistry(&registry);

        const char* whenCondition = R"({"op":"==","slot":"actor","field":"ready","value":true})";
        ModifierHandle h = set.AddModifier(MakeConditionalModifier("buff", "health", ModifierOperation::Add, 50.0f, whenCondition));
        ASSERT_TRUE(h.IsValid());
        // Deliberately never call RemoveModifier — the destructor must clean up
        // parsed_condition itself.
    } // set (and its live conditional modifier's heap-allocated ConditionExpr) destructs here

    SUCCEED();
}

TEST(DiaAttributeConditionalModifiers, AttributeSet_GoesOutOfScope_WithTwoLiveConditionalModifiersOnDifferentAttributes_DoesNotCrash)
{
    ConditionTestState state{};
    state.ready = true;

    {
        Dia::Condition::ConditionRegistry registry(&state);
        ConfigureTestRegistry(registry);
        AttributeSchema schema = MakeDragonSchema();
        AttributeSet set = AttributeSet::CreateFromSchema(schema);
        set.SetConditionRegistry(&registry);

        const char* whenCondition = R"({"op":"==","slot":"actor","field":"ready","value":true})";
        ModifierHandle h1 = set.AddModifier(MakeConditionalModifier("healthBuff", "health", ModifierOperation::Add, 50.0f, whenCondition));
        ModifierHandle h2 = set.AddModifier(MakeConditionalModifier("strengthBuff", "strength", ModifierOperation::Add, 10.0f, whenCondition));
        ASSERT_TRUE(h1.IsValid());
        ASSERT_TRUE(h2.IsValid());
        // Both left live — the destructor's slot loop must walk every attribute's
        // slot (not just one) and delete each entry's parsed_condition.
    }

    SUCCEED();
}

TEST(DiaAttributeConditionalModifiers, AttributeSet_GoesOutOfScope_WithMixedConditionalAndUnconditionalModifiers_DoesNotCrash)
{
    ConditionTestState state{};
    state.ready = true;

    {
        Dia::Condition::ConditionRegistry registry(&state);
        ConfigureTestRegistry(registry);
        AttributeSchema schema = MakeDragonSchema();
        AttributeSet set = AttributeSet::CreateFromSchema(schema);
        set.SetConditionRegistry(&registry);

        const char* whenCondition = R"({"op":"==","slot":"actor","field":"ready","value":true})";
        ModifierHandle conditional   = set.AddModifier(MakeConditionalModifier("condBuff", "health", ModifierOperation::Add, 50.0f, whenCondition));
        ModifierHandle unconditional = set.AddModifier(MakeModifier("plainBuff", "health", ModifierOperation::Add, 25.0f));
        ASSERT_TRUE(conditional.IsValid());
        ASSERT_TRUE(unconditional.IsValid());
        // Destructor's per-entry null-check on parsed_condition must delete only the
        // conditional entry's ConditionExpr and skip the unconditional one (nullptr).
    }

    SUCCEED();
}

// ===========================================================================
// Logging — AC-4 / AC-5 rejection paths log a warning (DIA_LOG_WARNING)
// ===========================================================================

namespace
{
    class ConditionalModifierLogSink : public Dia::Observation::Log::ISink
    {
    public:
        static const unsigned int kMaxEntries = 128;

        ConditionalModifierLogSink()
            : mEntryCount(0)
        {
            SetLevelThreshold(Dia::Observation::Log::LogLevel::kDebug);
            SetChannelFilter(Dia::Core::StringCRC("Attribute"), true);
        }

        void OnLogEntry(const Dia::Observation::Log::LogEntry& entry) override
        {
            if (mEntryCount < kMaxEntries)
                mEntries[mEntryCount++] = entry;
        }

        const char* GetName() const override { return "ConditionalModifierLogSink"; }

        unsigned int CountByLevel(Dia::Observation::Log::LogLevel level) const
        {
            unsigned int count = 0;
            for (unsigned int i = 0; i < mEntryCount; ++i)
            {
                if (mEntries[i].level == level) ++count;
            }
            return count;
        }

        void Clear() { mEntryCount = 0; }

    private:
        Dia::Observation::Log::LogEntry mEntries[kMaxEntries];
        unsigned int mEntryCount;
    };
}

class DiaAttributeConditionalModifiersLoggingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mSink.Clear();
        Dia::Observation::Log::Logger::Instance().RegisterThreadBuffer();
        Dia::Observation::Log::Logger::Instance().RegisterSink(&mSink);
    }

    void TearDown() override
    {
        Dia::Observation::Log::Logger::Instance().UnregisterSink(&mSink);
        Dia::Observation::Log::Logger::Instance().UnregisterThreadBuffer();
    }

    void FlushLogs()
    {
        Dia::Observation::Log::Logger::Instance().FlushSync();
    }

    ConditionalModifierLogSink mSink;
};

TEST_F(DiaAttributeConditionalModifiersLoggingTest, AC4_UnresolvableAccessor_LogsWarning)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    ConditionTestState state{};
    Dia::Condition::ConditionRegistry registry(&state); // empty
    set.SetConditionRegistry(&registry);

    FlushLogs();
    mSink.Clear();

    const char* whenCondition = R"({"op":">=","slot":"unknown_slot","field":"missing_field","value":1})";
    (void)set.AddModifier(MakeConditionalModifier("buff", "health", ModifierOperation::Add, 50.0f, whenCondition));

    FlushLogs();
    EXPECT_GE(mSink.CountByLevel(Dia::Observation::Log::LogLevel::kWarning), 1u);
}

TEST_F(DiaAttributeConditionalModifiersLoggingTest, AC5_MalformedJson_LogsWarning)
{
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet set = AttributeSet::CreateFromSchema(schema);

    ConditionTestState state{};
    Dia::Condition::ConditionRegistry registry(&state);
    ConfigureTestRegistry(registry);
    set.SetConditionRegistry(&registry);

    FlushLogs();
    mSink.Clear();

    const char* malformedJson = "{ this is not valid json ";
    (void)set.AddModifier(MakeConditionalModifier("buff", "health", ModifierOperation::Add, 50.0f, malformedJson));

    FlushLogs();
    EXPECT_GE(mSink.CountByLevel(Dia::Observation::Log::LogLevel::kWarning), 1u);
}
