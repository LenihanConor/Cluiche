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

#include <DiaAttribute/AttributeAccessorBridge.h>
#include <DiaAttribute/AttributeSchema.h>
#include <DiaAttribute/AttributeSet.h>
#include <DiaAttribute/AttributeSetComponent.h>

#include <DiaCondition/ConditionExpr.h>
#include <DiaCondition/ConditionRegistry.h>

#include <DiaEntity/Domain.h>
#include <DiaEntity/ComponentPool.h>

#include <DiaSaveGame/SaveContext.h>
#include <DiaSaveGame/LoadContext.h>

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <stdio.h>
#include <string.h>
#include <string>

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

// ===========================================================================
// Bug fix — use-after-free in RebindObserver when the previously-observed
// entity/component was destroyed since the last GetJSONState() call.
//
// RebindObserver used to unconditionally dereference mObservedComponent to
// unsubscribe the debugger, with no liveness check on the entity that owned it.
// If that entity (and its AttributeSetComponent) was destroyed since the last
// GetJSONState() call, mObservedComponent is a dangling/stale pointer — the
// component's storage may have been destructed and its pool slot recycled.
// The fix tracks mObservedEntity alongside mObservedComponent and only
// dereferences the previous component if Domain::GetAliveEntity confirms it is
// still alive with the same generation.
//
// NOTE ON PROVING THIS RED (see task report for the full account): this
// codebase's ComponentPool/HandlePool storage is a fixed-size byte array (no
// heap), and AttributeObserverSubject's own storage is a fixed-size
// DynamicArrayC<IAttributeObserver*, 16> (also no heap). HandlePool::Free()
// calls ~T() in place but does not poison/zero the freed bytes. So dereferencing
// the stale pointer in a short-lived, single-threaded test reads
// already-destructed-but-still-resident memory — undefined behaviour, but not
// memory that is reliably unmapped/poisoned/overwritten by anything else before
// this test observes it. This test therefore cannot and does not claim to
// reliably crash on the pre-fix code; run against the pre-fix code it completed
// without crashing (quoted in the task report). What IS asserted, on both the
// pre-fix and post-fix code, is the observable state-based contract: the
// destroy+reselect sequence must not corrupt the debugger's bookkeeping, must
// rebuild exactly once per real selection change, and must leave the domain
// able to cleanly observe a fresh, unrelated entity afterwards.
// ===========================================================================

TEST(AttributeVisualDebugger, Bugfix_ObservedEntityDestroyed_ThenDifferentEntitySelected_NoCrash)
{
    Harness h;
    h.CreateSelectedAttributeEntity(); // entity A, selected

    h.State(); // subscribes the debugger to A's AttributeSet
    ASSERT_EQ(h.mDebugger.GetRebuildCountForTesting(), 1u);

    // Destroy A. Applied at EndOfFrame: its AttributeSetComponent (and the AttributeSet
    // the debugger is subscribed to) is detached and its pool slot freed.
    h.mDomain.QueueDestroy(h.mEntity);
    h.mDomain.EndOfFrame();

    // Select a DIFFERENT entity with no AttributeSetComponent. ResolveSelectedEntity
    // correctly returns Invalid() for A's dead slot; this bare entity resolves to a
    // nullptr component. comp (nullptr) != mObservedComponent (still A's stale pointer),
    // so RebindObserver is invoked on exactly the buggy code path.
    Dia::Entity::Entity entityB = h.CreateSelectedBareEntity();

    Json::Value state;
    ASSERT_NO_FATAL_FAILURE(state = h.State())
        << "must not crash when the previously-observed entity was destroyed";

    // B is selected but carries no AttributeSetComponent.
    EXPECT_TRUE (state["stats"]["hasSelection"].asBool());
    EXPECT_FALSE(state["stats"]["hasAttributeSet"].asBool());
    EXPECT_EQ(state["stats"]["attributes"].size(), 0u);

    // Exactly one rebuild for this call — the selection change is real and must not be
    // served from A's now-meaningless cached tree.
    EXPECT_EQ(h.mDebugger.GetRebuildCountForTesting(), 2u);

    // The debugger must now be cleanly unbound (no bookkeeping left half-updated by a
    // skipped/mishandled Unsubscribe) and able to observe a THIRD, unrelated, live entity
    // from a clean slate.
    h.CreateSelectedAttributeEntity(); // entity C, fresh AttributeSetComponent, selected
    h.Set().SetBaseValue(StringCRC("health"), 1800.0f); // must fire OnAttributeChanged normally

    Json::Value stateC = h.State();
    EXPECT_TRUE(stateC["stats"]["hasAttributeSet"].asBool());
    Json::Value health = FindAttribute(stateC, "health");
    ASSERT_FALSE(health.isNull());
    EXPECT_FLOAT_EQ(health["baseValue"].asFloat(), 1800.0f);

    (void)entityB;
}

