// Spec: docs/specs/applications/cow/systems/s1-worldgeneration/worldgen-view.plan.md (task 7)
// WorldGenViewHost over fakes: in-process CellLayerRegistry + CellFieldRegistry, FakeMirrorRegistrar, FakeDebugDraw, port lambdas.
#include <gtest/gtest.h>

#ifdef DIA_DEBUG

#include "Modules/WorldGenViewHost.h"

#include <DiaCellLayers/Testing/CellLayerTestSupport.h>
#include <DiaCellLayers/Testing/FakeDebugDraw.h>
#include <DiaCellLayers/Testing/FakeMirrorRegistrar.h>
#include <DiaCellLayers/Testing/LayerUploaderTestSupport.h>
#include <DiaValueSchema/ValueSchemaLoader.h>

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

    Rig(const Dia::ValueSchema::ValueSchemaService* schema = nullptr, bool generate = true)
    {
        Dia::CellLayers::Testing::FakePuId() = 1;
        if (generate)
            Tick(12345);
        WorldGenViewHost::Deps d;
        d.registry = &registry;
        d.cellField = &cellField;
        d.schema = schema;
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

TEST(WorldGenViewHost, BuildRegistersEveryDescriptorAndTwoOverlaysAgainstOneGrid)
{
    Rig r;
    ASSERT_TRUE(r.host->Build());
    EXPECT_EQ(r.registry.LayerCount(), WorldLayerSource::Count());
    EXPECT_EQ(r.registry.OverlayCount(), 2u);
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
    ASSERT_EQ(rows.size(), 2u);
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
    ASSERT_EQ(avail.size(), 2u);
    EXPECT_FALSE(avail[0]) << "rivers unavailable without a world";
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
    EXPECT_EQ(r.registry.OverlayCount(), 2u);
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

#endif // DIA_DEBUG
