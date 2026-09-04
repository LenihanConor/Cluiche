////////////////////////////////////////////////////////////////////////////////
// TestAttributeVisualDebugger.cpp
// Suite: AttributeVisualDebugger
//
// Covers AC-1..AC-5 of the DiaAttribute Visual Debugger feature:
//   AC-1: IDebugDomain identity (domain id, group, accent, panel-only)
//   AC-2: selected entity's attributes — resolved value, base value, clamp range
//   AC-3: modifier stack — name, operation, value, live when_condition state
//   AC-4: rebuilds are push-driven (OnAttributeChanged), not per-frame
//   AC-5: conditional modifiers are re-checked on a bounded poll cadence
//
// Feature spec: docs/specs/applications/dia/systems/diaattribute/visual-debugger.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaAttributeVisualDebugger/AttributeVisualDebugger.h>

#include <DiaAttribute/AttributeSchema.h>
#include <DiaAttribute/AttributeSet.h>
#include <DiaAttribute/AttributeSetComponent.h>

#include <DiaCondition/ConditionExpr.h>
#include <DiaCondition/ConditionRegistry.h>

#include <DiaEntity/Domain.h>
#include <DiaEntity/ComponentPool.h>

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <string.h>

using Dia::Attribute::AttributeModifier;
using Dia::Attribute::AttributeSchema;
using Dia::Attribute::AttributeSet;
using Dia::Attribute::AttributeSetComponent;
using Dia::Attribute::ModifierHandle;
using Dia::Attribute::ModifierOperation;
using Dia::AttributeVisualDebugger::AttributeVisualDebugger;
using Dia::Core::StringCRC;

// ===========================================================================
// Helpers
// ===========================================================================

namespace
{

Json::Value ParseJson(const char* str)
{
    Json::Value root;
    Json::Reader reader;
    reader.parse(str, root);
    return root;
}

// health: 0..2000 (default 1000), strength: 0..200 (default 50)
AttributeSchema MakeDragonSchema()
{
    const char* json =
        "{ \"schema_name\": \"dragon\","
        "  \"attributes\": ["
        "    { \"attribute_name\": \"health\",   \"minimum_value\": 0.0, \"maximum_value\": 2000.0, \"default_value\": 1000.0 },"
        "    { \"attribute_name\": \"strength\", \"minimum_value\": 0.0, \"maximum_value\": 200.0,  \"default_value\": 50.0 }"
        "  ] }";
    return AttributeSchema::LoadFromJsonValue(ParseJson(json));
}

// AttributeSet::AddModifier is [[nodiscard]]; none of these tests remove a modifier
// again, so the handle is deliberately dropped here rather than at every call site.
void AddMod(AttributeSet& set, const AttributeModifier& modifier)
{
    const ModifierHandle unused = set.AddModifier(modifier);
    (void)unused;
}

AttributeModifier MakeModifier(const char* name, const char* attr, ModifierOperation op, float value)
{
    AttributeModifier mod{};
    mod.modifier_name     = StringCRC(name);
    mod.attribute_name    = StringCRC(attr);
    mod.operation         = op;
    mod.value             = value;
    mod.when_condition[0] = '\0';
    return mod;
}

AttributeModifier MakeConditionalModifier(const char* name, const char* attr, ModifierOperation op,
                                          float value, const char* whenConditionJson)
{
    AttributeModifier mod = MakeModifier(name, attr, op, value);
    strncpy_s(mod.when_condition, sizeof(mod.when_condition), whenConditionJson, _TRUNCATE);
    return mod;
}

// Backing data for the ConditionRegistry accessors — one "actor" slot with a
// "ready" bool field, flipped directly by the AC-5 test.
struct ConditionTestState
{
    float health = 0.0f;
    bool  ready  = false;
};

float ReadHealthAccessor(void* data) { return static_cast<ConditionTestState*>(data)->health; }
bool  ReadReadyAccessor(void* data)  { return static_cast<ConditionTestState*>(data)->ready; }

void ConfigureTestRegistry(Dia::Condition::ConditionRegistry& registry)
{
    registry.RegisterFloat(StringCRC("actor"), StringCRC("health"), &ReadHealthAccessor);
    registry.RegisterBool (StringCRC("actor"), StringCRC("ready"),  &ReadReadyAccessor);
}

const char* kWhenReady = R"({"op":"==","slot":"actor","field":"ready","value":true})";

// Full harness: a Domain with the AttributeSetComponent pool registered, one entity
// carrying an initialized AttributeSet, a DebugLayerManager acting as the domain's
// IDebugContext, and the debugger itself.
//
// Declaration order matters: mState outlives mRegistry, which outlives mDomain (whose
// components own the parsed ConditionExprs that reference the registry).
struct Harness
{
    ConditionTestState                mState;
    Dia::Condition::ConditionRegistry mRegistry;
    Dia::Entity::Domain               mDomain;
    Dia::Debug::DebugLayerManager     mMgr;
    AttributeVisualDebugger           mDebugger;
    Dia::Entity::Entity               mEntity;
    AttributeSetComponent*            mComp = nullptr;