// ===========================================================================
// Coverage gap 14 — kMaxTrackedConditionalModifiers (64) cache-cap boundary.
// RefreshConditionalCache's `if (mConditionCache.IsFull()) return;` truncation
// had no test. RebuildJSONState walks every attribute/modifier unconditionally
// (the cap only bounds the internal poll-diff cache), so the panel must still
// report every attribute correctly even when the live conditional-modifier
// count exceeds the cap.
// ===========================================================================

TEST(AttributeVisualDebugger, Coverage_ConditionCacheCap_ExceedsMaxTrackedConditionalModifiers_NoCrash)
{
    Harness h;

    const unsigned int kAttrCount = 70; // > kMaxTrackedConditionalModifiers (64)

    std::string json = "{ \"schema_name\": \"wide\", \"attributes\": [";
    for (unsigned int i = 0; i < kAttrCount; ++i)
    {
        char entry[192];
        snprintf(entry, sizeof(entry),
            "%s{ \"attribute_name\": \"attr_%02u\", \"minimum_value\": 0.0, \"maximum_value\": 10000.0, \"default_value\": %u.0 }",
            (i == 0) ? "" : ",", i, i);
        json += entry;
    }
    json += "] }";
    AttributeSchema wideSchema = AttributeSchema::LoadFromJsonValue(ParseJson(json.c_str()));

    Dia::Entity::Entity entity = h.mDomain.CreateEntity();
    h.mDomain.QueueAddComponent<AttributeSetComponent>(entity, Json::Value());
    h.mDomain.EndOfFrame();

    AttributeSetComponent* comp = h.mDomain.GetComponent<AttributeSetComponent>(entity);
    ASSERT_NE(comp, nullptr);
    comp->InitializeFromSchema(wideSchema);
    comp->GetAttributeSet().SetConditionRegistry(&h.mRegistry);

    h.mState.ready = true; // condition true for every conditional modifier below

    // One conditional modifier per attribute — 70 conditional modifiers total, exceeding
    // kMaxTrackedConditionalModifiers (64). Same "generate N unique names programmatically"
    // technique TestDiaAttributeAccessorBridge.cpp uses for its own >64 cap test.
    for (unsigned int i = 0; i < kAttrCount; ++i)
    {
        char attrName[32];
        snprintf(attrName, sizeof(attrName), "attr_%02u", i);
        char modName[32];
        snprintf(modName, sizeof(modName), "mod_%02u", i);
        AddMod(comp->GetAttributeSet(),
               MakeConditionalModifier(modName, attrName, ModifierOperation::Add, 1.0f, kWhenReady));
    }

    h.Select(entity);

    Json::Value state;
    ASSERT_NO_FATAL_FAILURE(state = h.State());

    ASSERT_TRUE(state["stats"]["hasAttributeSet"].asBool());
    EXPECT_EQ(state["stats"]["attributes"].size(), kAttrCount)
        << "RebuildJSONState walks every attribute regardless of the condition-cache cap";

    // Every attribute's single conditional modifier must still be reported correctly — the
    // 64 cap bounds only the internal poll-diff cache, not the displayed JSON tree.
    for (unsigned int i = 0; i < kAttrCount; ++i)
    {
        char attrName[32];
        snprintf(attrName, sizeof(attrName), "attr_%02u", i);
        Json::Value attr = FindAttribute(state, attrName);
        ASSERT_FALSE(attr.isNull()) << "attribute " << attrName << " missing";
        ASSERT_EQ(attr["modifiers"].size(), 1u) << attrName;
        EXPECT_TRUE(attr["modifiers"][0u]["isConditional"].asBool()) << attrName;
        EXPECT_TRUE(attr["modifiers"][0u]["conditionTrue"].asBool()) << attrName;
    }

    // PollConditionalModifiersChanged also walks the live set against the truncated cache —
    // must not crash even though more than 64 conditional modifiers exist.
    for (unsigned int i = 0; i < AttributeVisualDebugger::kConditionalPollIntervalFrames; ++i)
        ASSERT_NO_FATAL_FAILURE(h.State());
}

