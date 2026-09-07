////////////////////////////////////////////////////////////////////////////////
// TestEntitySpatialVisualDebugger.cpp
// IDebugDomain for EntitySpatial — identity, JSON state, OnCommand.
// System spec: docs/specs/applications/dia/systems/diadebugdomain/diadebugdomain.md
////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <DiaEntitySpatialVisualDebugger/EntitySpatialVisualDebugger.h>
#include <DiaEntitySpatial/EntitySpatialModule.h>
#include <DiaEntitySpatial/EntitySpatialIndex.h>
#include <DiaEntitySpatial/SpatialComponent.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/ComponentPool.h>
#include <DiaDebugDraw/Domain/DebugGroupAccents.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>
#include <DiaMaths/Vector/Vector2D.h>

#include <cstring>
#include <memory>

using namespace Dia::EntitySpatial;

namespace
{

EntitySpatialIndex::SquareDef MakeSquareDef()
{
    EntitySpatialIndex::SquareDef def;
    def.worldBounds = Dia::Geometry2D::AARect(
        Dia::Maths::Vector2D(-100.f, -100.f),
        Dia::Maths::Vector2D( 100.f,  100.f));
    def.cellSize = 10.f;
    return def;
}

struct Fixture
{
    Dia::Entity::Domain           domain;
    EntitySpatialIndex::SquareDef def = MakeSquareDef();
    std::unique_ptr<EntitySpatialModule> module;

    Fixture()
    {
        domain.RegisterPool(
            new Dia::Entity::ComponentPool<SpatialComponent>(SpatialComponent::kTypeId));
        module = std::make_unique<EntitySpatialModule>(domain, def);
    }
};

Json::Value ToggleArgs(const char* drawerName)
{
    Json::Value args;
    args["drawer"] = drawerName;
    return args;
}

Json::Value ScaleArgs(const char* key, float value)
{
    Json::Value args;
    args["key"]   = key;
    args["value"] = value;
    return args;
}

} // namespace

// ===========================================================================
// Identity
// ===========================================================================

TEST(EntitySpatialVisualDebugger_Identity, DomainIdIsEntitySpatial)
{
    Fixture f;
    EntitySpatialVisualDebugger domain(*f.module, f.domain, f.def);

    EXPECT_EQ(domain.GetDomainId(), Dia::Core::StringCRC("entityspatial"));
    EXPECT_STREQ(domain.GetDisplayName(), "Entity Spatial");
}

TEST(EntitySpatialVisualDebugger_Identity, GroupIsEntity)
{
    Fixture f;
    EntitySpatialVisualDebugger domain(*f.module, f.domain, f.def);

    EXPECT_EQ(domain.GetGroup(), Dia::Core::StringCRC("Entity"));
    EXPECT_EQ(domain.GetAccentColour(), Dia::VisualDebugger::DebugGroupAccents::kEntity);
}

TEST(EntitySpatialVisualDebugger_Identity, HasWorldDrawersTrue)
{
    Fixture f;
    EntitySpatialVisualDebugger domain(*f.module, f.domain, f.def);

    EXPECT_TRUE(domain.HasWorldDrawers());
    EXPECT_EQ(domain.GetDrawerCount(), 3);
}

TEST(EntitySpatialVisualDebugger_Identity, DescriptionWithin80Chars)
{
    Fixture f;
    EntitySpatialVisualDebugger domain(*f.module, f.domain, f.def);

    ASSERT_NE(domain.GetDescription(), nullptr);
    EXPECT_LE(strlen(domain.GetDescription()), 80u);
}

// ===========================================================================
// JSON state — AC10
// ===========================================================================

TEST(EntitySpatialVisualDebugger_JSONState, ReportsDrawersAndStats)
{
    Fixture f;
    EntitySpatialVisualDebugger domain(*f.module, f.domain, f.def);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_TRUE(state.isMember("drawers"));
    ASSERT_TRUE(state["drawers"].isArray());
    EXPECT_EQ(state["drawers"].size(), 3u);
    ASSERT_TRUE(state.isMember("stats"));
    EXPECT_TRUE(state["stats"].isObject());
}

TEST(EntitySpatialVisualDebugger_JSONState, DrawerNames_AreGrid_Entities_Query)
{
    Fixture f;
    EntitySpatialVisualDebugger domain(*f.module, f.domain, f.def);

    Json::Value state;
    domain.GetJSONState(state);

    ASSERT_EQ(state["drawers"].size(), 3u);
    EXPECT_STREQ(state["drawers"][0u]["name"].asCString(), "Grid");
    EXPECT_STREQ(state["drawers"][1u]["name"].asCString(), "Entities");
    EXPECT_STREQ(state["drawers"][2u]["name"].asCString(), "Query");
}