    Harness()
        : mRegistry(&mState)
        // Domain supplies both the selection resolution (GetAliveEntity) and the
        // component lookup, so the domain takes a single Domain&.
        , mDebugger(mDomain)
    {
        ConfigureTestRegistry(mRegistry);

        mDomain.RegisterPool(new Dia::Entity::ComponentPool<AttributeSetComponent>(AttributeSetComponent::kTypeId));

        mDebugger.Register(mMgr); // captures mMgr as the IDebugContext
    }

    // Creates an entity with an initialized AttributeSetComponent and selects it.
    void CreateSelectedAttributeEntity()
    {
        mEntity = mDomain.CreateEntity();
        mDomain.QueueAddComponent<AttributeSetComponent>(mEntity, Json::Value());
        mDomain.EndOfFrame();

        mComp = mDomain.GetComponent<AttributeSetComponent>(mEntity);
        EXPECT_NE(mComp, nullptr) << "harness could not attach an AttributeSetComponent";

        AttributeSchema schema = MakeDragonSchema();
        mComp->InitializeFromSchema(schema);
        mComp->GetAttributeSet().SetConditionRegistry(&mRegistry);

        Select(mEntity);
    }

    // Creates an entity with NO AttributeSetComponent and selects it.
    Dia::Entity::Entity CreateSelectedBareEntity()
    {
        Dia::Entity::Entity e = mDomain.CreateEntity();
        mDomain.EndOfFrame();
        Select(e);
        return e;
    }

    void Select(Dia::Entity::Entity e)
    {
        // IDebugContext contract: selected id is entity.GetIndex() + 1 (0 = none).
        mMgr.SetSelectedEntityId(e.GetIndex() + 1);
    }

    AttributeSet& Set() { return mComp->GetAttributeSet(); }

    Json::Value State()
    {
        Json::Value out;
        mDebugger.GetJSONState(out);
        return out;
    }
};

// Locates the attribute object named `name` in stats.attributes, or null if absent.
Json::Value FindAttribute(const Json::Value& state, const char* name)
{
    const Json::Value& attrs = state["stats"]["attributes"];
    for (Json::ArrayIndex i = 0; i < attrs.size(); ++i)
    {
        if (attrs[i]["name"].asString() == name)
            return attrs[i];
    }
    return Json::Value();
}

// Locates the modifier object named `name` within an attribute object, or null.
Json::Value FindModifier(const Json::Value& attribute, const char* name)
{
    const Json::Value& mods = attribute["modifiers"];
    for (Json::ArrayIndex i = 0; i < mods.size(); ++i)
    {
        if (mods[i]["modifierName"].asString() == name)
            return mods[i];
    }
    return Json::Value();
}

} // anonymous namespace

// ===========================================================================
// AC-1 — registers as an IDebugDomain and is discoverable by DiaDebugPanel
// ===========================================================================

TEST(AttributeVisualDebugger, AC1_Identity_ExposesCanonicalDomainMetadata)
{
    Harness h;

    EXPECT_EQ(h.mDebugger.GetDomainId(), StringCRC("Attribute"));
    EXPECT_STREQ(h.mDebugger.GetDisplayName(), "Attribute");
    EXPECT_EQ(h.mDebugger.GetGroup(), StringCRC("AIBehavior"));
    EXPECT_EQ(h.mDebugger.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kAIBehavior);
}

TEST(AttributeVisualDebugger, AC1_Identity_DescriptionIsNonEmptyAndWithin80Chars)
{
    Harness h;

    const char* desc = h.mDebugger.GetDescription();
    ASSERT_NE(desc, nullptr);
    EXPECT_GT(strlen(desc), 0u);
    EXPECT_LE(strlen(desc), 80u) << "IDebugDomain::GetDescription must stay <= 80 characters";
}

