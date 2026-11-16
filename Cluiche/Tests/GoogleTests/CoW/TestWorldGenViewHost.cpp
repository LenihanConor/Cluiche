// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/worldgen-view.plan.md (task 7)
// WorldGenViewHost over fakes: in-process CellLayerRegistry + CellFieldRegistry, FakeMirrorRegistrar, FakeDebugDraw, port lambdas.
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include <Modules/WorldGenerationModule.h>   // before windows.h users (RandomStream min/max)
#include "Modules/WorldGenViewHost.h"
#include "Modules/WorldGenDebugDomain.h"

#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaValueSchema/ValueSchemaLoader.h>
#include <Shared/WorldGeneration/Testing/TestGuarantees.h>
#include <Shared/WorldGeneration/Testing/TestPlacers.h>
#include <Shared/WorldGeneration/SiteGuarantees.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>

using namespace CoW;
using namespace CoW::WorldGeneration;
using Dia::CellLayers::Testing::FakeDebugDraw;
using Dia::CellLayers::Testing::FakeMirrorRegistrar;
using Dia::Maths::Vector2D;

namespace
{
Json::Value Obj(const char* key, const Json::Value& v)
{
    Json::Value o(Json::objectValue);
    o[key] = v;
    return o;
}

// Counts text requests as well as lines.
struct RecordingDraw : public FakeDebugDraw
{
    uint32_t texts = 0;
    void RequestDrawText(const Vector2D&, const char*, float, Dia::Core::RGBA) override { ++texts; }
};

Dia::SimTime::SimTimeContext Ctx()
{
    return Dia::SimTime::SimTimeContext{ Dia::Core::TimeAbsolute::Zero(), Dia::Core::TimeRelative::CreateFromSeconds(1.0f / 60.0f), 0, 1.0f, false };
}

struct Rig
{
    WorldGenSession session;
    Dia::Maths::RandomStream rng{ 12345 };
    Dia::Graphics::CellFieldRegistry cellField;
    Dia::CellLayers::CellLayerRegistry registry{ nullptr, &Dia::CellLayers::Testing::FakePuIdFn };
    FakeMirrorRegistrar registrar;
    Vector2D window{ 1280.0f, 720.0f };
    float camX = -1, camY = -1, camZoom = -1;
    uint32_t setCameraCalls = 0;
    WorldGenViewHost::InputSnapshot input;
    bool haveInput = false;
    std::unique_ptr<WorldGenViewHost> host;

    Rig(const Dia::ValueSchema::ValueSchemaService* schema = nullptr, bool generate = true, SharedWorldHexGrid* shared = nullptr)
    {
        Dia::CellLayers::Testing::FakePuId() = 1;
        if (generate)
            Tick(12345);
        WorldGenViewHost::Deps d;
        d.registry = &registry;
        d.cellField = &cellField;
        d.schema = schema;
        d.sharedGrid = shared;
        d.registrar = &registrar;
        d.windowSize = [this]() { return window; };
        d.getCamera = [this](float& x, float& y, float& z) { x = camX; y = camY; z = camZoom; };
        d.setCamera = [this](float x, float y, float z) { camX = x; camY = y; camZoom = z; ++setCameraCalls; };
        d.readInput = [this](WorldGenViewHost::InputSnapshot& s) { s = input; return haveInput; };
        host = std::make_unique<WorldGenViewHost>(session, d);
    }

    void Tick(uint64_t seed)
    {
        session.Regenerate(Obj("seed", Json::Value(static_cast<Json::UInt64>(seed))));
        ASSERT_TRUE(session.Tick(rng));
    }

    void Resize(const char* size)
    {
        session.SetSize(Obj("size", size));
        ASSERT_TRUE(session.Tick(rng));
    }

    // One frame: Update then Draw, like the stage module and the layer manager.
    void Frame(FakeDebugDraw& draw)
    {
        host->Update(Ctx());
        host->Draw(draw);
    }
};

int IndexOf(const Rig& r, int x, int y) { return y * r.host->Grid().Dimensions().width + x; }
}

TEST(WorldGenViewHost, BuildRegistersEveryDescriptorAndFourOverlaysAgainstOneGrid)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    EXPECT_EQ(r.registry.LayerCount(), WorldLayerSource::Count());
    EXPECT_EQ(r.registry.OverlayCount(), 6u);   // rivers, selection.neighbours, regions.edges, sites, dynasty.starts, guarantee.evidence
    ASSERT_EQ(r.registry.GridCount(), 1u);
    EXPECT_EQ(r.registry.GridAt(0), &r.host->Grid().Desc());
    for (uint32_t i = 0; i < WorldLayerSource::Count(); ++i)
    {
        const Dia::CellLayers::CellLayerDesc* d = r.registry.Find(Dia::Core::StringCRC(WorldLayerSource::At(i).id));
        ASSERT_NE(d, nullptr) << WorldLayerSource::At(i).id;
        EXPECT_EQ(d->grid, &r.host->Grid().Desc());
        EXPECT_EQ(d->schemaKey, Dia::Core::StringCRC(WorldLayerSource::At(i).schemaKey));
    }
    EXPECT_NE(r.registry.FindOverlay(Dia::Core::StringCRC("rivers")), nullptr);
    EXPECT_NE(r.registry.FindOverlay(Dia::Core::StringCRC("selection.neighbours")), nullptr);
    EXPECT_NE(r.registry.FindOverlay(Dia::Core::StringCRC("regions.edges")), nullptr);
    EXPECT_NE(r.registry.FindOverlay(Dia::Core::StringCRC("sites")), nullptr);
    EXPECT_TRUE(r.registrar.Has("cells.select.cell"));
    EXPECT_TRUE(r.registrar.Has("cells.layer.base_by_id"));
}

TEST(WorldGenViewHost, UploadsExactlyOncePerRevisionAndNeverOnIdleFrames)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    FakeDebugDraw draw;
    r.Frame(draw);
    EXPECT_EQ(r.host->UploadCount(), 1u);
    EXPECT_EQ(r.host->LayerVersion(), r.session.Revision());
    for (int i = 0; i < 300; ++i)
        r.Frame(draw);
    EXPECT_EQ(r.host->UploadCount(), 1u) << "idle frames must not upload";
    EXPECT_GE(draw.cellFieldRequests, 300u);   // but the map is still drawn every frame

    r.Tick(777);
    const uint32_t before = r.session.Revision();
    r.Frame(draw);
    EXPECT_EQ(r.host->UploadCount(), 2u);
    EXPECT_EQ(r.host->LayerVersion(), before);
    for (int i = 0; i < 300; ++i)
        r.Frame(draw);
    EXPECT_EQ(r.host->UploadCount(), 2u);
    EXPECT_EQ(r.host->ReadFailures(), 0u);
}

TEST(WorldGenViewHost, SelectCellSetsViewerSelectionAndGetSelectionReturnsIt)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    FakeDebugDraw draw;
    r.Frame(draw);
    int x = -1, y = -1;
    EXPECT_FALSE(r.host->HasSelection());
    EXPECT_FALSE(r.host->GetSelection(x, y));

    EXPECT_TRUE(r.host->SelectCell(7, 9));
    EXPECT_TRUE(r.host->HasSelection());
    ASSERT_TRUE(r.host->GetSelection(x, y));
    EXPECT_EQ(x, 7);
    EXPECT_EQ(y, 9);
    // Same object the viewer reads.
    const Dia::CellLayers::ICellSelectionView* view = r.host->Viewer()->ActiveSelectionView();
    ASSERT_NE(view, nullptr);
    EXPECT_EQ(view->Count(), 1u);
    EXPECT_TRUE(view->Contains(static_cast<uint32_t>(IndexOf(r, 7, 9))));

    EXPECT_TRUE(r.host->SelectCell(1, 2));   // replaces
    ASSERT_TRUE(r.host->GetSelection(x, y));
    EXPECT_EQ(x, 1);
    EXPECT_EQ(y, 2);

    EXPECT_FALSE(r.host->SelectCell(-1, 0));
    EXPECT_FALSE(r.host->SelectCell(0, 42));
    EXPECT_FALSE(r.host->SelectCell(66, 0));
    ASSERT_TRUE(r.host->GetSelection(x, y));   // a rejected select leaves the selection alone
    EXPECT_EQ(x, 1);

    // The sim-hosted command selects through the same store.
    Json::Value args(Json::objectValue);
    args["q"] = 4;
    args["r"] = 5;
    r.registrar.Execute("cells.select.cell", args);
    ASSERT_TRUE(r.host->GetSelection(x, y));
    EXPECT_EQ(x, 4);
    EXPECT_EQ(y, 5);

    r.host->ClearSelection();
    EXPECT_FALSE(r.host->HasSelection());
}

TEST(WorldGenViewHost, SetColourModeSwitchesActiveBaseAndRejectsUnknownIds)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    FakeDebugDraw draw;
    r.Frame(draw);
    EXPECT_STREQ(r.host->GetColourMode(), "relief");

    EXPECT_TRUE(r.host->SetColourMode("elevation"));
    r.Frame(draw);
    EXPECT_STREQ(r.host->GetColourMode(), "elevation");
    EXPECT_EQ(r.host->Viewer()->ActiveBase(), Dia::Core::StringCRC("elevation"));

    EXPECT_FALSE(r.host->SetColourMode("nonsense"));
    EXPECT_FALSE(r.host->SetColourMode(nullptr));
    EXPECT_FALSE(r.host->SetColourMode(""));
    EXPECT_STREQ(r.host->GetColourMode(), "elevation");

    EXPECT_TRUE(r.host->SetColourMode("terrain"));   // alias of the default base
    EXPECT_STREQ(r.host->GetColourMode(), "relief");

    EXPECT_TRUE(r.host->SetColourMode("terrainType"));
    r.Frame(draw);
    EXPECT_STREQ(r.host->GetColourMode(), "terrainType");

    uint32_t modes = 0;
    r.host->ListColourModes([&](const char*) { ++modes; });
    EXPECT_EQ(modes, WorldLayerSource::Count());
}

