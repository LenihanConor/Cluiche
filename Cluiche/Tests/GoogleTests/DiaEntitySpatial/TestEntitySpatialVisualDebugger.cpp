// Suite: EntitySpatialVisualDebugger

#include <gtest/gtest.h>
#include <DiaEntitySpatial/Adaptors/EntitySpatialOverlay.h>
#include <DiaEntitySpatial/EntitySpatialModule.h>
#include <DiaEntitySpatial/EntitySpatialIndex.h>
#include <DiaEntitySpatial/SpatialComponent.h>
#include <DiaEntitySpatial/Testing/SpatialTestHelpers.h>
#include <DiaEntity/Domain.h>
#include <DiaEntity/Entity.h>
#include <DiaEntity/ComponentPool.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaCore/DebugDraw/IDebugDraw.h>
#include <DiaCore/Colour/RGBA.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaMaths/Vector/Vector3D.h>
#include <cstdint>
#include <memory>

using namespace Dia::EntitySpatial::Adaptors;
using namespace Dia::EntitySpatial::Testing;
using namespace Dia::EntitySpatial;
using namespace Dia::Entity;
using namespace Dia::Maths;

// ---------------------------------------------------------------------------
// Mock IDebugDraw — counts calls per draw type and captures last circle fill
// ---------------------------------------------------------------------------

struct MockDebugDraw : Dia::Core::IDebugDraw
{
    int             circleCount        = 0;
    int             lineCount          = 0;
    int             rectCount          = 0;
    int             arcCount           = 0;
    int             rayCount           = 0;
    int             textCount          = 0;
    Dia::Core::RGBA lastCircleOutline  = Dia::Core::RGBA(0, 0, 0, 0);
    mutable Dia::Maths::Vector2D mousePixel{ 0.0f, 0.0f };

    // circle (outline + fill) — overlay always calls the 3-arg overload which
    // forwards to this with fill = RGBA(0,0,0,0). Capture outline to detect hit colour.
    void RequestDraw(const Dia::Maths::Vector2D&, float,
                     Dia::Core::RGBA outline, Dia::Core::RGBA) override
    {
        ++circleCount;
        lastCircleOutline = outline;
    }

    // line (start, end, colour)
    void RequestDraw(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&,
                     Dia::Core::RGBA) override
    {
        ++lineCount;
    }

    void RequestDrawPoint(const Dia::Maths::Vector2D&, Dia::Core::RGBA) override {}

    void RequestDrawRect(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&,
                         Dia::Core::RGBA, Dia::Core::RGBA) override
    {
        ++rectCount;
    }

    void RequestDrawArc(const Dia::Maths::Vector2D&, float, float, float,
                        Dia::Core::RGBA) override
    {
        ++arcCount;
    }

    void RequestDrawRay(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&,
                        float, Dia::Core::RGBA) override
    {
        ++rayCount;
    }

    void RequestDraw(const Dia::Maths::Vector2D&, const Dia::Maths::Vector2D&,
                     const Dia::Maths::Vector2D&,
                     Dia::Core::RGBA, Dia::Core::RGBA) override {}

    void RequestDrawText(const Dia::Maths::Vector2D&, const char*,
                         float, Dia::Core::RGBA) override
    {
        ++textCount;
    }

    void RequestDrawLine3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&,
                           Dia::Core::RGBA) override {}
    void RequestDrawRay3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&,
                          float, Dia::Core::RGBA) override {}
    void RequestDrawBox3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&,
                          Dia::Core::RGBA) override {}
    void RequestDrawSphere3D(const Dia::Maths::Vector3D&, float,
                             Dia::Core::RGBA) override {}
    void RequestDrawArrow3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&,
                            float, float, Dia::Core::RGBA) override {}

    uint32_t DroppedCount() const override { return 0; }
    const Dia::Maths::Vector2D& GetMousePixel() const override { return mousePixel; }
};