TEST(AttributeVisualDebugger, AC1_PanelOnly_NoWorldDrawers)
{
    Harness h;

    EXPECT_FALSE(h.mDebugger.HasWorldDrawers());
    EXPECT_EQ(h.mDebugger.GetDrawerCount(), 0);
    EXPECT_EQ(h.mDebugger.GetDrawer(0), nullptr);
}

TEST(AttributeVisualDebugger, AC1_GetJSONState_AlwaysEmitsDrawersAndStats)
{
    Harness h;

    Json::Value state = h.State();

    ASSERT_TRUE(state.isMember("drawers")) << "IDebugDomain contract: must always emit a drawers array";
    ASSERT_TRUE(state["drawers"].isArray());
    ASSERT_EQ(state["drawers"].size(), 1u);
    EXPECT_EQ(state["drawers"][0u]["name"].asString(), "AttributeInspector");
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());

    ASSERT_TRUE(state.isMember("stats")) << "IDebugDomain contract: must always emit a stats object";
}

// ===========================================================================
// AC-2 — selected entity's attributes: resolved value, base value, clamp range
// ===========================================================================

TEST(AttributeVisualDebugger, AC2_SelectedEntity_ReportsEveryAttributeValueBaseAndRange)
{
    Harness h;
    h.CreateSelectedAttributeEntity();

    Json::Value state = h.State();

    ASSERT_TRUE(state["stats"]["hasSelection"].asBool());
    ASSERT_TRUE(state["stats"]["hasAttributeSet"].asBool());
    ASSERT_TRUE(state["stats"]["attributes"].isArray());
    EXPECT_EQ(state["stats"]["attributes"].size(), 2u) << "schema defines health + strength";

    Json::Value health = FindAttribute(state, "health");
    ASSERT_FALSE(health.isNull()) << "health attribute missing from panel state";
    EXPECT_FLOAT_EQ(health["value"].asFloat(),     1000.0f);
    EXPECT_FLOAT_EQ(health["baseValue"].asFloat(), 1000.0f);
    EXPECT_FLOAT_EQ(health["minimum"].asFloat(),   0.0f);
    EXPECT_FLOAT_EQ(health["maximum"].asFloat(),   2000.0f);

    Json::Value strength = FindAttribute(state, "strength");
    ASSERT_FALSE(strength.isNull()) << "strength attribute missing from panel state";
    EXPECT_FLOAT_EQ(strength["value"].asFloat(),     50.0f);
    EXPECT_FLOAT_EQ(strength["baseValue"].asFloat(), 50.0f);
    EXPECT_FLOAT_EQ(strength["minimum"].asFloat(),   0.0f);
    EXPECT_FLOAT_EQ(strength["maximum"].asFloat(),   200.0f);
}

TEST(AttributeVisualDebugger, AC2_ResolvedValueDivergesFromBaseWhenModifiersApply)
{
    Harness h;
    h.CreateSelectedAttributeEntity();
    AddMod(h.Set(), MakeModifier("buff", "health", ModifierOperation::Add, 250.0f));

    Json::Value health = FindAttribute(h.State(), "health");
    ASSERT_FALSE(health.isNull());

    EXPECT_FLOAT_EQ(health["baseValue"].asFloat(), 1000.0f) << "base must remain unmodified";
    EXPECT_FLOAT_EQ(health["value"].asFloat(),     1250.0f) << "resolved must include the Add modifier";
}

TEST(AttributeVisualDebugger, AC2_NoSelection_ReportsNoSelectionWithoutCrashing)
{
    Harness h;
    h.CreateSelectedAttributeEntity();
    h.mMgr.SetSelectedEntityId(0); // 0 = nothing selected

    Json::Value state = h.State();

    EXPECT_FALSE(state["stats"]["hasSelection"].asBool());
    EXPECT_FALSE(state["stats"]["hasAttributeSet"].asBool());
    EXPECT_EQ(state["stats"]["attributes"].size(), 0u);
}

TEST(AttributeVisualDebugger, AC2_SelectedEntityWithoutAttributeSet_ReportsNoAttributeData)
{
    Harness h;
    h.CreateSelectedBareEntity();

    Json::Value state = h.State();

    EXPECT_TRUE (state["stats"]["hasSelection"].asBool());
    EXPECT_FALSE(state["stats"]["hasAttributeSet"].asBool())
        << "an entity with no AttributeSetComponent must report no attribute data, not assert";
    EXPECT_EQ(state["stats"]["attributes"].size(), 0u);
}