// Consumer Amendments ACs 13-15: the host registers against the shared grid it is given, drives its rebuilds, and a second
// registrant on the same grid yields one grid in the registry.
TEST(WorldGenViewHost, UsesTheSharedGridRebuildsItInPlaceAndASecondRegistrantSharesIt)
{
    SharedWorldHexGrid shared;
    FakeDebugDraw draw;
    {
        Rig r(nullptr, true, &shared);
        ASSERT_TRUE(r.host->Build());
        ASSERT_TRUE(static_cast<bool>(shared));
        EXPECT_EQ(&r.host->Grid(), shared.Get()) << "the host must use the shared instance, not make its own";
        EXPECT_TRUE(shared.IsDriven());
        const Dia::Graphics::CellGridDesc* desc = &shared->Desc();

        // A second, independent registrant given the same grid.
        ASSERT_EQ(r.registry.Register(Dia::CellLayers::Testing::MakeLayer("second.registrant", &shared->Desc())), Dia::CellLayers::CellRegisterResult::Ok);
        EXPECT_EQ(r.registry.GridCount(), 1u);
        EXPECT_EQ(r.registry.GridAt(0), desc);

        const uint32_t gen = shared.Generation();
        r.Resize("Large");
        r.Frame(draw);
        EXPECT_GT(shared.Generation(), gen);
        EXPECT_EQ(shared->Dimensions().width, 104);
        EXPECT_EQ(&shared->Desc(), desc) << "rebuilt in place: registrants keep one desc address";
        for (uint32_t i = 0; i < WorldLayerSource::Count(); ++i)
            EXPECT_EQ(r.registry.Find(Dia::Core::StringCRC(WorldLayerSource::At(i).id))->grid, desc);
        EXPECT_EQ(r.registry.GridCount(), 1u);
    }
    EXPECT_FALSE(shared.IsDriven()) << "the owner takes the grid back when the host goes away";
}

TEST(WorldGenViewHost, SizeChangeKeepsInRangeSelectionOrClearsAndLeavesNoDanglingGrid)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    FakeDebugDraw draw;
    r.Frame(draw);
    ASSERT_TRUE(r.host->SetColourMode("terrainType"));
    r.Frame(draw);
    const uint32_t smallCells = r.host->Grid().CellCount();

    auto expectOneGrid = [&](const char* when)
    {
        SCOPED_TRACE(when);
        ASSERT_EQ(r.registry.GridCount(), 1u);
        EXPECT_EQ(r.registry.GridAt(0), &r.host->Grid().Desc());
        EXPECT_EQ(r.registry.LayerCount(), WorldLayerSource::Count());
        for (uint32_t i = 0; i < r.registry.LayerCount(); ++i)
        {
            const Dia::CellLayers::CellLayerDesc* d = r.registry.LayerAt(i);
            ASSERT_NE(d, nullptr);
            EXPECT_EQ(d->grid, &r.host->Grid().Desc());
            EXPECT_EQ(d->grid->width, r.host->Grid().Dimensions().width);
            EXPECT_EQ(d->grid->height, r.host->Grid().Dimensions().height);
            EXPECT_EQ(d->grid->centresXY, r.host->Grid().Centres().data());
            EXPECT_EQ(d->grid->neighbours, r.host->Grid().Neighbours().data());
        }
    };
    expectOneGrid("small");

    // Small -> Large with a selection that is still in range.
    ASSERT_TRUE(r.host->SelectCell(10, 11));
    r.Resize("Large");
    r.Frame(draw);
    expectOneGrid("large");
    EXPECT_EQ(r.host->Grid().Dimensions().width, 104);
    EXPECT_GT(r.host->Grid().CellCount(), smallCells);
    int x = -1, y = -1;
    ASSERT_TRUE(r.host->GetSelection(x, y));
    EXPECT_EQ(x, 10);
    EXPECT_EQ(y, 11);
    EXPECT_STREQ(r.host->GetColourMode(), "terrainType") << "colour mode survives a rebuild";
    EXPECT_EQ(r.host->ReadFailures(), 0u);
    EXPECT_EQ(r.host->LayerVersion(), r.session.Revision()) << "new grid uploaded";

    // Select a cell that exists only at Large, then Large -> Small: cleared.
    ASSERT_TRUE(r.host->SelectCell(100, 60));
    r.Resize("Small");
    r.Frame(draw);
    expectOneGrid("small again");
    EXPECT_EQ(r.host->Grid().Dimensions().width, 66);
    EXPECT_FALSE(r.host->HasSelection());
    EXPECT_EQ(r.host->ReadFailures(), 0u);
    EXPECT_EQ(r.host->LayerVersion(), r.session.Revision());

    // The map still draws and selecting works on the rebuilt grid.
    const uint32_t requests = draw.cellFieldRequests;
    r.Frame(draw);
    EXPECT_EQ(draw.cellFieldRequests, requests + 1);
    EXPECT_TRUE(r.host->SelectCell(65, 41));
    EXPECT_FALSE(r.host->SelectCell(66, 41));

    // Many round trips must not exhaust the registry's fixed selection store.
    for (int i = 0; i < 12; ++i)
    {
        r.Resize(i % 2 == 0 ? "Standard" : "Small");
        r.Frame(draw);
    }
    EXPECT_TRUE(r.host->SelectCell(3, 3));
    EXPECT_EQ(r.registry.RejectedRegistrations(), 0u);
    expectOneGrid("after churn");
}

TEST(WorldGenViewHost, PendingSizeIsIgnoredUntilTheWorldIsGenerated)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    FakeDebugDraw draw;
    r.Frame(draw);
    r.session.SetSize(Obj("size", "Large"));   // pending: no Tick yet
    r.Frame(draw);
    EXPECT_EQ(r.host->Grid().Dimensions().width, 66);
    ASSERT_TRUE(r.session.Tick(r.rng));
    r.Frame(draw);
    EXPECT_EQ(r.host->Grid().Dimensions().width, 104);
}

TEST(WorldGenViewHost, RiverOverlayEmitsOneLinePerSegmentAndCachesPerRevision)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    ASSERT_TRUE(r.host->SetOverlayEnabled("rivers", true));
    ASSERT_TRUE(r.host->SetOverlayEnabled("selection.neighbours", false));
    ASSERT_TRUE(r.host->SetOverlayEnabled("sites", false));   // sites default on
    const size_t expected = BuildRiverSegments(r.session.World(), r.host->Grid()).size();
    ASSERT_GT(expected, 0u);

    RecordingDraw draw;
    r.Frame(draw);
    EXPECT_EQ(draw.lines.size(), expected);
    for (int i = 0; i < 20; ++i)
        r.Frame(draw);
    EXPECT_EQ(r.host->RiverSegmentRebuilds(), 1u) << "segments are cached while the revision is unchanged";

    r.Tick(4242);
    r.Frame(draw);
    EXPECT_EQ(r.host->RiverSegmentRebuilds(), 2u);

    ASSERT_TRUE(r.host->SetOverlayEnabled("rivers", false));
    RecordingDraw off;
    r.Frame(off);
    EXPECT_EQ(off.lines.size(), 0u);
}

TEST(WorldGenViewHost, NeighbourOverlayDrawsSixLabelsForAnInteriorCellAndOnlyInBoundsAtACorner)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    ASSERT_TRUE(r.host->SetOverlayEnabled("rivers", false));
    ASSERT_TRUE(r.host->SetOverlayEnabled("selection.neighbours", true));

    FakeDebugDraw warm;
    r.Frame(warm);   // the viewer clears the selection when it first activates the grid

    // Baseline: selection outline only (no overlay lines) so the overlay's own lines can be isolated.
    ASSERT_TRUE(r.host->SelectCell(20, 20));
    RecordingDraw none;
    ASSERT_TRUE(r.host->SetOverlayEnabled("selection.neighbours", false));
    r.Frame(none);
    ASSERT_TRUE(r.host->SetOverlayEnabled("selection.neighbours", true));

    RecordingDraw interior;
    r.Frame(interior);
    EXPECT_EQ(interior.texts, 6u);
    EXPECT_EQ(interior.lines.size() - none.lines.size(), 36u) << "6 edges per in-bounds neighbour";

    ASSERT_TRUE(r.host->SelectCell(0, 0));
    const NeighbourOutline expected = BuildNeighbourOutline(r.host->Grid(), 0);
    uint32_t inBounds = 0;
    for (int d = 0; d < 6; ++d)
        inBounds += expected.hasLabel[d] ? 1u : 0u;
    ASSERT_LT(inBounds, 6u);
    RecordingDraw corner;
    r.Frame(corner);
    EXPECT_EQ(corner.texts, inBounds);

    // No selection: nothing drawn by the overlay.
    r.host->ClearSelection();
    RecordingDraw cleared;
    r.Frame(cleared);
    EXPECT_EQ(cleared.texts, 0u);

    // A multi-cell selection has no single centre: no labels.
    Json::Value box(Json::objectValue);
    box["q0"] = 2; box["r0"] = 2; box["q1"] = 4; box["r1"] = 4;
    r.registrar.Execute("cells.select.box", box);
    RecordingDraw many;
    r.Frame(many);
    EXPECT_EQ(many.texts, 0u);
}