TEST(EntitySpatialVisualDebugger_JSONState, AllDrawers_EnabledByDefault)
{
    Fixture f;
    EntitySpatialVisualDebugger domain(*f.module, f.domain, f.def);

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
    EXPECT_TRUE(state["drawers"][1u]["enabled"].asBool());
    EXPECT_TRUE(state["drawers"][2u]["enabled"].asBool());
}

TEST(EntitySpatialVisualDebugger_JSONState, Stats_HasLabelSize)
{
    Fixture f;
    EntitySpatialVisualDebugger domain(*f.module, f.domain, f.def);

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_TRUE(state["stats"].isMember("labelSize"));
}

// ===========================================================================
// OnCommand toggle — AC11
// ===========================================================================

TEST(EntitySpatialVisualDebugger_OnCommand, Toggle_Grid_DisablesIt)
{
    Fixture f;
    EntitySpatialVisualDebugger domain(*f.module, f.domain, f.def);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Grid"));

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_FALSE(state["drawers"][0u]["enabled"].asBool());
    EXPECT_TRUE (state["drawers"][1u]["enabled"].asBool());
    EXPECT_TRUE (state["drawers"][2u]["enabled"].asBool());
}

TEST(EntitySpatialVisualDebugger_OnCommand, Toggle_Entities_DisablesIt)
{
    Fixture f;
    EntitySpatialVisualDebugger domain(*f.module, f.domain, f.def);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Entities"));

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_TRUE (state["drawers"][0u]["enabled"].asBool());
    EXPECT_FALSE(state["drawers"][1u]["enabled"].asBool());
    EXPECT_TRUE (state["drawers"][2u]["enabled"].asBool());
}

TEST(EntitySpatialVisualDebugger_OnCommand, Toggle_Query_Twice_Restores)
{
    Fixture f;
    EntitySpatialVisualDebugger domain(*f.module, f.domain, f.def);

    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Query"));
    domain.OnCommand(Dia::Core::StringCRC("toggle"), ToggleArgs("Query"));

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_TRUE(state["drawers"][2u]["enabled"].asBool());
}

// ===========================================================================
// OnCommand setScale — AC12
// ===========================================================================

TEST(EntitySpatialVisualDebugger_OnCommand, SetScale_LabelSize_UpdatesStats)
{
    Fixture f;
    EntitySpatialVisualDebugger domain(*f.module, f.domain, f.def);

    domain.OnCommand(Dia::Core::StringCRC("setScale"), ScaleArgs("labelSize", 20.0f));

    Json::Value state;
    domain.GetJSONState(state);

    EXPECT_FLOAT_EQ(state["stats"]["labelSize"].asFloat(), 20.0f);
}

TEST(EntitySpatialVisualDebugger_OnCommand, SetScale_UnknownKey_NoOp)
{
    Fixture f;
    EntitySpatialVisualDebugger domain(*f.module, f.domain, f.def);

    Json::Value args;
    args["key"]   = "notAKey";
    args["value"] = 5.0;
    EXPECT_NO_FATAL_FAILURE(
        domain.OnCommand(Dia::Core::StringCRC("setScale"), args));
}

TEST(EntitySpatialVisualDebugger_OnCommand, UnknownCommand_NoOp)
{
    Fixture f;
    EntitySpatialVisualDebugger domain(*f.module, f.domain, f.def);

    Json::Value empty(Json::objectValue);
    EXPECT_NO_FATAL_FAILURE(
        domain.OnCommand(Dia::Core::StringCRC("notACommand"), empty));
}

TEST(EntitySpatialVisualDebugger_OnCommand, MalformedToggle_NoOp)
{
    Fixture f;
    EntitySpatialVisualDebugger domain(*f.module, f.domain, f.def);

    Json::Value empty(Json::objectValue);
    domain.OnCommand(Dia::Core::StringCRC("toggle"), empty);

    Json::Value wrongType(Json::objectValue);
    wrongType["drawer"] = 42;
    domain.OnCommand(Dia::Core::StringCRC("toggle"), wrongType);

    Json::Value state;
    domain.GetJSONState(state);
    EXPECT_TRUE(state["drawers"][0u]["enabled"].asBool());
}

#endif // DIA_DEBUG