// ===========================================================================
// AC-3 — modifier stack: name, operation, value, live condition state
// ===========================================================================

TEST(AttributeVisualDebugger, AC3_ModifierStack_ReportsNameOperationAndValue)
{
    Harness h;
    h.CreateSelectedAttributeEntity();
    AddMod(h.Set(), MakeModifier("flatBuff",  "health", ModifierOperation::Add,      100.0f));
    AddMod(h.Set(), MakeModifier("scaleBuff", "health", ModifierOperation::Multiply, 1.5f));

    Json::Value health = FindAttribute(h.State(), "health");
    ASSERT_FALSE(health.isNull());
    ASSERT_TRUE(health["modifiers"].isArray());
    ASSERT_EQ(health["modifiers"].size(), 2u);

    Json::Value flat = FindModifier(health, "flatBuff");
    ASSERT_FALSE(flat.isNull());
    EXPECT_EQ(flat["operation"].asString(), "Add") << "operation must be human-readable, not a raw int";
    EXPECT_FLOAT_EQ(flat["value"].asFloat(), 100.0f);

    Json::Value scale = FindModifier(health, "scaleBuff");
    ASSERT_FALSE(scale.isNull());
    EXPECT_EQ(scale["operation"].asString(), "Multiply");
    EXPECT_FLOAT_EQ(scale["value"].asFloat(), 1.5f);
}

TEST(AttributeVisualDebugger, AC3_OverrideOperation_ReportedAsString)
{
    Harness h;
    h.CreateSelectedAttributeEntity();
    AddMod(h.Set(), MakeModifier("ovr", "strength", ModifierOperation::Override, 999.0f));

    Json::Value strength = FindAttribute(h.State(), "strength");
    ASSERT_FALSE(strength.isNull());

    Json::Value ovr = FindModifier(strength, "ovr");
    ASSERT_FALSE(ovr.isNull());
    EXPECT_EQ(ovr["operation"].asString(), "Override");
}

TEST(AttributeVisualDebugger, AC3_UnconditionalModifier_ReportedAsNotConditionalAndActive)
{
    Harness h;
    h.CreateSelectedAttributeEntity();
    AddMod(h.Set(), MakeModifier("always", "health", ModifierOperation::Add, 10.0f));

    Json::Value mod = FindModifier(FindAttribute(h.State(), "health"), "always");
    ASSERT_FALSE(mod.isNull());

    EXPECT_FALSE(mod["isConditional"].asBool());
    EXPECT_TRUE (mod["conditionTrue"].asBool()) << "an unconditional modifier always contributes";
}

TEST(AttributeVisualDebugger, AC3_ConditionalModifier_ReportsConditionFalseWhenGateIsFalse)
{
    Harness h;
    h.CreateSelectedAttributeEntity();
    h.mState.ready = false;
    AddMod(h.Set(), MakeConditionalModifier("readyBuff", "health", ModifierOperation::Add, 50.0f, kWhenReady));

    Json::Value mod = FindModifier(FindAttribute(h.State(), "health"), "readyBuff");
    ASSERT_FALSE(mod.isNull());

    EXPECT_TRUE (mod["isConditional"].asBool());
    EXPECT_FALSE(mod["conditionTrue"].asBool());
}

TEST(AttributeVisualDebugger, AC3_ConditionalModifier_ReportsConditionTrueWhenGateIsTrue)
{
    Harness h;
    h.CreateSelectedAttributeEntity();
    h.mState.ready = true;
    AddMod(h.Set(), MakeConditionalModifier("readyBuff", "health", ModifierOperation::Add, 50.0f, kWhenReady));

    Json::Value mod = FindModifier(FindAttribute(h.State(), "health"), "readyBuff");
    ASSERT_FALSE(mod.isNull());

    EXPECT_TRUE(mod["isConditional"].asBool());
    EXPECT_TRUE(mod["conditionTrue"].asBool());
}

// ===========================================================================
// AC-4 — rebuilds are push-driven (OnAttributeChanged), not per-frame
// ===========================================================================