TEST(WorldGenViewHost, ListOverlaysReportsEnabledAndAvailable)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    FakeDebugDraw draw;
    r.Frame(draw);
    struct Row { std::string id; bool enabled; bool available; };
    auto list = [&]()
    {
        std::vector<Row> rows;
        r.host->ListOverlays([&](const char* id, bool en, bool av) { rows.push_back({ id, en, av }); });
        return rows;
    };
    std::vector<Row> rows = list();
    ASSERT_EQ(rows.size(), 10u);   // rivers, neighbours, regions.edges, sites, one toggle per built-in kind, dynasty.starts, guarantee.evidence
    EXPECT_EQ(rows[2].id, "regions.edges");
    EXPECT_EQ(rows[3].id, "sites");
    EXPECT_EQ(rows[4].id, "sites.CoW.Node.Corridor");
    EXPECT_EQ(rows[7].id, "sites.CoW.Node.Resource");
    EXPECT_EQ(rows[0].id, "rivers");
    EXPECT_TRUE(rows[0].enabled);
    EXPECT_TRUE(rows[0].available);
    EXPECT_EQ(rows[1].id, "selection.neighbours");
    EXPECT_FALSE(rows[1].available) << "nothing selected";

    r.host->SelectCell(5, 5);
    r.host->SetOverlayEnabled("rivers", false);
    rows = list();
    EXPECT_FALSE(rows[0].enabled);
    EXPECT_TRUE(rows[1].available);
    EXPECT_FALSE(r.host->SetOverlayEnabled("grid", true));
}

TEST(WorldGenViewHost, DetailProviderRowsComeFromTheSessionCell)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    FakeDebugDraw draw;
    r.Frame(draw);
    Dia::CellLayers::CellDetailResult result;
    r.registry.Details().Build(&r.host->Grid().Desc(), static_cast<uint32_t>(IndexOf(r, 3, 4)), nullptr, result);
    ASSERT_GT(result.Count(), 0u);
    bool sawCell = false;
    for (uint32_t i = 0; i < result.Count(); ++i)
    {
        const Dia::CellLayers::CellDetailRow& row = result.At(i);
        if (std::string(row.label.AsCStr()) == "Cell")
        {
            sawCell = true;
            EXPECT_STREQ(row.text.AsCStr(), "3,4");
        }
    }
    EXPECT_TRUE(sawCell);
}

TEST(WorldGenViewHost, TeardownThenDestroyLeavesTheRegistryAndRegistrarEmpty)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    FakeDebugDraw draw;
    r.Frame(draw);
    r.host->SelectCell(2, 2);
    r.host->Teardown();
    EXPECT_EQ(r.registry.LayerCount(), 0u);
    EXPECT_EQ(r.registry.OverlayCount(), 0u);
    EXPECT_EQ(r.registry.GridCount(), 0u);
    EXPECT_EQ(r.registrar.Count(), 0u);
    EXPECT_FALSE(r.host->IsBuilt());
    r.host->Teardown();   // idempotent
    r.host->Update(Ctx());   // inert after teardown
    r.host->Draw(draw);
    EXPECT_FALSE(r.host->SelectCell(1, 1));
    r.host.reset();
    EXPECT_EQ(r.registry.LayerCount(), 0u);
}

TEST(WorldGenViewHost, DestructorAloneUnregistersEverything)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    r.host.reset();
    EXPECT_EQ(r.registry.LayerCount(), 0u);
    EXPECT_EQ(r.registry.OverlayCount(), 0u);
    EXPECT_EQ(r.registrar.Count(), 0u);
}

TEST(WorldGenViewHost, BuildFailsCleanlyWithoutRegistry)
{
    WorldGenSession session;
    WorldGenViewHost::Deps d;
    WorldGenViewHost host(session, d);
    EXPECT_FALSE(host.Build());
    EXPECT_FALSE(host.IsBuilt());
    EXPECT_FALSE(host.SelectCell(0, 0));
    EXPECT_STREQ(host.GetColourMode(), "relief");
}

TEST(WorldGenViewHost, FirstUpdateFitsTheCameraAndHomeRefits)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    FakeDebugDraw draw;
    r.Frame(draw);
    EXPECT_GT(r.camZoom, 0.0f);
    const float fitZoom = r.camZoom;
    const float fitX = r.camX;
    EXPECT_NEAR(r.camX, (r.host->Grid().BoundsMin().x + r.host->Grid().BoundsMax().x) * 0.5f, 1.0f);

    // Zoom in with the wheel, then Home.
    r.haveInput = true;
    r.input.pointer.valid = true;
    r.input.pointer.x = 640;
    r.input.pointer.y = 360;
    r.input.camera.wheelDelta = 3.0f;
    r.Frame(draw);
    EXPECT_GT(r.camZoom, fitZoom);
    r.input.camera.wheelDelta = 0.0f;
    r.input.camera.keyHome = true;
    r.Frame(draw);
    EXPECT_NEAR(r.camZoom, fitZoom, 1e-4f);
    EXPECT_NEAR(r.camX, fitX, 1e-3f);

    // Window resize refits.
    r.input.camera.keyHome = false;
    r.window = Vector2D(640.0f, 360.0f);
    r.Frame(draw);
    EXPECT_LT(r.camZoom, fitZoom);

    // Teardown restores the camera that was there before the host moved it.
    r.camX = 0; r.camY = 0; r.camZoom = 1;
    Rig r2;
    ASSERT_TRUE(r2.host->Build());
    r2.camX = 11.0f; r2.camY = 22.0f; r2.camZoom = 3.0f;
    r2.Frame(draw);
    r2.host->Teardown();
    EXPECT_FLOAT_EQ(r2.camX, 11.0f);
    EXPECT_FLOAT_EQ(r2.camY, 22.0f);
    EXPECT_FLOAT_EQ(r2.camZoom, 3.0f);
}

TEST(WorldGenViewHost, ClickingACellThroughPointerFramesSelectsIt)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    FakeDebugDraw draw;
    r.Frame(draw);

    const WorldHexGrid& g = r.host->Grid();
    const Vector2D w = g.CellCenter(static_cast<uint32_t>(IndexOf(r, 12, 8)));
    const CameraState& cs = r.host->Camera().State();
    const float sx = (w.x - cs.centreX) * cs.zoom + r.window.x * 0.5f;
    const float sy = (w.y - cs.centreY) * cs.zoom + r.window.y * 0.5f;

    r.haveInput = true;
    r.input.pointer.valid = true;
    r.input.pointer.x = sx;
    r.input.pointer.y = sy;
    r.input.pointer.left = false;
    r.Frame(draw);
    r.input.pointer.left = true;
    r.Frame(draw);
    r.input.pointer.left = false;
    r.Frame(draw);
    r.Frame(draw);

    int x = -1, y = -1;
    ASSERT_TRUE(r.host->GetSelection(x, y));
    EXPECT_EQ(x, 12);
    EXPECT_EQ(y, 8);
}

namespace
{
std::string ReadFile(const std::filesystem::path& p)
{
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::filesystem::path FindFromRepoRoot(const char* relative)
{
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::absolute(std::filesystem::path(__FILE__), ec).parent_path();
    while (!dir.empty())
    {
        const std::filesystem::path candidate = dir / relative;
        if (std::filesystem::exists(candidate, ec))
            return candidate;
        const std::filesystem::path parent = dir.parent_path();
        if (parent == dir)
            break;
        dir = parent;
    }
    return {};
}
}

TEST(WorldGenViewHost, LegendComesFromTheRegisteredSchemaOfTheActiveBase)
{
    const auto schemaPath = FindFromRepoRoot("Cluiche/Assets/CoW/Stages/S1WorldGenerationTestbedStage/worldgen.schema.json");
    ASSERT_FALSE(schemaPath.empty());
    const std::string text = ReadFile(schemaPath);
    const Dia::ValueSchema::LoadResult loaded = Dia::ValueSchema::ValueSchemaLoader::LoadFromText(text.c_str(), "worldgen.schema.json");
    ASSERT_TRUE(loaded.mOk);

    Dia::CellLayers::Testing::UploaderSchemaFixture fx;
    std::set<std::string> ramps;
    for (unsigned i = 0; i < loaded.mEntries.Size(); ++i)
    {
        const Dia::ValueSchema::ValueInfo& e = loaded.mEntries[i];
        if (e.mUi.isObject() && e.mUi["ramp"].isString())
            ramps.insert(e.mUi["ramp"].asString());
        fx.Add(e);
    }
    for (const std::string& name : ramps)
        if (name != "sequential.grey")
            fx.ramps.Register(Dia::CellLayers::Testing::UploaderSchemaFixture::MakeRamp(name.c_str(), 0x102030FF, 0xE0F0FFFF), true, "test");
    const Dia::ValueSchema::ValueSchemaService& service = fx.Publish();

    Rig r(&service);
    r.registry.SetSchema(&service);
    ASSERT_TRUE(r.host->Build());
    FakeDebugDraw draw;
    r.Frame(draw);
    EXPECT_STREQ(r.host->GetColourMode(), "relief") << "schema default_base wins";

    Json::Value legend;
    r.host->FillLegend(legend);
    ASSERT_TRUE(legend.isObject()) << "float layer: {min,max,ramp,stops}";
    EXPECT_DOUBLE_EQ(legend["min"].asDouble(), 0.0);
    EXPECT_DOUBLE_EQ(legend["max"].asDouble(), 1.0);
    EXPECT_STREQ(legend["ramp"].asCString(), "sequential.terrain");
    ASSERT_GE(legend["stops"].size(), 2u);
    EXPECT_EQ(legend["stops"][0u]["colour"].asString().size(), 7u);
    EXPECT_EQ(legend["stops"][0u]["colour"].asString()[0], '#');

    ASSERT_TRUE(r.host->SetColourMode("terrainType"));
    r.Frame(draw);
    r.host->FillLegend(legend);
    ASSERT_TRUE(legend.isArray()) << "category layer: [{label,colour}]";
    EXPECT_EQ(legend.size(), static_cast<Json::ArrayIndex>(static_cast<uint32_t>(TerrainType::Count)));
    for (const Json::Value& e : legend)
    {
        EXPECT_FALSE(e["label"].asString().empty());
        EXPECT_EQ(e["colour"].asString().size(), 7u);
    }

    // Both relief and its elevation companion are uploaded; the frame counter still moves once per revision.
    ASSERT_TRUE(r.host->SetColourMode("relief"));
    r.Frame(draw);
    const uint32_t uploads = r.host->UploadCount();
    for (int i = 0; i < 50; ++i)
        r.Frame(draw);
    EXPECT_EQ(r.host->UploadCount(), uploads);
}

TEST(WorldGenViewHost, LegendIsNullWithoutASchema)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    FakeDebugDraw draw;
    r.Frame(draw);
    Json::Value legend(Json::objectValue);
    r.host->FillLegend(legend);
    EXPECT_TRUE(legend.isNull() || legend.isObject());
}