// ---------------------------------------------------------------------------
// Layer name constants for tests (literal StringCRC, no DebugLayerNames.h)
// ---------------------------------------------------------------------------

static Dia::Core::StringCRC kGridLayer     { "entityspatial.grid"     };
static Dia::Core::StringCRC kEntitiesLayer { "entityspatial.entities" };
static Dia::Core::StringCRC kQueryLayer    { "entityspatial.query"    };

// ---------------------------------------------------------------------------
// Helper: build a 200x200 square grid with 10-unit cells (20x20 = 400 cells)
// ---------------------------------------------------------------------------

static EntitySpatialIndex::SquareDef MakeSquareDef()
{
    EntitySpatialIndex::SquareDef def;
    def.worldBounds = Dia::Geometry2D::AARect(
        Vector2D(-100.f, -100.f),
        Vector2D( 100.f,  100.f));
    def.cellSize = 10.f;
    return def;
}

// ---------------------------------------------------------------------------
// Helper: build a domain with SpatialComponent pool registered
// ---------------------------------------------------------------------------

static void RegisterSpatialPool(Dia::Entity::Domain& domain)
{
    domain.RegisterPool(
        new Dia::Entity::ComponentPool<SpatialComponent>(SpatialComponent::kTypeId));
}

// ===========================================================================
// EntitySpatialGridOverlay tests
// ===========================================================================

TEST(EntitySpatialVisualDebugger, EntitySpatialGridOverlay_Construct_DoesNotCrash)
{
    auto def = MakeSquareDef();
    EXPECT_NO_FATAL_FAILURE(EntitySpatialGridOverlay overlay(def, kGridLayer));
}

TEST(EntitySpatialVisualDebugger, EntitySpatialGridOverlay_SquareGrid_DrawsCorrectCellCount)
{
    // 200x200 world / 10-unit cells = 20 cols * 20 rows = 400 cells
    auto def = MakeSquareDef();
    EntitySpatialGridOverlay overlay(def, kGridLayer);

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_EQ(mock.rectCount, 400);
}

TEST(EntitySpatialVisualDebugger, EntitySpatialGridOverlay_HexGrid_DrawsCorrectLineCount)
{
    // 3 cols * 2 rows = 6 hex cells; each cell draws 6 edges = 36 lines
    EntitySpatialIndex::HexDef hexDef;
    hexDef.colCount  = 3;
    hexDef.rowCount  = 2;
    hexDef.hexRadius = 10.f;
    hexDef.origin    = Vector2D(0.f, 0.f);

    EntitySpatialGridOverlay overlay(hexDef, kGridLayer);

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_EQ(mock.lineCount, 36); // 6 cells * 6 edges each
}

TEST(EntitySpatialVisualDebugger, EntitySpatialGridOverlay_WhenDisabled_DrawsNothing)
{
    auto def = MakeSquareDef();
    EntitySpatialGridOverlay overlay(def, kGridLayer);
    overlay.SetEnabled(false);

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_EQ(mock.rectCount, 0);
    EXPECT_EQ(mock.lineCount, 0);
}

// ===========================================================================
// EntitySpatialEntityOverlay tests
// ===========================================================================

TEST(EntitySpatialVisualDebugger, EntitySpatialEntityOverlay_Construct_DoesNotCrash)
{
    Dia::Entity::Domain domain;
    RegisterSpatialPool(domain);
    auto def    = MakeSquareDef();
    auto module = std::make_unique<EntitySpatialModule>(domain, def);

    EXPECT_NO_FATAL_FAILURE(
        EntitySpatialEntityOverlay overlay(*module, domain, kEntitiesLayer));
}