TEST(AttributeVisualDebugger, AC4_FirstGetJSONState_BuildsExactlyOnce)
{
    Harness h;
    h.CreateSelectedAttributeEntity();

    EXPECT_EQ(h.mDebugger.GetRebuildCountForTesting(), 0u) << "nothing built before the first call";
    h.State();
    EXPECT_EQ(h.mDebugger.GetRebuildCountForTesting(), 1u);
}

TEST(AttributeVisualDebugger, AC4_RepeatedGetJSONState_WithoutMutation_DoesNotRebuild)
{
    Harness h;
    h.CreateSelectedAttributeEntity();

    h.State();
    const unsigned int afterFirst = h.mDebugger.GetRebuildCountForTesting();

    h.State();
    h.State();

    EXPECT_EQ(h.mDebugger.GetRebuildCountForTesting(), afterFirst)
        << "unchanged frames must reuse the cached tree, not recompute every frame";
}

TEST(AttributeVisualDebugger, AC4_MutationFiresChangeEvent_TriggersExactlyOneRebuild)
{
    Harness h;
    h.CreateSelectedAttributeEntity();

    h.State(); // subscribes the debugger to the selected entity's AttributeSet
    ASSERT_EQ(h.mDebugger.GetRebuildCountForTesting(), 1u);

    h.Set().SetBaseValue(StringCRC("health"), 1500.0f); // fires OnAttributeChanged -> marks dirty

    Json::Value state = h.State();
    EXPECT_EQ(h.mDebugger.GetRebuildCountForTesting(), 2u) << "the push event must trigger one rebuild";

    Json::Value health = FindAttribute(state, "health");
    ASSERT_FALSE(health.isNull());
    EXPECT_FLOAT_EQ(health["baseValue"].asFloat(), 1500.0f) << "rebuilt tree must show the new value";

    h.State();
    EXPECT_EQ(h.mDebugger.GetRebuildCountForTesting(), 2u) << "and must go quiet again afterwards";
}

TEST(AttributeVisualDebugger, AC4_AddModifierFiresChangeEvent_TriggersRebuild)
{
    Harness h;
    h.CreateSelectedAttributeEntity();

    h.State();
    const unsigned int before = h.mDebugger.GetRebuildCountForTesting();

    AddMod(h.Set(), MakeModifier("buff", "health", ModifierOperation::Add, 25.0f));

    Json::Value state = h.State();
    EXPECT_EQ(h.mDebugger.GetRebuildCountForTesting(), before + 1u);

    Json::Value health = FindAttribute(state, "health");
    ASSERT_FALSE(FindModifier(health, "buff").isNull()) << "the new modifier must appear after the rebuild";
}

TEST(AttributeVisualDebugger, AC4_SelectionChange_TriggersRebuild)
{
    Harness h;
    h.CreateSelectedAttributeEntity();

    h.State();
    const unsigned int before = h.mDebugger.GetRebuildCountForTesting();

    h.mMgr.SetSelectedEntityId(0); // selection cleared -> must rebuild, not serve a stale tree

    Json::Value state = h.State();
    EXPECT_EQ(h.mDebugger.GetRebuildCountForTesting(), before + 1u);
    EXPECT_FALSE(state["stats"]["hasSelection"].asBool());
}

// ===========================================================================
// AC-5 — bounded poll covers the condition-driven-change notification gap
// ===========================================================================

TEST(AttributeVisualDebugger, AC5_ConditionFlipWithNoMutation_IsPickedUpByTheBoundedPoll)
{
    Harness h;
    h.CreateSelectedAttributeEntity();
    h.mState.ready = false;
    AddMod(h.Set(), MakeConditionalModifier("readyBuff", "health", ModifierOperation::Add, 50.0f, kWhenReady));

    // Frame 1 — condition is false, buff excluded.
    {
        Json::Value mod = FindModifier(FindAttribute(h.State(), "health"), "readyBuff");
        ASSERT_FALSE(mod.isNull());
        ASSERT_FALSE(mod["conditionTrue"].asBool());
    }
    const unsigned int afterFirst = h.mDebugger.GetRebuildCountForTesting();

    // Flip the gate directly. This fires NO change notification — the documented
    // Feature 3 gap. A push-only overlay would stay blind to it forever.
    h.mState.ready = true;

    // Well inside the poll interval the panel still serves the cached (stale) tree —
    // this is the bounded-staleness contract, and proves the cache is real.
    h.State();
    h.State();
    EXPECT_EQ(h.mDebugger.GetRebuildCountForTesting(), afterFirst)
        << "the poll must not fire every frame";
    EXPECT_FALSE(FindModifier(FindAttribute(h.State(), "health"), "readyBuff")["conditionTrue"].asBool())
        << "still within the poll interval — cached state expected";

    // Drive past one poll boundary.
    for (unsigned int i = 0; i < AttributeVisualDebugger::kConditionalPollIntervalFrames; ++i)
        h.State();

    Json::Value mod = FindModifier(FindAttribute(h.State(), "health"), "readyBuff");
    ASSERT_FALSE(mod.isNull());
    EXPECT_TRUE(mod["conditionTrue"].asBool())
        << "the bounded poll must have re-evaluated the when_condition and refreshed the panel";

    // Exactly one extra rebuild: the poll that detected the flip. Emphatically not one per frame.
    EXPECT_EQ(h.mDebugger.GetRebuildCountForTesting(), afterFirst + 1u)
        << "the poll must rebuild only when a condition actually flipped";
}