// ---- Gap audit (task 12): edge cases ------------------------------------------------------------------------------

TEST(WorldGenViewHost, NoWorldYetBuildUpdateDrawDoNotCrashAndUploadZeros)
{
    Rig r(nullptr, false);
    ASSERT_EQ(r.session.Revision(), 0u);
    ASSERT_TRUE(r.host->Build());
    RecordingDraw draw;
    for (int i = 0; i < 5; ++i)
        r.Frame(draw);
    EXPECT_EQ(r.host->ReadFailures(), 0u);
    EXPECT_EQ(draw.lines.size(), 0u) << "rivers need a world";
    EXPECT_EQ(draw.texts, 0u);
    EXPECT_FALSE(r.host->HasSelection());
    std::vector<bool> avail;
    r.host->ListOverlays([&](const char*, bool, bool a) { avail.push_back(a); });
    ASSERT_EQ(avail.size(), 10u);   // 6 overlays + 4 built-in site kind toggles
    EXPECT_FALSE(avail[0]) << "rivers unavailable without a world";
    for (size_t i = 2; i < avail.size(); ++i) EXPECT_FALSE(avail[i]) << "region and site overlays need fresh regions";
    Json::Value legend;
    r.host->FillLegend(legend);   // must not crash

    // The first world arriving afterwards is picked up.
    r.Tick(12345);
    r.Frame(draw);
    EXPECT_EQ(r.host->LayerVersion(), r.session.Revision());
    EXPECT_EQ(r.host->ReadFailures(), 0u);
}

TEST(WorldGenViewHost, BuildTwiceTeardownTwiceAndTeardownBeforeBuildAreSafe)
{
    Rig r;
    r.host->Teardown();   // before Build: nothing registered, nothing to undo
    EXPECT_EQ(r.registry.LayerCount(), 0u);
    EXPECT_FALSE(r.host->IsBuilt());

    ASSERT_TRUE(r.host->Build());
    ASSERT_TRUE(r.host->Build());   // second Build is a no-op, no duplicate registrations
    EXPECT_EQ(r.registry.LayerCount(), WorldLayerSource::Count());
    EXPECT_EQ(r.registry.OverlayCount(), 6u);   // rivers, neighbours, regions.edges, sites, dynasty.starts, guarantee.evidence
    EXPECT_EQ(r.registry.GridCount(), 1u);

    r.host->Teardown();
    r.host->Teardown();
    EXPECT_EQ(r.registry.LayerCount(), 0u);

    ASSERT_TRUE(r.host->Build());   // rebuildable after teardown
    EXPECT_EQ(r.registry.LayerCount(), WorldLayerSource::Count());
    FakeDebugDraw draw;
    r.Frame(draw);
    EXPECT_EQ(r.host->LayerVersion(), r.session.Revision());
}

TEST(WorldGenViewHost, UnavailableOverlayToggleAndUnknownOverlayDoNotCrash)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    FakeDebugDraw draw;
    r.Frame(draw);
    EXPECT_TRUE(r.host->SetOverlayEnabled("selection.neighbours", false));   // nothing selected: still just a flag
    EXPECT_TRUE(r.host->SetOverlayEnabled("selection.neighbours", true));
    EXPECT_FALSE(r.host->SetOverlayEnabled("", true));
    EXPECT_FALSE(r.host->SetOverlayEnabled(nullptr, true));
    RecordingDraw d;
    r.Frame(d);
    EXPECT_EQ(d.texts, 0u);
}

TEST(WorldGenViewHost, WindowSizeZeroNeverFitsAndNeverProducesNonFiniteCamera)
{
    Rig r;
    r.window = Vector2D(0.0f, 0.0f);
    ASSERT_TRUE(r.host->Build());
    FakeDebugDraw draw;
    r.haveInput = true;
    r.input.pointer.valid = true;
    r.input.pointer.x = 10; r.input.pointer.y = 10;
    r.input.pointer.left = true;
    for (int i = 0; i < 5; ++i)
        r.Frame(draw);   // pointer frames with a 0x0 viewport go through the picker
    EXPECT_TRUE(std::isfinite(r.host->Camera().State().zoom));
    EXPECT_TRUE(std::isfinite(r.host->Camera().State().centreX));
    EXPECT_TRUE(std::isfinite(r.host->Camera().State().centreY));
    EXPECT_GT(r.host->Camera().State().zoom, 0.0f);

    r.window = Vector2D(1280.0f, 720.0f);   // the window appears: the camera fits now
    r.input.pointer.left = false;
    r.Frame(draw);
    EXPECT_NEAR(r.camX, (r.host->Grid().BoundsMin().x + r.host->Grid().BoundsMax().x) * 0.5f, 1.0f);
}

TEST(WorldGenViewHost, HugeAndNonFiniteWheelAndPointerNeverCorruptTheCamera)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    FakeDebugDraw draw;
    r.Frame(draw);
    r.haveInput = true;
    r.input.pointer.valid = true;
    r.input.pointer.x = 640; r.input.pointer.y = 360;

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    const float wheels[] = { 1e9f, -1e9f, inf, -inf, nan, 3.0f };
    for (float w : wheels)
    {
        r.input.camera.wheelDelta = w;
        r.Frame(draw);
        const CameraState& s = r.host->Camera().State();
        SCOPED_TRACE(w);
        EXPECT_TRUE(std::isfinite(s.zoom));
        EXPECT_TRUE(std::isfinite(s.centreX));
        EXPECT_TRUE(std::isfinite(s.centreY));
        EXPECT_GE(s.zoom, WorldCameraController::kMinZoom);
        EXPECT_LE(s.zoom, WorldCameraController::kMaxZoom);
    }

    r.input.camera.wheelDelta = 0.0f;
    const float coords[][2] = { { nan, nan }, { inf, -inf }, { 1e30f, -1e30f } };
    for (const auto& c : coords)
    {
        r.input.pointer.x = c[0];
        r.input.pointer.y = c[1];
        r.input.pointer.left = true;
        r.input.pointer.middle = true;
        r.Frame(draw);
        r.input.pointer.left = false;
        r.input.pointer.middle = false;
        r.Frame(draw);
        const CameraState& s = r.host->Camera().State();
        EXPECT_TRUE(std::isfinite(s.zoom));
        EXPECT_TRUE(std::isfinite(s.centreX));
        EXPECT_TRUE(std::isfinite(s.centreY));
        EXPECT_FALSE(r.host->HasSelection()) << "a garbage pointer must not select a cell";
    }
}

TEST(WorldGenViewHost, SetSizeWhileASelectionExistsKeepsOrClearsAndNeverExhaustsSelectionStore)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    FakeDebugDraw draw;
    r.Frame(draw);
    for (int i = 0; i < 24; ++i)
    {
        ASSERT_TRUE(r.host->SelectCell(5, 5));
        r.Resize(i % 3 == 0 ? "Large" : (i % 3 == 1 ? "Standard" : "Small"));
        r.Frame(draw);
        int x = -1, y = -1;
        ASSERT_TRUE(r.host->GetSelection(x, y)) << i;   // (5,5) is in range at every size
        EXPECT_EQ(x, 5);
        EXPECT_EQ(y, 5);
    }
    EXPECT_EQ(r.registry.RejectedRegistrations(), 0u);
    r.Resize("Large");
    r.Frame(draw);
    ASSERT_TRUE(r.host->SelectCell(103, 63));
    r.Resize("Small");
    r.Frame(draw);
    EXPECT_FALSE(r.host->HasSelection());
    EXPECT_EQ(r.registry.GridCount(), 1u);
}

