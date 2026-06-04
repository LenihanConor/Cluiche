#include <gtest/gtest.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/ComponentPool.h>
#include <DiaEntity/JsonBlueprintLoader.h>
#include "BpTransformComponent.h"

using namespace Dia::Entity;
using namespace DiaEntityTest;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void RegisterBlueprintPools(Domain& domain) {
    domain.RegisterPool(new ComponentPool<BpTransform>(BpTransform::kTypeId));
    domain.RegisterPool(new ComponentPool<BpHealth>(BpHealth::kTypeId));
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(DiaEntityBlueprintLoader, LoadEmptyBlueprint_Succeeds) {
    Domain domain;
    RegisterBlueprintPools(domain);

    Json::Value bp;
    bp["version"]  = 1;
    bp["entities"] = Json::Value(Json::objectValue);

    JsonBlueprintLoader loader;
    EXPECT_TRUE(loader.Load(domain, bp));
}

TEST(DiaEntityBlueprintLoader, LoadSingleEntity_CreatesEntity) {
    Domain domain;
    RegisterBlueprintPools(domain);

    Json::Value bp;
    bp["version"]          = 1;
    bp["entities"]["hero"] = Json::Value(Json::objectValue);

    JsonBlueprintLoader loader;
    EXPECT_TRUE(loader.Load(domain, bp));

    domain.EndOfFrame();

    // One entity created: index=0, generation=1.
    Entity e0(0, 1);
    EXPECT_TRUE(domain.IsAlive(e0));
}

TEST(DiaEntityBlueprintLoader, LoadEntityWithComponent_FieldsLoaded) {
    Domain domain;
    RegisterBlueprintPools(domain);

    Json::Value compCfg;
    compCfg["x"] = 3.0f;
    compCfg["y"] = 7.5f;

    Json::Value bp;
    bp["version"]                            = 1;
    bp["entities"]["solo"]["bp-transform"]   = compCfg;

    JsonBlueprintLoader loader;
    EXPECT_TRUE(loader.Load(domain, bp));
    domain.EndOfFrame();

    // HandlePool: first entity allocated gets index=0, generation=1 (0 is even=dead, bumped to 1=live).
    Entity e0(0, 1);
    EXPECT_TRUE(domain.IsAlive(e0));
    EXPECT_TRUE(domain.HasComponent<BpTransform>(e0));

    BpTransform* t = domain.GetComponent<BpTransform>(e0);
    ASSERT_NE(t, nullptr);
    EXPECT_FLOAT_EQ(t->x, 3.0f);
    EXPECT_FLOAT_EQ(t->y, 7.5f);
}

TEST(DiaEntityBlueprintLoader, LoadMultipleEntities_AllAlive) {
    Domain domain;
    RegisterBlueprintPools(domain);

    Json::Value bp;
    bp["version"]               = 1;
    bp["entities"]["e1"]        = Json::Value(Json::objectValue);
    bp["entities"]["e2"]        = Json::Value(Json::objectValue);
    bp["entities"]["e3"]        = Json::Value(Json::objectValue);

    JsonBlueprintLoader loader;
    EXPECT_TRUE(loader.Load(domain, bp));

    domain.EndOfFrame();

    // Three entities created at indices 0..2; first use gets generation=1.
    EXPECT_TRUE(domain.IsAlive(Entity(0, 1)));
    EXPECT_TRUE(domain.IsAlive(Entity(1, 1)));
    EXPECT_TRUE(domain.IsAlive(Entity(2, 1)));
}

TEST(DiaEntityBlueprintLoader, LoadUnknownComponentType_Skips) {
    Domain domain;
    RegisterBlueprintPools(domain);

    Json::Value bp;
    bp["version"]                               = 1;
    bp["entities"]["hero"]["unknown-component"] = Json::Value(Json::objectValue);

    JsonBlueprintLoader loader;
    // Unknown component should be skipped gracefully; load should still succeed.
    EXPECT_TRUE(loader.Load(domain, bp));

    domain.EndOfFrame();

    // Entity was still created (index=0, generation=1).
    Entity e0(0, 1);
    EXPECT_TRUE(domain.IsAlive(e0));
    // But it has no components (unknown type was skipped).
    EXPECT_FALSE(domain.HasComponent<BpTransform>(e0));
}

TEST(DiaEntityBlueprintLoader, LoadVersionMismatch_ReturnsFalse) {
    Domain domain;
    RegisterBlueprintPools(domain);

    Json::Value bp;
    bp["version"]  = 99;
    bp["entities"] = Json::Value(Json::objectValue);

    JsonBlueprintLoader loader;
    EXPECT_FALSE(loader.Load(domain, bp));
}

TEST(DiaEntityBlueprintLoader, LoadMissingVersionField_ReturnsFalse) {
    Domain domain;
    RegisterBlueprintPools(domain);

    Json::Value bp;
    bp["entities"] = Json::Value(Json::objectValue);

    JsonBlueprintLoader loader;
    EXPECT_FALSE(loader.Load(domain, bp));
}

TEST(DiaEntityBlueprintLoader, LoadReferencesBlock_Ignored) {
    Domain domain;
    RegisterBlueprintPools(domain);

    Json::Value ref;
    ref["entity"]    = "player";
    ref["component"] = "bp-health";
    ref["field"]     = "swordRef";
    ref["ref"]       = "sword";

    Json::Value bp;
    bp["version"]            = 1;
    bp["entities"]["player"] = Json::Value(Json::objectValue);

    Json::Value refs(Json::arrayValue);
    refs.append(ref);
    bp["references"] = refs;

    JsonBlueprintLoader loader;
    // References block present but F4 is not built — should succeed with a warning.
    EXPECT_TRUE(loader.Load(domain, bp));
}

TEST(DiaEntityBlueprintLoader, LoadMultipleComponents_AllAttached) {
    Domain domain;
    RegisterBlueprintPools(domain);

    Json::Value transformCfg;
    transformCfg["x"] = 1.0f;
    transformCfg["y"] = 2.0f;

    Json::Value healthCfg;
    healthCfg["maxHp"] = 250;

    Json::Value bp;
    bp["version"]                            = 1;
    bp["entities"]["boss"]["bp-transform"]   = transformCfg;
    bp["entities"]["boss"]["bp-health"]      = healthCfg;

    JsonBlueprintLoader loader;
    EXPECT_TRUE(loader.Load(domain, bp));

    domain.EndOfFrame();

    // First entity: index=0, generation=1.
    Entity e0(0, 1);
    EXPECT_TRUE(domain.IsAlive(e0));
    EXPECT_TRUE(domain.HasComponent<BpTransform>(e0));
    EXPECT_TRUE(domain.HasComponent<BpHealth>(e0));

    BpTransform* t = domain.GetComponent<BpTransform>(e0);
    ASSERT_NE(t, nullptr);
    EXPECT_FLOAT_EQ(t->x, 1.0f);
    EXPECT_FLOAT_EQ(t->y, 2.0f);

    BpHealth* h = domain.GetComponent<BpHealth>(e0);
    ASSERT_NE(h, nullptr);
    EXPECT_EQ(h->maxHp, 250);
}