TEST(AttributeVisualDebugger, AC5_ResolvedValueRefreshesAfterConditionFlipPoll)
{
    Harness h;
    h.CreateSelectedAttributeEntity();
    h.mState.ready = false;
    AddMod(h.Set(), MakeConditionalModifier("readyBuff", "health", ModifierOperation::Add, 50.0f, kWhenReady));

    ASSERT_FLOAT_EQ(FindAttribute(h.State(), "health")["value"].asFloat(), 1000.0f);

    h.mState.ready = true;
    for (unsigned int i = 0; i <= AttributeVisualDebugger::kConditionalPollIntervalFrames; ++i)
        h.State();

    EXPECT_FLOAT_EQ(FindAttribute(h.State(), "health")["value"].asFloat(), 1050.0f)
        << "the conditional Add must be included once the poll observed the flip";
}

TEST(AttributeVisualDebugger, AC5_StableConditions_DoNotTriggerPollRebuilds)
{
    Harness h;
    h.CreateSelectedAttributeEntity();
    h.mState.ready = true;
    AddMod(h.Set(), MakeConditionalModifier("readyBuff", "health", ModifierOperation::Add, 50.0f, kWhenReady));

    h.State();
    const unsigned int afterFirst = h.mDebugger.GetRebuildCountForTesting();

    // Several poll boundaries, condition never changes -> no rebuilds at all.
    for (unsigned int i = 0; i < AttributeVisualDebugger::kConditionalPollIntervalFrames * 3u; ++i)
        h.State();

    EXPECT_EQ(h.mDebugger.GetRebuildCountForTesting(), afterFirst)
        << "a poll that detects no change must not rebuild";
}

// ===========================================================================
// OnCommand — toggle contract, no crash on unknown commands
// ===========================================================================

TEST(AttributeVisualDebugger, OnCommand_ToggleAttributeInspector_FlipsEnabledFlag)
{
    Harness h;
    h.CreateSelectedAttributeEntity();

    ASSERT_TRUE(h.State()["drawers"][0u]["enabled"].asBool());

    Json::Value args(Json::objectValue);
    args["drawer"] = "AttributeInspector";
    h.mDebugger.OnCommand(StringCRC("toggle"), args);

    EXPECT_FALSE(h.State()["drawers"][0u]["enabled"].asBool());

    h.mDebugger.OnCommand(StringCRC("toggle"), args);
    EXPECT_TRUE(h.State()["drawers"][0u]["enabled"].asBool()) << "double toggle restores";
}

TEST(AttributeVisualDebugger, OnCommand_UnknownCommandAndMalformedArgs_AreNoOps)
{
    Harness h;
    h.CreateSelectedAttributeEntity();

    h.mDebugger.OnCommand(StringCRC("setScale"), Json::Value(Json::objectValue));
    h.mDebugger.OnCommand(StringCRC("nonsense"), Json::Value(Json::objectValue));
    h.mDebugger.OnCommand(StringCRC("toggle"),   Json::Value(Json::objectValue)); // missing "drawer"

    Json::Value args(Json::objectValue);
    args["drawer"] = 42; // wrong type
    h.mDebugger.OnCommand(StringCRC("toggle"), args);

    EXPECT_TRUE(h.State()["drawers"][0u]["enabled"].asBool()) << "no malformed command may flip state";
}

#endif // DIA_DEBUG