TEST(WorldGenViewHost, ZeroRiverWorldDrawsNoRiverLinesAndDoesNotRebuildEveryFrame)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    ASSERT_TRUE(r.host->SetOverlayEnabled("selection.neighbours", false));
    ASSERT_TRUE(r.host->SetOverlayEnabled("sites", false));   // sites default on
    // Strip the rivers from the generated world through the same session the host reads.
    WorldStructure& w = const_cast<WorldStructure&>(r.session.World());
    for (int i = 0; i < w.CellCount(); ++i)
        w.Cells()[i].waterSource = WaterSource::None;
    ASSERT_EQ(BuildRiverSegments(w, r.host->Grid()).size(), 0u);
    RecordingDraw draw;
    for (int i = 0; i < 5; ++i)
        r.Frame(draw);
    EXPECT_EQ(draw.lines.size(), 0u);
    EXPECT_EQ(r.host->RiverSegmentRebuilds(), 1u);
}

TEST(WorldGenViewHost, FillLegendBeforeBuildAndAfterTeardownIsNull)
{
    Rig r;
    Json::Value legend(Json::objectValue);
    r.host->FillLegend(legend);   // no layer registered, no domain
    EXPECT_TRUE(legend.isNull());
    ASSERT_TRUE(r.host->Build());
    r.host->Teardown();
    r.host->FillLegend(legend);
    EXPECT_TRUE(legend.isNull());
    bool any = false;
    r.host->ListOverlays([&](const char*, bool, bool) { any = true; });
    EXPECT_FALSE(any);
}

TEST(WorldGenViewHost, CellSelectCommandOutOfRangeReturnsErrorAndKeepsSelection)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    FakeDebugDraw draw;
    r.Frame(draw);
    ASSERT_TRUE(r.host->SelectCell(4, 4));

    const int bad[][2] = { { -1, 0 }, { 0, -1 }, { 66, 0 }, { 0, 42 }, { 100000, 100000 }, { (std::numeric_limits<int>::min)(), 5 } };
    for (const auto& b : bad)
    {
        Json::Value args(Json::objectValue);
        args["q"] = b[0];
        args["r"] = b[1];
        const Json::Value reply = r.registrar.Execute("cells.select.cell", args);
        SCOPED_TRACE(b[0]);
        EXPECT_FALSE(reply["success"].asBool()) << reply.toStyledString();
    }
    Json::Value missing(Json::objectValue);
    r.registrar.Execute("cells.select.cell", missing);   // no q/r at all: no crash
    int x = -1, y = -1;
    ASSERT_TRUE(r.host->GetSelection(x, y));
    EXPECT_EQ(x, 4);
    EXPECT_EQ(y, 4);
}

// ---- Regions plan task 10: region edges and the Sites overlay ----


namespace
{
std::vector<std::string> OverlayIds(WorldGenViewHost& h)
{
    std::vector<std::string> ids;
    h.ListOverlays([&](const char* id, bool, bool) { ids.push_back(id); });
    return ids;
}

void OnlyOverlay(Rig& r, const char* keep)
{
    for (const std::string& id : OverlayIds(*r.host))
        if (id.rfind("sites.", 0) != 0)   // per-kind toggles stay on; the master "sites" decides
            ASSERT_TRUE(r.host->SetOverlayEnabled(id.c_str(), id == keep)) << id;
}

size_t SiteLines(const Rig& r, const std::vector<SiteKindInfo>& kinds)
{
    size_t n = 0;
    for (const SiteKindInfo& k : kinds)
        n += BuildSiteMarkers(r.session.World(), r.host->Grid(), k.kindCrc, k.debugShape, r.host->Grid().CellSize() * WorldGenViewHost::kSiteMarkerScale).size();
    return n;
}
}

TEST(WorldGenViewHost, RegionEdgesOverlayStartsOffDrawsTheGeometryAndCachesPerRevision)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    ASSERT_TRUE(r.session.RegionsFresh());
    bool enabled = true;
    r.host->ListOverlays([&](const char* id, bool en, bool) { if (std::string(id) == "regions.edges") enabled = en; });
    EXPECT_FALSE(enabled) << "region edges start off";

    OnlyOverlay(r, "regions.edges");
    const size_t expected = BuildRegionEdges(r.session.World(), r.host->Grid()).size();
    ASSERT_GT(expected, 0u);
    RecordingDraw draw;
    r.Frame(draw);
    EXPECT_EQ(draw.lines.size(), expected);
    for (int i = 0; i < 10; ++i) r.Frame(draw);
    EXPECT_EQ(r.host->RegionEdgeRebuilds(), 1u);
    r.Tick(999);
    r.Frame(draw);
    EXPECT_EQ(r.host->RegionEdgeRebuilds(), 2u);
}

TEST(WorldGenViewHost, RegionEdgesAndSitesDrawNothingWhenRegionsAreNotFresh)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    r.session.Set(Obj("rivers", 0.31));   // live preview clears regions and sites
    ASSERT_TRUE(r.session.Tick(r.rng));
    ASSERT_FALSE(r.session.RegionsFresh());
    OnlyOverlay(r, "regions.edges");
    ASSERT_TRUE(r.host->SetOverlayEnabled("sites", true));
    RecordingDraw draw;
    r.Frame(draw);
    EXPECT_EQ(draw.lines.size(), 0u);
    bool edgesAvailable = true, sitesAvailable = true;
    r.host->ListOverlays([&](const char* id, bool, bool av) {
        if (std::string(id) == "regions.edges") edgesAvailable = av;
        if (std::string(id) == "sites") sitesAvailable = av;
    });
    EXPECT_FALSE(edgesAvailable);
    EXPECT_FALSE(sitesAvailable);
}

TEST(WorldGenViewHost, SitesOverlayDrawsEveryEnabledKindGenericallyAndPerKindTogglesRemoveOneKind)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    OnlyOverlay(r, "sites");
    const std::vector<SiteKindInfo> kinds = ListSiteKinds(r.session.Registry());
    ASSERT_EQ(kinds.size(), 4u);
    const size_t all = SiteLines(r, kinds);
    ASSERT_GT(all, 0u);
    RecordingDraw draw;
    r.Frame(draw);
    EXPECT_EQ(draw.lines.size(), all);

    ASSERT_TRUE(r.host->SetOverlayEnabled("sites.CoW.Node.Hunting", false));
    std::vector<SiteKindInfo> rest;
    for (const SiteKindInfo& k : kinds)
        if (k.kindCrc != SiteKindId(BuiltinNodeKinds::Hunting())) rest.push_back(k);
    RecordingDraw less;
    r.Frame(less);
    EXPECT_EQ(less.lines.size(), SiteLines(r, rest));
    EXPECT_LT(less.lines.size(), all);

    ASSERT_TRUE(r.host->SetOverlayEnabled("sites", false));
    RecordingDraw none;
    r.Frame(none);
    EXPECT_EQ(none.lines.size(), 0u);
    EXPECT_FALSE(r.host->SetOverlayEnabled("sites.Unknown.Kind", true));
}

TEST(WorldGenViewHost, RegisteredKindGetsAToggleWithItsColourAndShape)
{
    Rig r(nullptr, false);
    SiteRegistry registry;
    Testing::TestPlacer placer("Test.Hearth", {}, 0);
    ASSERT_TRUE(registry.Register(&placer));
    r.session.SetSiteRegistry(&registry);
    r.Tick(12345);
    ASSERT_TRUE(r.host->Build());
    ASSERT_EQ(r.session.World().sites.Count(Dia::Core::StringCRC("Test.Hearth")), 1);

    const std::vector<std::string> ids = OverlayIds(*r.host);
    ASSERT_EQ(ids.size(), 11u);
    EXPECT_EQ(ids[8], "sites.Test.Hearth");

    struct KindRow { std::string overlay, kind; uint32_t colour; SiteShape shape; bool enabled; int count; };
    std::vector<KindRow> rows;
    r.host->ListSiteKinds([&](const IWorldGenView::SiteKindRow& k) {
        rows.push_back({ k.overlay, k.info->id, k.info->debugColour, k.info->debugShape, k.enabled, k.count });
    });
    ASSERT_EQ(rows.size(), 5u);
    EXPECT_EQ(rows[4].overlay, "sites.Test.Hearth");
    EXPECT_EQ(rows[4].kind, "Test.Hearth");
    EXPECT_EQ(rows[4].colour, 0xFF00FFu);
    EXPECT_EQ(rows[4].shape, SiteShape::Dot);
    EXPECT_TRUE(rows[4].enabled);
    EXPECT_EQ(rows[4].count, 1);
    EXPECT_EQ(rows[0].kind, "CoW.Node.Corridor");
    EXPECT_EQ(rows[0].shape, SiteShape::Diamond);

    ASSERT_TRUE(r.host->SetOverlayEnabled("sites.Test.Hearth", false));
    r.host->ListSiteKinds([&](const IWorldGenView::SiteKindRow& k) { if (std::string(k.info->id) == "Test.Hearth") EXPECT_FALSE(k.enabled); });

    OnlyOverlay(r, "sites");
    ASSERT_TRUE(r.host->SetOverlayEnabled("sites.Test.Hearth", true));
    RecordingDraw draw;
    r.Frame(draw);
    EXPECT_EQ(draw.lines.size(), SiteLines(r, ListSiteKinds(registry))) << "no per-kind drawer code: the test kind draws too";

    r.session.SetSiteRegistry(nullptr);   // registry outlives the session's use of it
}

// ---- worldgen-panel-rest plan task 4: RegionEdges / Sites / DynastyStarts / GuaranteeEvidence drawers ----