TEST(EntitySpatialVisualDebugger, EntitySpatialEntityOverlay_Draw_CirclePerSpatialEntity)
{
    Dia::Entity::Domain domain;
    RegisterSpatialPool(domain);
    auto def    = MakeSquareDef();
    auto module = std::make_unique<EntitySpatialModule>(domain, def);

    // Spawn 3 entities with SpatialComponent
    SpawnSpatialEntity(domain, Vector2D( 0.f,  0.f), 1.0f);
    SpawnSpatialEntity(domain, Vector2D(10.f, 10.f), 2.0f);
    SpawnSpatialEntity(domain, Vector2D(20.f, 20.f), 3.0f);
    FlushDomain(domain);
    module->Update();

    EntitySpatialEntityOverlay overlay(*module, domain, kEntitiesLayer);

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_EQ(mock.circleCount, 3);
}

TEST(EntitySpatialVisualDebugger, EntitySpatialEntityOverlay_Draw_WhenDisabled_DrawsNothing)
{
    Dia::Entity::Domain domain;
    RegisterSpatialPool(domain);
    auto def    = MakeSquareDef();
    auto module = std::make_unique<EntitySpatialModule>(domain, def);

    SpawnSpatialEntity(domain, Vector2D(5.f, 5.f), 1.0f);
    FlushDomain(domain);
    module->Update();

    EntitySpatialEntityOverlay overlay(*module, domain, kEntitiesLayer);
    overlay.SetEnabled(false);

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_EQ(mock.circleCount, 0);
    EXPECT_EQ(mock.textCount, 0);
}

TEST(EntitySpatialVisualDebugger, EntitySpatialEntityOverlay_Draw_ShowLabels_DrawsText)
{
    Dia::Entity::Domain domain;
    RegisterSpatialPool(domain);
    auto def    = MakeSquareDef();
    auto module = std::make_unique<EntitySpatialModule>(domain, def);

    SpawnSpatialEntity(domain, Vector2D( 5.f, 5.f), 1.0f);
    SpawnSpatialEntity(domain, Vector2D(15.f, 5.f), 1.0f);
    FlushDomain(domain);
    module->Update();

    EntityOverlayConfig config;
    config.showLabels = true;

    EntitySpatialEntityOverlay overlay(*module, domain, kEntitiesLayer, config);

    MockDebugDraw mock;
    overlay.Draw(mock);

    // 2 entities → 2 circles + 2 text labels
    EXPECT_EQ(mock.circleCount, 2);
    EXPECT_EQ(mock.textCount,   2);
}

TEST(EntitySpatialVisualDebugger, EntitySpatialEntityOverlay_HitHighlight_UsesHitColour)
{
    // Single entity so lastCircleOutline is unambiguously the hit entity's colour.
    auto def = MakeSquareDef();

    Dia::Entity::Domain domain;
    RegisterSpatialPool(domain);
    auto module = std::make_unique<EntitySpatialModule>(domain, def);

    Entity hitEntity = SpawnSpatialEntity(domain, Vector2D(5.f, 5.f), 1.0f, 0x01u);
    FlushDomain(domain);
    module->Update();

    EntitySpatialEntityOverlay overlay(*module, domain, kEntitiesLayer);

    Dia::Core::Containers::DynamicArrayC<Entity, 64> hits;
    hits.Add(hitEntity);
    overlay.SetQueryHits(hits);

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_EQ(mock.circleCount, 1);
    // Hit colour is RGBA(255, 255, 0, 220) — yellow
    EXPECT_EQ(mock.lastCircleOutline.R(), 255u);
    EXPECT_EQ(mock.lastCircleOutline.G(), 255u);
    EXPECT_EQ(mock.lastCircleOutline.B(),   0u);
    EXPECT_EQ(mock.lastCircleOutline.A(), 220u);
}

// ===========================================================================
// EntitySpatialQueryOverlay tests
// ===========================================================================

TEST(EntitySpatialVisualDebugger, EntitySpatialQueryOverlay_Circle_DrawsOneCircle)
{
    EntitySpatialQueryOverlay overlay(kQueryLayer);

    QueryDescriptor desc;
    desc.shape  = QueryDescriptor::Shape::Circle;
    desc.origin = Vector2D(0.f, 0.f);
    desc.radius = 15.f;
    overlay.PushQuery(desc);

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_EQ(mock.circleCount, 1);
    EXPECT_EQ(mock.rectCount,   0);
    EXPECT_EQ(mock.rayCount,    0);
    EXPECT_EQ(mock.arcCount,    0);
    EXPECT_EQ(mock.lineCount,   0);
}