// ===========================================================================
// Coverage gap 15 — disabled-drawer mutation handling. Mutating the underlying
// AttributeSet while AttributeInspector is toggled off must not be lost: the
// panel shows nothing while disabled (RebuildJSONState's mLastEnabledSeen
// guard), but re-enabling must reveal the mutation that happened while hidden.
// ===========================================================================

TEST(AttributeVisualDebugger, Coverage_MutationWhileDisabled_ReflectedOnceReenabled)
{
    Harness h;
    h.CreateSelectedAttributeEntity();

    h.State(); // subscribes; builds with the drawer enabled

    Json::Value args(Json::objectValue);
    args["drawer"] = "AttributeInspector";
    h.mDebugger.OnCommand(StringCRC("toggle"), args); // disable

    Json::Value disabledState = h.State();
    EXPECT_FALSE(disabledState["drawers"][0u]["enabled"].asBool());
    EXPECT_EQ(disabledState["stats"]["attributes"].size(), 0u)
        << "nothing is displayed while the drawer is toggled off";

    // Mutate while disabled — fires OnAttributeChanged, marks dirty.
    h.Set().SetBaseValue(StringCRC("health"), 1750.0f);

    Json::Value stillDisabled = h.State();
    EXPECT_EQ(stillDisabled["stats"]["attributes"].size(), 0u)
        << "still disabled — a rebuild happens but RebuildJSONState's mLastEnabledSeen guard "
           "skips the attribute walk";

    // Re-enable.
    h.mDebugger.OnCommand(StringCRC("toggle"), args);

    Json::Value reenabled = h.State();
    EXPECT_TRUE(reenabled["drawers"][0u]["enabled"].asBool());
    Json::Value health = FindAttribute(reenabled, "health");
    ASSERT_FALSE(health.isNull());
    EXPECT_FLOAT_EQ(health["baseValue"].asFloat(), 1750.0f)
        << "the mutation that happened while hidden must be visible once re-enabled";
}

// ===========================================================================
// Coverage gap 16 — a deserialized AttributeSet fed into the debugger. Proves
// the debugger works correctly against freshly-issued handles and heap-owned
// parsed_condition pointers from a Deserialize path, not just direct
// AddModifier calls.
// ===========================================================================