namespace
{
// DebugFrameData's geometry budget: lines past kCap are counted as dropped (the real IDebugDraw behaviour).
struct CappedDraw : public RecordingDraw
{
    static constexpr size_t kCap = 8192;
    uint32_t dropped = 0;
    void RequestDraw(const Vector2D& a, const Vector2D& b, Dia::Core::RGBA c) override
    {
        if (lines.size() >= kCap) { ++dropped; return; }
        RecordingDraw::RequestDraw(a, b, c);
    }
    uint32_t DroppedCount() const override { return dropped; }
};

bool OverlayEnabled(WorldGenViewHost& h, const char* id, bool* available = nullptr)
{
    bool en = false;
    bool found = false;
    h.ListOverlays([&](const char* i, bool e, bool a) { if (std::string(i) == id) { en = e; found = true; if (available) *available = a; } });
    EXPECT_TRUE(found) << id;
    return en;
}

std::vector<std::pair<std::string, bool>> OverlayState(WorldGenViewHost& h)
{
    std::vector<std::pair<std::string, bool>> v;
    h.ListOverlays([&](const char* i, bool e, bool) { v.push_back({ i, e }); });
    return v;
}

void AllOverlays(Rig& r, bool on)
{
    for (const std::string& id : OverlayIds(*r.host))
        ASSERT_TRUE(r.host->SetOverlayEnabled(id.c_str(), on)) << id;
}

void SetWorldDir(Rig& r, const std::filesystem::path& dir)
{
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    r.session.SetWorldDirectories("", dir.string());
}

size_t DynastyLines(Rig& r, const GuaranteeResult* spacing)
{
    float spacingParam = kDefaultMinimumDynastySpacing;
    if (spacing)
        for (const Measurement& m : spacing->measurements)
            if (m.keyCrc == MeasurementKeys::MinimumSpacing().Value()) spacingParam = m.value;
    const float radius = r.host->Grid().CellSize() * WorldGenViewHost::kSiteMarkerScale;
    size_t n = 0;
    for (const DynastyStartMarker& m : BuildDynastyStarts(r.session.World(), r.host->Grid(), spacingParam, spacing, radius))
        n += m.marker.size() + m.ring.size();
    return n;
}

const ValidationResult* Fresh(Rig& r)
{
    return FreshEvidence(r.session.LastReport(), &r.session.Evidence(), r.session.EvidenceRevision(), r.session.Revision());
}

// Places sites of one built-in dragon-layer kind on the main landmass, evenly spread through its land cells (row order).
// S-1 ships no Hearth / Nexus / DynastyStart placer yet, so the evidence and start drawers need these.
class MainlandPlacer : public ISitePlacer
{
public:
    MainlandPlacer(const Dia::Core::StringCRC& kind, const char* name, int slot, int count)
        : mInfo(MakeSiteKindInfo(kind, name, 0x00C0FF, SiteShape::Ring)), mSlot(slot), mCount(count) {}
    const SiteKindInfo& Info() const override { return mInfo; }
    std::span<const Dia::Core::StringCRC> DependsOn() const override { return {}; }
    PlaceResult Place(const PlacementContext& ctx, Dia::Maths::RandomStream&, SiteSet& out) override
    {
        std::vector<uint32_t> land;
        for (int i = 0; i < ctx.world.CellCount(); ++i)
        {
            const Cell& c = ctx.world.Cells()[i];
            if (c.landmass == ctx.world.mainLandmass && c.waterSource == WaterSource::None)
                land.push_back(static_cast<uint32_t>(i));
        }
        if (land.empty())
            return PlaceResult::TerrainCause("no land");
        for (int k = 0; k < mCount; ++k)
        {
            Site s;
            s.kindCrc = mInfo.kindCrc;
            s.cell = land[land.size() * static_cast<size_t>(mSlot + k + 1) / 8u];
            out.Add(s);
        }
        return PlaceResult::Ok();
    }
private:
    SiteKindInfo mInfo;
    int mSlot;
    int mCount;
};

// 2 hearths, 2 nexuses, 3 dynasty starts: enough for the HearthNexus proof paths and the spacing rings.
struct DragonSites
{
    MainlandPlacer hearth{ SiteKinds::Hearth(), "Hearth", 0, 2 };
    MainlandPlacer nexus{ SiteKinds::Nexus(), "Nexus", 2, 2 };
    MainlandPlacer start{ SiteKinds::DynastyStart(), "Dynasty start", 4, 3 };
    SiteRegistry registry;
    DragonSites() { registry.Register(&hearth); registry.Register(&nexus); registry.Register(&start); }
    // For a session-owned rig: registry must outlive the rig (declare DragonSites first), generation uses it from here on.
    void Install(Rig& r) { r.session.SetSiteRegistry(&registry); r.Tick(12345); }
};

// First guarantee in the fresh evidence with proof paths.
uint32_t GuaranteeWithProofs(Rig& r)
{
    const ValidationResult* ev = Fresh(r);
    if (ev)
        for (const GuaranteeResult& g : ev->guarantees)
            if (g.proofPaths.Size() > 0) return g.idCrc;
    return 0;
}

}

TEST(WorldGenViewHost, PanelRestDrawersAreRegisteredWithTheSpecDefaults)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    EXPECT_EQ(r.registry.OverlayCount(), 6u);   // + dynasty.starts, guarantee.evidence
    EXPECT_NE(r.registry.FindOverlay(Dia::Core::StringCRC("dynasty.starts")), nullptr);
    EXPECT_NE(r.registry.FindOverlay(Dia::Core::StringCRC("guarantee.evidence")), nullptr);
    EXPECT_TRUE(OverlayEnabled(*r.host, "rivers"));
    EXPECT_FALSE(OverlayEnabled(*r.host, "regions.edges"));
    EXPECT_TRUE(OverlayEnabled(*r.host, "sites")) << "Sites default On";
    EXPECT_TRUE(OverlayEnabled(*r.host, "sites.CoW.Node.Corridor"));
    EXPECT_FALSE(OverlayEnabled(*r.host, "dynasty.starts"));
    EXPECT_FALSE(OverlayEnabled(*r.host, "guarantee.evidence"));
    r.host->Teardown();
    EXPECT_EQ(r.registry.OverlayCount(), 0u);
}

TEST(WorldGenViewHost, RegionEdgesAreDrawnInTheRegionPaletteColours)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    OnlyOverlay(r, "regions.edges");
    RecordingDraw draw;
    r.Frame(draw);
    ASSERT_EQ(draw.lines.size(), BuildColouredRegionEdges(r.session.World(), r.host->Grid()).size());
    std::set<uint32_t> colours;
    for (const auto& l : draw.lines) colours.insert((l.colour.R() << 16) | (l.colour.G() << 8) | l.colour.B());
    EXPECT_GT(colours.size(), 1u) << "one colour per region slot, not a single white";
}

TEST(WorldGenViewHost, DynastyStartsToggleAddsAndRemovesMarkersAndRings)
{
    DragonSites sites;
    Rig r(nullptr, false);
    sites.Install(r);
    ASSERT_TRUE(r.host->Build());
    OnlyOverlay(r, "sites");
    ASSERT_TRUE(r.host->SetOverlayEnabled("sites", false));
    const ValidationResult* ev = Fresh(r);
    const size_t expected = DynastyLines(r, ev ? ev->Find(Dia::Core::StringCRC("CoW.Guarantee.DynastySpacing")) : nullptr);
    ASSERT_GT(expected, 0u);
    RecordingDraw off;
    r.Frame(off);
    EXPECT_EQ(off.lines.size(), 0u);
    ASSERT_TRUE(r.host->SetOverlayEnabled("dynasty.starts", true));
    RecordingDraw on;
    r.Frame(on);
    EXPECT_EQ(on.lines.size(), expected);
    ASSERT_TRUE(r.host->SetOverlayEnabled("dynasty.starts", false));
    RecordingDraw off2;
    r.Frame(off2);
    EXPECT_EQ(off2.lines.size(), 0u);
}

TEST(WorldGenViewHost, ExpandGuaranteeChangesThePolylinesDrawn)
{
    DragonSites sites;
    Rig r(nullptr, false);
    sites.Install(r);
    ASSERT_TRUE(r.host->Build());
    ASSERT_TRUE(r.session.Validate(Json::Value(Json::objectValue))["success"].asBool());
    const uint32_t id = GuaranteeWithProofs(r);
    ASSERT_NE(id, 0u);
    OnlyOverlay(r, "guarantee.evidence");
    ASSERT_TRUE(r.host->SetOverlayEnabled("sites", false));
    EXPECT_EQ(r.host->ExpandedGuarantee(), 0u);

    RecordingDraw collapsed;
    r.Frame(collapsed);
    EXPECT_EQ(collapsed.lines.size(), BuildGuaranteeEvidence(Fresh(r), r.host->Grid(), 0).LineCount());

    r.host->ExpandGuarantee(id);
    EXPECT_EQ(r.host->ExpandedGuarantee(), id);
    const size_t expanded = BuildGuaranteeEvidence(Fresh(r), r.host->Grid(), id).LineCount();
    ASSERT_GT(expanded, collapsed.lines.size());
    RecordingDraw draw;
    r.Frame(draw);
    EXPECT_EQ(draw.lines.size(), expanded);

    r.host->ExpandGuarantee(0);
    RecordingDraw back;
    r.Frame(back);
    EXPECT_EQ(back.lines.size(), collapsed.lines.size());

    ASSERT_TRUE(r.host->SetOverlayEnabled("guarantee.evidence", false));
    r.host->ExpandGuarantee(id);
    RecordingDraw none;
    r.Frame(none);
    EXPECT_EQ(none.lines.size(), 0u);
}