TEST(EntitySpatialVisualDebugger, EntitySpatialQueryOverlay_Region_DrawsOneRect)
{
    EntitySpatialQueryOverlay overlay(kQueryLayer);

    QueryDescriptor desc;
    desc.shape = QueryDescriptor::Shape::Region;
    desc.rect  = Dia::Geometry2D::AARect(Vector2D(-5.f, -5.f), Vector2D(5.f, 5.f));
    overlay.PushQuery(desc);

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_EQ(mock.rectCount,   1);
    EXPECT_EQ(mock.circleCount, 0);
    EXPECT_EQ(mock.rayCount,    0);
}

TEST(EntitySpatialVisualDebugger, EntitySpatialQueryOverlay_Ray_DrawsOneRay)
{
    EntitySpatialQueryOverlay overlay(kQueryLayer);

    QueryDescriptor desc;
    desc.shape   = QueryDescriptor::Shape::Ray;
    desc.origin  = Vector2D(0.f, 0.f);
    desc.dir     = Vector2D(1.f, 0.f);
    desc.maxDist = 50.f;
    overlay.PushQuery(desc);

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_EQ(mock.rayCount,    1);
    EXPECT_EQ(mock.circleCount, 0);
    EXPECT_EQ(mock.rectCount,   0);
}

TEST(EntitySpatialVisualDebugger, EntitySpatialQueryOverlay_Sector_DrawsArcAndTwoLines)
{
    EntitySpatialQueryOverlay overlay(kQueryLayer);

    QueryDescriptor desc;
    desc.shape     = QueryDescriptor::Shape::Sector;
    desc.origin    = Vector2D(0.f, 0.f);
    desc.dir       = Vector2D(1.f, 0.f);
    desc.radius    = 20.f;
    desc.halfAngle = 0.5f; // ~28 degrees
    overlay.PushQuery(desc);

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_EQ(mock.arcCount,  1);
    EXPECT_EQ(mock.lineCount, 2); // two radius lines
    EXPECT_EQ(mock.circleCount, 0);
    EXPECT_EQ(mock.rectCount,   0);
    EXPECT_EQ(mock.rayCount,    0);
}

TEST(EntitySpatialVisualDebugger, EntitySpatialQueryOverlay_KNearest_DrawsOneCircle)
{
    EntitySpatialQueryOverlay overlay(kQueryLayer);

    QueryDescriptor desc;
    desc.shape  = QueryDescriptor::Shape::KNearest;
    desc.origin = Vector2D(10.f, 10.f);
    desc.k      = 5;
    overlay.PushQuery(desc);

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_EQ(mock.circleCount, 1);
    EXPECT_EQ(mock.rectCount,   0);
    EXPECT_EQ(mock.rayCount,    0);
    EXPECT_EQ(mock.arcCount,    0);
    EXPECT_EQ(mock.lineCount,   0);
}

TEST(EntitySpatialVisualDebugger, EntitySpatialQueryOverlay_WhenDisabled_DrawsNothing)
{
    EntitySpatialQueryOverlay overlay(kQueryLayer);

    QueryDescriptor desc;
    desc.shape  = QueryDescriptor::Shape::Circle;
    desc.origin = Vector2D(0.f, 0.f);
    desc.radius = 10.f;
    overlay.PushQuery(desc);
    overlay.SetEnabled(false);

    MockDebugDraw mock;
    overlay.Draw(mock);

    EXPECT_EQ(mock.circleCount, 0);
    EXPECT_EQ(mock.rectCount,   0);
    EXPECT_EQ(mock.rayCount,    0);
    EXPECT_EQ(mock.arcCount,    0);
    EXPECT_EQ(mock.lineCount,   0);
}