TEST(AttributeVisualDebugger, Coverage_DeserializedAttributeSet_ReportsCorrectlyWithNoCrash)
{
    Harness h;

    // Build + populate a standalone source set (mirrors TestDiaAttributeSaveSerialization.cpp).
    AttributeSchema schema = MakeDragonSchema();
    AttributeSet source = AttributeSet::CreateFromSchema(schema);

    Dia::Condition::ConditionRegistry sourceRegistry(&h.mState);
    ConfigureTestRegistry(sourceRegistry);
    source.SetConditionRegistry(&sourceRegistry);

    source.SetBaseValue(StringCRC("health"), 1300.0f);
    AddMod(source, MakeModifier("flatBuff", "health", ModifierOperation::Add, 100.0f));

    h.mState.ready = true;
    AddMod(source, MakeConditionalModifier("readyBuff", "strength", ModifierOperation::Add, 20.0f, kWhenReady));

    Dia::SaveGame::SaveContext save;
    source.Serialize(save);

    static char buf[Dia::SaveGame::SaveContext::kBufferSize];
    ASSERT_TRUE(save.Flush(buf, sizeof(buf)));

    Json::Value reparsed;
    Json::Reader reader;
    ASSERT_TRUE(reader.parse(buf, reparsed));

    // Deserialize into a FRESH AttributeSetComponent attached to an entity in the harness's
    // Domain — proves the debugger works against deserialize-issued handles and heap-owned
    // parsed_condition pointers, not just direct AddModifier calls.
    Dia::Entity::Entity entity = h.mDomain.CreateEntity();
    h.mDomain.QueueAddComponent<AttributeSetComponent>(entity, Json::Value());
    h.mDomain.EndOfFrame();

    AttributeSetComponent* comp = h.mDomain.GetComponent<AttributeSetComponent>(entity);
    ASSERT_NE(comp, nullptr);
    comp->InitializeFromSchema(schema);
    comp->GetAttributeSet().SetConditionRegistry(&h.mRegistry); // must be set BEFORE Deserialize

    Dia::SaveGame::LoadContext load(reparsed);
    comp->GetAttributeSet().Deserialize(load);

    h.Select(entity);

    Json::Value state;
    ASSERT_NO_FATAL_FAILURE(state = h.State());
    ASSERT_TRUE(state["stats"]["hasAttributeSet"].asBool());

    Json::Value health = FindAttribute(state, "health");
    ASSERT_FALSE(health.isNull());
    EXPECT_FLOAT_EQ(health["baseValue"].asFloat(), 1300.0f);
    Json::Value flat = FindModifier(health, "flatBuff");
    ASSERT_FALSE(flat.isNull());
    EXPECT_EQ(flat["operation"].asString(), "Add");
    EXPECT_FLOAT_EQ(flat["value"].asFloat(), 100.0f);

    Json::Value strength = FindAttribute(state, "strength");
    ASSERT_FALSE(strength.isNull());
    Json::Value ready = FindModifier(strength, "readyBuff");
    ASSERT_FALSE(ready.isNull());
    EXPECT_TRUE(ready["isConditional"].asBool());
    EXPECT_TRUE(ready["conditionTrue"].asBool())
        << "h.mState.ready == true at both deserialize time and registry-read time";
}

// ===========================================================================
// Coverage gap 17 — AttributeAccessorBridge and the visual debugger observing
// the same AttributeSet. Confirms the bridge's own live read
// (registry.GetFloat) and the debugger's panel state agree after a
// condition-driven flip.
//
// The gating condition deliberately depends on an EXTERNAL accessor (the
// harness's actor.ready, backed by ConditionTestState) rather than one of the
// bridge's own self.* attributes. This keeps the test focused on the actual
// scenario it's proving — the bridge's direct read and the debugger's panel
// state agreeing after a condition-driven flip — without also exercising
// AttributeSet::ResolveValue's per-slot reentrancy guard (see AttributeSet.h/
// .cpp and DiaAttributeAccessorBridge.AC4_BridgedSetGatesItsOwnConditionalModifier,
// which covers that cross-attribute-same-set case directly). The bridge
// registry below is intentionally a SECOND, independent ConditionRegistry
// (data = &h.Set()) used only for direct registry.GetFloat reads, never
// installed via SetConditionRegistry.
// ===========================================================================