TEST(WorldGenViewHost, StaleEvidenceDrawsNothingAndSaysWhy)
{
    DragonSites sites;
    Rig r(nullptr, false);
    sites.Install(r);
    ASSERT_TRUE(r.host->Build());
    ASSERT_TRUE(r.session.Validate(Json::Value(Json::objectValue))["success"].asBool());
    const uint32_t id = GuaranteeWithProofs(r);
    ASSERT_NE(id, 0u);
    OnlyOverlay(r, "guarantee.evidence");
    ASSERT_TRUE(r.host->SetOverlayEnabled("sites", false));
    r.host->ExpandGuarantee(id);
    bool available = false;
    OverlayEnabled(*r.host, "guarantee.evidence", &available);
    EXPECT_TRUE(available);
    EXPECT_EQ(r.host->EvidenceHiddenReason(), "");
    RecordingDraw fresh;
    r.Frame(fresh);
    ASSERT_GT(fresh.lines.size(), 0u);

    r.session.Set(Obj("rivers", 0.31));   // live preview: a new revision, evidence belongs to the old world
    ASSERT_TRUE(r.session.Tick(r.rng));
    RecordingDraw stale;
    r.Frame(stale);
    EXPECT_EQ(stale.lines.size(), 0u);
    EXPECT_NE(r.host->EvidenceHiddenReason().find("Evidence hidden"), std::string::npos);
    OverlayEnabled(*r.host, "guarantee.evidence", &available);
    EXPECT_FALSE(available);
}

TEST(WorldGenViewHost, FailedGenerationHidesEvidenceWithTheFailedReason)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    WorldGeneration::Testing::FailFirstNGuarantee flaky("Test.Flaky", 100);
    SiteRegistry registry;
    ASSERT_TRUE(registry.RegisterGuarantee(&flaky));
    r.session.SetSiteRegistry(&registry);
    ASSERT_TRUE(r.session.Regenerate(Obj("maxRetryAttempts", 2))["success"].asBool());
    for (int i = 0; i < 80 && (r.session.IsDirty() || r.session.IsGenerating()); ++i) r.session.Tick(r.rng);
    ASSERT_NE(r.session.LastReport(), nullptr);
    ASSERT_NE(r.session.LastReport()->outcome, GenerationOutcome::Success);
    OnlyOverlay(r, "guarantee.evidence");
    ASSERT_TRUE(r.host->SetOverlayEnabled("sites", false));
    RecordingDraw draw;
    r.Frame(draw);
    EXPECT_EQ(draw.lines.size(), 0u);
    EXPECT_EQ(r.host->EvidenceHiddenReason(), "Evidence hidden: last generation failed (see Validation)");
    r.session.SetSiteRegistry(nullptr);
}

TEST(WorldGenViewHost, EvidenceFromBeforeAFailedRunIsHiddenAndAValidateAfterItIsShown)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    WorldGeneration::Testing::FailFirstNGuarantee flaky("Test.Flaky", 100);
    SiteRegistry registry;
    ASSERT_TRUE(registry.RegisterGuarantee(&flaky));
    r.session.SetSiteRegistry(&registry);
    ASSERT_TRUE(r.session.Validate(Json::Value(Json::objectValue))["success"].asBool());
    ASSERT_NE(Fresh(r), nullptr);
    EXPECT_EQ(r.host->EvidenceHiddenReason(), "");

    ASSERT_TRUE(r.session.Regenerate(Obj("maxRetryAttempts", 2))["success"].asBool());
    for (int i = 0; i < 80 && (r.session.IsDirty() || r.session.IsGenerating()); ++i) r.session.Tick(r.rng);
    ASSERT_TRUE(r.session.LastRunKeptPreviousWorld());
    ASSERT_EQ(r.session.EvidenceRevision(), r.session.Revision()) << "the pre-failure Validate still matches the kept world's revision";
    EXPECT_EQ(r.host->EvidenceHiddenReason(), "Evidence hidden: last generation failed (see Validation)");

    ASSERT_TRUE(r.session.Validate(Json::Value(Json::objectValue))["success"].asBool());
    EXPECT_EQ(r.host->EvidenceHiddenReason().find("last generation failed"), std::string::npos);
    r.session.SetSiteRegistry(nullptr);
}

namespace
{
// Fails with a few violating cells and many long proof paths (far more lines than the evidence budget).
class ManyPathsFailingGuarantee : public IWorldGuarantee
{
public:
    Dia::Core::StringCRC GetId() const override { return Dia::Core::StringCRC("Test.ManyPaths"); }
    GuaranteeResult Check(const ValidationContext&) const override
    {
        GuaranteeResult g;
        g.idCrc = GetId().Value();
        g.satisfied = false;
        g.AddDiagnostic("many paths");
        for (uint32_t i = 0; i < 40; ++i)
        {
            ProofPath p;
            for (uint32_t c = 0; c < 60; ++c) p.cells.Add(100 + i * 60 + c);
            g.proofPaths.Add(p);
        }
        for (uint32_t c = 0; c < 3; ++c) g.violatingCells.Add(10 + c * 7);
        return g;
    }
};
}

TEST(WorldGenViewHost, LongProofPathsNeverStarveTheViolationMarksAndTruncationIsReported)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    ManyPathsFailingGuarantee many;
    SiteRegistry registry;
    ASSERT_TRUE(registry.RegisterGuarantee(&many));
    r.session.SetSiteRegistry(&registry);
    ASSERT_TRUE(r.session.Validate(Json::Value(Json::objectValue))["success"].asBool());
    OnlyOverlay(r, "guarantee.evidence");
    ASSERT_TRUE(r.host->SetOverlayEnabled("sites", false));
    r.host->ExpandGuarantee(Dia::Core::StringCRC("Test.ManyPaths").Value());

    const GuaranteeEvidenceGeometry geo = BuildGuaranteeEvidence(Fresh(r), r.host->Grid(), Dia::Core::StringCRC("Test.ManyPaths").Value());
    size_t markLines = 0;
    for (const ViolatingCellMark& v : geo.violations) markLines += v.outline.size() + v.cross.size();
    ASSERT_GT(markLines, 0u);
    ASSERT_GT(geo.LineCount(), WorldGenViewHost::kEvidenceLineBudget);

    RecordingDraw draw;
    r.Frame(draw);
    EXPECT_EQ(draw.lines.size(), WorldGenViewHost::kEvidenceLineBudget);
    size_t red = 0;
    for (const auto& l : draw.lines) if (l.colour.R() == 255 && l.colour.G() == 60 && l.colour.B() == 60) ++red;
    EXPECT_EQ(red, markLines);
    EXPECT_NE(r.host->EvidenceHiddenReason().find("Evidence truncated"), std::string::npos);
    r.session.SetSiteRegistry(nullptr);
}