TEST(AttributeVisualDebugger, Coverage_AccessorBridgeAndVisualDebugger_AgreeOnSameAttributeSet)
{
    Harness h;
    h.CreateSelectedAttributeEntity(); // wires h.mRegistry (data=h.mState) as the condition registry
    h.mState.ready = false;

    // A SEPARATE registry whose `data` pointer IS the observed AttributeSet — required by
    // AttributeAccessorBridge's precondition. Used only for direct registry.GetFloat reads
    // below (mirroring how DiaRules/DiaUtilityAI would read it) — never installed via
    // SetConditionRegistry, so resolving `health` never invokes a bridge accessor.
    Dia::Condition::ConditionRegistry bridgeRegistry(&h.Set());
    Dia::Attribute::AttributeAccessorBridge::RegisterAccessors(bridgeRegistry, h.Set(), StringCRC("self"));

    // Gated on the EXTERNAL actor.ready accessor (h.mRegistry), not on any self.* bridged
    // attribute — see the reentrancy note above.
    AddMod(h.Set(), MakeConditionalModifier("readyBuff", "health", ModifierOperation::Add, 300.0f, kWhenReady));

    // Baseline — condition false, both reads agree.
    {
        Json::Value health = FindAttribute(h.State(), "health");
        ASSERT_FALSE(health.isNull());
        EXPECT_FLOAT_EQ(health["value"].asFloat(), 1000.0f);
        EXPECT_FLOAT_EQ(bridgeRegistry.GetFloat(StringCRC("self"), StringCRC("health")), 1000.0f);
    }

    // Flip the external gate directly. This fires NO change notification (the documented
    // Feature 3 gap) — only the bounded poll (AC-5) picks it up.
    h.mState.ready = true;

    // Wait out a full poll interval so the assertion holds regardless of whether the push
    // path or the bounded poll is what actually picked up the flip.
    for (unsigned int i = 0; i <= AttributeVisualDebugger::kConditionalPollIntervalFrames; ++i)
        h.State();

    Json::Value health = FindAttribute(h.State(), "health");
    ASSERT_FALSE(health.isNull());
    EXPECT_FLOAT_EQ(health["value"].asFloat(), 1300.0f);
    EXPECT_TRUE(FindModifier(health, "readyBuff")["conditionTrue"].asBool());

    EXPECT_FLOAT_EQ(bridgeRegistry.GetFloat(StringCRC("self"), StringCRC("health")), 1300.0f)
        << "the bridge's own read (used by DiaRules/DiaUtilityAI) must agree with the panel";
}

// ===========================================================================
// Coverage gap 18 — a second, independent IAttributeObserver coexists with the
// debugger's own subscription on the same AttributeSet.
// ===========================================================================

namespace
{
    class CountingAttributeObserver : public Dia::Attribute::IAttributeObserver
    {
    public:
        void OnAttributeChanged(const Dia::Attribute::AttributeChangedEvent&) override { ++changeCount; }
        unsigned int changeCount = 0;
    };
}

TEST(AttributeVisualDebugger, Coverage_SecondIndependentObserver_CoexistsWithDebuggerSubscription)
{
    Harness h;
    h.CreateSelectedAttributeEntity();

    h.State(); // subscribes the debugger

    CountingAttributeObserver second;
    h.Set().GetObserverSubject().Subscribe(&second);

    h.Set().SetBaseValue(StringCRC("health"), 1600.0f);

    EXPECT_EQ(second.changeCount, 1u) << "the second observer must see the mutation too";

    Json::Value state = h.State();
    EXPECT_EQ(h.mDebugger.GetRebuildCountForTesting(), 2u)
        << "the debugger's own rebuild must also have fired — both observers coexist on one subject";

    Json::Value health = FindAttribute(state, "health");
    ASSERT_FALSE(health.isNull());
    EXPECT_FLOAT_EQ(health["baseValue"].asFloat(), 1600.0f);

    h.Set().GetObserverSubject().Unsubscribe(&second);
}

#endif // DIA_DEBUG