TEST(WorldGenViewHost, ImportedWorldHasNoEvidenceAndSaysSo)
{
    Rig r;
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "cow_view_host_import";
    SetWorldDir(r, dir);
    ASSERT_TRUE(r.host->Build());
    ASSERT_TRUE(r.session.ExportWorld(Obj("name", "w"))["success"].asBool());
    ASSERT_TRUE(r.session.ImportWorld(Obj("name", "w"))["success"].asBool());
    OnlyOverlay(r, "guarantee.evidence");
    ASSERT_TRUE(r.host->SetOverlayEnabled("sites", false));
    RecordingDraw draw;
    r.Frame(draw);
    EXPECT_EQ(draw.lines.size(), 0u);
    EXPECT_NE(r.host->EvidenceHiddenReason().find("Evidence hidden"), std::string::npos);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST(WorldGenViewHost, DrawerToggleStateAndExpandedGuaranteeSurviveRegenerateSizeChangeAndImport)
{
    Rig r;
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "cow_view_host_persist";
    SetWorldDir(r, dir);
    ASSERT_TRUE(r.host->Build());
    const uint32_t hearthNexus = Dia::Core::StringCRC("CoW.Guarantee.HearthNexus").Value();
    ASSERT_TRUE(r.host->SetOverlayEnabled("regions.edges", true));
    ASSERT_TRUE(r.host->SetOverlayEnabled("sites", false));
    ASSERT_TRUE(r.host->SetOverlayEnabled("sites.CoW.Node.Hunting", false));
    ASSERT_TRUE(r.host->SetOverlayEnabled("dynasty.starts", true));
    ASSERT_TRUE(r.host->SetOverlayEnabled("guarantee.evidence", true));
    r.host->ExpandGuarantee(hearthNexus);
    const auto before = OverlayState(*r.host);
    FakeDebugDraw draw;

    r.Tick(999);   // regenerate
    r.Frame(draw);
    EXPECT_EQ(OverlayState(*r.host), before);
    EXPECT_EQ(r.host->ExpandedGuarantee(), hearthNexus);

    r.Resize("Standard");   // size change re-registers the layers
    r.Frame(draw);
    EXPECT_EQ(OverlayState(*r.host), before);

    ASSERT_TRUE(r.session.ExportWorld(Obj("name", "w"))["success"].asBool());
    ASSERT_TRUE(r.session.ImportWorld(Obj("name", "w"))["success"].asBool());
    r.Frame(draw);
    EXPECT_EQ(OverlayState(*r.host), before);
    EXPECT_EQ(r.host->ExpandedGuarantee(), hearthNexus);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST(WorldGenViewHost, EveryDrawerOnAtLargeWithEvidenceExpandedNeverDropsGeometry)
{
    DragonSites sites;
    Rig r(nullptr, false);
    r.session.SetSize(Obj("size", "Large"));
    sites.Install(r);
    ASSERT_TRUE(r.host->Build());
    ASSERT_TRUE(r.session.Validate(Json::Value(Json::objectValue))["success"].asBool());
    AllOverlays(r, true);
    ASSERT_TRUE(r.host->SelectCell(52, 32));
    std::vector<uint32_t> ids{ 0 };
    ASSERT_NE(Fresh(r), nullptr);
    for (const GuaranteeResult& g : Fresh(r)->guarantees) ids.push_back(g.idCrc);
    size_t worst = 0;
    for (const uint32_t id : ids)
    {
        r.host->ExpandGuarantee(id);
        CappedDraw draw;
        r.Frame(draw);
        worst = (std::max)(worst, draw.lines.size());
        EXPECT_EQ(draw.DroppedCount(), 0u) << "expanded " << id;
    }
    std::cout << "[ measured ] Large, every drawer on, worst expanded guarantee: " << worst << " lines\n";
    EXPECT_EQ(r.host->DroppedCount(), 0u);
    EXPECT_LT(worst, CappedDraw::kCap);
}

TEST(WorldGenViewHost, DomainViewJsonListsTheNewDrawersWithLabelsAvailabilityAndEvidenceReason)
{
    DragonSites sites;
    WorldGenerationModule module{ Dia::Core::StringCRC("WorldGenerationModule") };
    WorldGenDebugDomain domain{ module };
    Dia::Maths::RandomStream rng{ 7 };
    Dia::Graphics::CellFieldRegistry cellField;
    Dia::CellLayers::CellLayerRegistry registry{ nullptr, &Dia::CellLayers::Testing::FakePuIdFn };
    FakeMirrorRegistrar registrar;
    Dia::CellLayers::Testing::FakePuId() = 1;
    ASSERT_TRUE(module.GetSiteRegistry().Register(&sites.hearth));
    ASSERT_TRUE(module.GetSiteRegistry().Register(&sites.nexus));
    ASSERT_TRUE(module.GetSiteRegistry().Register(&sites.start));
    WorldGenViewHost::Deps d;
    d.registry = &registry;
    d.cellField = &cellField;
    d.registrar = &registrar;
    WorldGenViewHost host(module.GetSession(), d);
    ASSERT_TRUE(host.Build());
    domain.SetView(&host);
    auto state = [&]() { Json::Value s; domain.ForceFullFrame(); domain.GetJSONState(s); return s["view"]; };
    auto find = [](const Json::Value& view, const char* name) -> Json::Value
    {
        for (const Json::Value& e : view["drawers"]) if (e["name"].asString() == name) return e;
        return Json::Value(Json::nullValue);
    };

    // No world yet: nothing to draw.
    Json::Value v = state();
    EXPECT_FALSE(find(v, "regions.edges")["available"].asBool());
    EXPECT_FALSE(find(v, "guarantee.evidence")["available"].asBool());

    module.GetSession().Regenerate(Obj("seed", 12345));
    for (int i = 0; i < 80 && (module.GetSession().IsDirty() || module.GetSession().IsGenerating()); ++i) module.GetSession().Tick(rng);
    v = state();
    EXPECT_EQ(find(v, "regions.edges")["label"].asString(), "Region edges");
    EXPECT_TRUE(find(v, "regions.edges")["available"].asBool());
    EXPECT_FALSE(find(v, "regions.edges")["enabled"].asBool());
    EXPECT_EQ(find(v, "sites")["label"].asString(), "Sites");
    EXPECT_TRUE(find(v, "sites")["enabled"].asBool());
    EXPECT_EQ(find(v, "sites.CoW.Node.Hunting")["label"].asString().rfind("Sites ", 0), 0u);
    EXPECT_EQ(find(v, "dynasty.starts")["label"].asString(), "Dynasty starts");
    EXPECT_TRUE(find(v, "dynasty.starts")["available"].asBool());
    EXPECT_EQ(find(v, "guarantee.evidence")["label"].asString(), "Guarantee evidence");
    EXPECT_TRUE(find(v, "guarantee.evidence")["available"].asBool()) << "a successful generation's report is fresh evidence";
    EXPECT_FALSE(v.isMember("evidenceHidden"));

    // expandGuarantee {id}: stored on the host; 0 collapses.
    const uint32_t id = Dia::Core::StringCRC("CoW.Guarantee.HearthNexus").Value();
    domain.OnCommand(Dia::Core::StringCRC("expandGuarantee"), Obj("id", id));
    EXPECT_EQ(host.ExpandedGuarantee(), id);
    EXPECT_EQ(state()["expandedGuarantee"].asUInt(), id);
    domain.OnCommand(Dia::Core::StringCRC("expandGuarantee"), Obj("id", 0));
    EXPECT_EQ(host.ExpandedGuarantee(), 0u);
    domain.OnCommand(Dia::Core::StringCRC("expandGuarantee"), Obj("id", id));
    domain.OnCommand(Dia::Core::StringCRC("expandGuarantee"), Json::Value(Json::objectValue));   // no id: collapse
    EXPECT_EQ(host.ExpandedGuarantee(), 0u);

    // Bad ids collapse (non-numeric, negative, fractional); an unknown but numeric id is stored and matches no guarantee.
    for (const Json::Value& bad : { Json::Value("abc"), Json::Value(-3), Json::Value(1.5), Json::Value(Json::nullValue) })
    {
        domain.OnCommand(Dia::Core::StringCRC("expandGuarantee"), Obj("id", id));
        domain.OnCommand(Dia::Core::StringCRC("expandGuarantee"), Obj("id", bad));
        EXPECT_EQ(host.ExpandedGuarantee(), 0u) << bad.toStyledString();
    }
    domain.OnCommand(Dia::Core::StringCRC("expandGuarantee"), Obj("id", 4040404u));
    EXPECT_EQ(host.ExpandedGuarantee(), 4040404u);
    domain.OnCommand(Dia::Core::StringCRC("expandGuarantee"), Json::Value(Json::nullValue));   // no args object at all
    EXPECT_EQ(host.ExpandedGuarantee(), 0u);

    // Toggling through the existing command works for the new drawers.
    domain.OnCommand(Dia::Core::StringCRC("toggle"), Obj("drawer", "dynasty.starts"));
    EXPECT_TRUE(OverlayEnabled(host, "dynasty.starts"));

    // A stale world hides evidence and the reason rides in the view JSON.
    module.GetSession().Set(Obj("rivers", 0.31));
    module.GetSession().Tick(rng);
    v = state();
    EXPECT_FALSE(find(v, "guarantee.evidence")["available"].asBool());
    EXPECT_NE(v["evidenceHidden"].asString().find("Evidence hidden"), std::string::npos);
    domain.SetView(nullptr);
}

TEST(WorldGenViewHost, CompactDomainStateAtLargeWithRealViewAndFailedReportStaysUnder12Kb)
{
    WorldGenerationModule module{ Dia::Core::StringCRC("WorldGenerationModule") };
    WorldGenDebugDomain domain{ module };
    Dia::Maths::RandomStream rng{ 7 };
    Dia::Graphics::CellFieldRegistry cellField;
    Dia::CellLayers::CellLayerRegistry registry{ nullptr, &Dia::CellLayers::Testing::FakePuIdFn };
    FakeMirrorRegistrar registrar;
    Dia::CellLayers::Testing::FakePuId() = 1;
    WorldGenSession& s = module.GetSession();
    auto settle = [&]() { for (int i = 0; i < 80 && (s.IsDirty() || s.IsGenerating()); ++i) s.Tick(rng); };
    s.SetSize(Obj("size", "Large"));
    s.Regenerate(Obj("seed", 12345));
    settle();
    WorldGenViewHost::Deps d;
    d.registry = &registry;
    d.cellField = &cellField;
    d.registrar = &registrar;
    WorldGenViewHost host(s, d);
    ASSERT_TRUE(host.Build());
    domain.SetView(&host);
    domain.OnCommand(Dia::Core::StringCRC("validate"), Json::Value(Json::objectValue));
    WorldGeneration::Testing::FailFirstNGuarantee flaky("Test.Flaky", 100);
    ASSERT_TRUE(module.GetSiteRegistry().RegisterGuarantee(&flaky));
    s.Regenerate(Obj("maxRetryAttempts", 2));
    settle();
    ASSERT_NE(s.LastReport(), nullptr);
    host.ExpandGuarantee(Dia::Core::StringCRC("CoW.Guarantee.HearthNexus").Value());
    Json::Value xy(Json::objectValue);
    xy["x"] = 52;
    xy["y"] = 32;
    domain.OnCommand(Dia::Core::StringCRC("selectCell"), xy);
    Json::Value st;
    domain.ForceFullFrame();
    domain.GetJSONState(st);
    Json::StreamWriterBuilder b;
    b["indentation"] = "";
    const size_t bytes = Json::writeString(b, st).size();
    std::cout << "[ measured ] compact WorldGen state at Large with real view, failed report: " << bytes << " bytes\n";
    for (const std::string& k : st.getMemberNames()) std::cout << "  [section] " << k << ": " << Json::writeString(b, st[k]).size() << "\n";
    for (const std::string& k : st["view"].getMemberNames()) std::cout << "  [view] " << k << ": " << Json::writeString(b, st["view"][k]).size() << "\n";
    EXPECT_LT(bytes, 12u * 1024u);
    domain.SetView(nullptr);
}

#endif // DIA_DEBUG
