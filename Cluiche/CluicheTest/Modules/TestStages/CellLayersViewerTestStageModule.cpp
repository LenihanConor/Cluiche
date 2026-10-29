#include "Modules/TestStages/CellLayersViewerTestStageModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaGraphics/CellField/CellFieldRegistry.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaPathfinding/HexTopology.h>

#ifdef DIA_DEBUG
#include <DiaAPI/DiaAPI.h>
#endif

#include <algorithm>
#include <cmath>

namespace CluicheTest {

namespace
{
    using Dia::CellLayers::CellValueKind;
    using Dia::Maths::Vector2D;
    using Dia::Pathfinding::CellCoord;
    using Dia::Pathfinding::HexTopology;

    const char* const kLayerTemperature = "temperature";
    const char* const kLayerRainfall    = "rainfall";
    const char* const kLayerBiome       = "biome";
    const char* const kLayerElevation   = "elevation";
    const char* const kSchemaFor        = "cluichetest.celllayers";

    // ValueSchemaRegistry full key = "<for>/<key>".
    Dia::Core::StringCRC SchemaKey(const char* key)
    {
        std::string full = std::string(kSchemaFor) + "/" + key;
        return Dia::Core::StringCRC(full.c_str());
    }

    float Clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    // Static (non-animated) value fields over cell coordinates.
    float StaticTemperature(int x, int y, int h)
    {
        const float lat = (static_cast<float>(y) / static_cast<float>(h > 1 ? h - 1 : 1)) - 0.5f; // -0.5..0.5
        return Clampf(16.0f + 20.0f * std::sin(x * 0.11f) * std::cos(y * 0.14f) - 18.0f * lat, -20.0f, 40.0f);
    }
    float StaticRainfall(int x, int y)
    {
        return Clampf(200.0f + 180.0f * std::sin(x * 0.2f + 1.0f) * std::sin(y * 0.25f + 2.0f), 0.0f, 400.0f);
    }
}

const Dia::Core::StringCRC CellLayersViewerTestStageModule::kTypeId("CellLayersViewerTestStageModule");

CellLayersViewerTestStageModule::CellLayersViewerTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

CellLayersViewerTestStageModule::~CellLayersViewerTestStageModule() = default;

Dia::Core::StringCRC CellLayersViewerTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("CellLayersViewerTestStage");
}

const Dia::Core::StringCRC* CellLayersViewerTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("test.cell_layers_viewer.passed")
    };
    outCount = 1;
    return names;
}

bool CellLayersViewerTestStageModule::AreDependenciesReady()
{
#ifdef DIA_DEBUG
    return mCellFieldStream.IsAvailable() && mSchemaStream.IsAvailable() && mLayerRegistryStream.IsAvailable();
#else
    return mCellFieldStream.IsAvailable();
#endif
}

void CellLayersViewerTestStageModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    TestStageModuleBase::OnConnectStreams(app);
    mCellFieldStream.Connect(app);
    mSchemaStream.Connect(app);
    mLayerRegistryStream.Connect(app);
}

void CellLayersViewerTestStageModule::BuildGrid(GridStorage& g, int w, int h, float size)
{
    HexTopology topo(w, h);
    const uint32_t count = topo.CellCount();
    g.width = w;
    g.height = h;
    g.centres.assign(2u * count, 0.0f);
    g.neighbours.assign(6u * count, -1);
    g.baseValue.assign(count, 0.0f);
    const float halfH = std::sqrt(3.0f) * size * 0.5f;
    g.boundsMin = Vector2D(1e30f, 1e30f);
    g.boundsMax = Vector2D(-1e30f, -1e30f);
    for (uint32_t i = 0; i < count; ++i)
    {
        const Vector2D c = HexTopology::CellCenter(topo.ToCoord(i), size);
        g.centres[2u * i]      = c.x;
        g.centres[2u * i + 1u] = c.y;
        g.boundsMin.x = (std::min)(g.boundsMin.x, c.x - size);
        g.boundsMin.y = (std::min)(g.boundsMin.y, c.y - halfH);
        g.boundsMax.x = (std::max)(g.boundsMax.x, c.x + size);
        g.boundsMax.y = (std::max)(g.boundsMax.y, c.y + halfH);
        for (uint32_t d = 0; d < 6u; ++d)
        {
            const uint32_t n = topo.NeighbourAt(i, d);
            g.neighbours[6u * i + d] = (n == Dia::Pathfinding::kNoCell) ? -1 : static_cast<int32_t>(n);
        }
    }
    g.desc = Dia::Graphics::CellGridDesc();
    g.desc.topology   = Dia::Core::StringCRC("hex.flat.oddq");
    g.desc.width      = static_cast<uint16_t>(w);
    g.desc.height     = static_cast<uint16_t>(h);
    g.desc.cellSize   = size;
    g.desc.centresXY  = g.centres.data();
    g.desc.neighbours = g.neighbours.data();
}

float CellLayersViewerTestStageModule::TemperatureAt(uint32_t index) const
{
    const HexTopology topo(kBigW, kBigH);
    const CellCoord c = topo.ToCoord(index);
    // Animation offset is 0 at version 1, so the default (non-animated) output is a pure function of the coordinates.
    const float anim = 8.0f * std::sin(static_cast<float>(mTempVersion - 1) * 0.35f);
    return Clampf(StaticTemperature(c.x, c.y, kBigH) + anim, -20.0f, 40.0f);
}

float CellLayersViewerTestStageModule::RainfallAt(uint32_t index) const
{
    const HexTopology topo(kBigW, kBigH);
    const CellCoord c = topo.ToCoord(index);
    return StaticRainfall(c.x, c.y);
}

void CellLayersViewerTestStageModule::ReadTemperature(float* out, uint32_t count)
{
    const uint32_t n = (std::min<uint32_t>)(count, kBigW * kBigH);
    for (uint32_t i = 0; i < n; ++i) out[i] = TemperatureAt(i);
}

void CellLayersViewerTestStageModule::ReadRainfall(float* out, uint32_t count)
{
    const uint32_t n = (std::min<uint32_t>)(count, kBigW * kBigH);
    for (uint32_t i = 0; i < n; ++i) out[i] = RainfallAt(i);
}

void CellLayersViewerTestStageModule::ReadBiome(float* out, uint32_t count)
{
    // Category index into the schema enum: tundra(cold,dry) forest(cold,wet) desert(warm,dry) jungle(warm,wet).
    const HexTopology topo(kBigW, kBigH);
    const uint32_t n = (std::min<uint32_t>)(count, kBigW * kBigH);
    for (uint32_t i = 0; i < n; ++i)
    {
        const CellCoord c = topo.ToCoord(i);
        const bool warm = StaticTemperature(c.x, c.y, kBigH) >= 15.0f;
        const bool wet  = StaticRainfall(c.x, c.y) >= 200.0f;
        out[i] = static_cast<float>((warm ? 2 : 0) + (wet ? 1 : 0));
    }
}

void CellLayersViewerTestStageModule::ReadElevation(float* out, uint32_t count)
{
    const HexTopology topo(kSmallW, kSmallH);
    const uint32_t n = (std::min<uint32_t>)(count, kSmallW * kSmallH);
    for (uint32_t i = 0; i < n; ++i)
    {
        const CellCoord c = topo.ToCoord(i);
        out[i] = Clampf(1500.0f + 1400.0f * std::sin(c.x * 0.4f) * std::cos(c.y * 0.5f), 0.0f, 3000.0f);
    }
}

void CellLayersViewerTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    mReady = false;
    mReadyFrames = 0;
    mAnimateCounter = 0;
    mAnimate = false;
    mTempVersion = 1;

    BuildGrid(mBig, kBigW, kBigH, kCellSize);
    BuildGrid(mSmall, kSmallW, kSmallH, kCellSize);

#ifdef DIA_DEBUG
    if (BuildViewer())
        mReady = true;
    else
        ReportFailed();
#else
    mReady = true;
#endif

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.cell_layers_viewer.passed"),
        [this]() -> Dia::Automation::CheckpointResult {
            if (mReady)
                return { true, "viewer hosted", 1.0f };
            return { false, "pending", 0.0f };
        });
}

void CellLayersViewerTestStageModule::OnUpdate(float /*deltaTime*/)
{
    if (!mReady)
        return;

    if (mAnimate && ++mAnimateCounter >= kAnimateEveryFrames)
    {
        mAnimateCounter = 0;
        ++mTempVersion;
    }

#ifdef DIA_DEBUG
    FitCameraToActiveGrid();
#endif

    if (++mReadyFrames >= kHoldFrames)
        ReportPassed();
}

void CellLayersViewerTestStageModule::OnStop()
{
#ifdef DIA_DEBUG
    TeardownViewer();
#endif
    mReady = false;
}

#ifdef DIA_DEBUG

bool CellLayersViewerTestStageModule::BuildViewer()
{
    using namespace Dia::CellLayers;

    mRegistry = FindCellLayerRegistry(mLayerRegistryStream);
    if (mRegistry == nullptr || !mCellFieldStream.IsAvailable())
    {
        DIA_LOG_ERROR("CluicheTest", "CellLayersViewerTestStage: CellLayerRegistry or CellFieldRegistry stream unavailable");
        return false;
    }
    const Dia::ValueSchema::ValueSchemaService* schema = mSchemaStream.IsAvailable() ? &mSchemaStream.Get() : nullptr;
    if (schema == nullptr)
        DIA_LOG_WARNING("CluicheTest", "CellLayersViewerTestStage: no ValueSchemaService - layers will show without schema data");

    auto makeLayer = [&](const char* id, const char* group, CellValueKind kind, GridStorage& grid,
                         fastdelegate::FastDelegate2<float*, uint32_t> read, fastdelegate::FastDelegate0<uint64_t> version) -> bool
    {
        CellLayerDesc d;
        d.id = Dia::Core::StringCRC(id);
        d.schemaKey = SchemaKey(id);
        d.group.Append(group);
        d.kind = kind;
        d.grid = &grid.desc;
        d.read = read;
        d.version = version;
        const CellRegisterResult r = mRegistry->Register(d);
        if (r != CellRegisterResult::Ok)
        {
            DIA_LOG_ERROR("CluicheTest", "CellLayersViewerTestStage: registering layer '%s' failed (%d)", id, static_cast<int>(r));
            return false;
        }
        return true;
    };

    // Registration order is irrelevant: CellLayersModule attaches the schema stream, so ui.default_base picks the base.
    const bool ok =
        makeLayer(kLayerTemperature, "Climate", CellValueKind::Float, mBig,
            fastdelegate::MakeDelegate(this, &CellLayersViewerTestStageModule::ReadTemperature),
            fastdelegate::MakeDelegate(this, &CellLayersViewerTestStageModule::VersionTemperature)) &&
        makeLayer(kLayerRainfall, "Climate", CellValueKind::Float, mBig,
            fastdelegate::MakeDelegate(this, &CellLayersViewerTestStageModule::ReadRainfall),
            fastdelegate::MakeDelegate(this, &CellLayersViewerTestStageModule::VersionRainfall)) &&
        makeLayer(kLayerBiome, "Terrain", CellValueKind::Category, mBig,
            fastdelegate::MakeDelegate(this, &CellLayersViewerTestStageModule::ReadBiome),
            fastdelegate::MakeDelegate(this, &CellLayersViewerTestStageModule::VersionBiome)) &&
        makeLayer(kLayerElevation, "Terrain", CellValueKind::Float, mSmall,
            fastdelegate::MakeDelegate(this, &CellLayersViewerTestStageModule::ReadElevation),
            fastdelegate::MakeDelegate(this, &CellLayersViewerTestStageModule::VersionElevation));
    if (!ok)
        return false;
    mLayersRegistered = true;

    CellOverlayDesc overlay;
    overlay.id = Dia::Core::StringCRC("grid_edges");
    overlay.group.Append("Debug");
    overlay.label.Append("Grid edges");
    overlay.draw = fastdelegate::MakeDelegate(this, &CellLayersViewerTestStageModule::DrawGridEdges);
    if (mRegistry->RegisterOverlay(overlay) != CellRegisterResult::Ok)
    {
        DIA_LOG_ERROR("CluicheTest", "CellLayersViewerTestStage: registering overlay failed");
        return false;
    }

    mUploader = Dia::Core::MakeUnique<LayerUploader>(mCellFieldStream.Get(), schema);
    mDomain = Dia::Core::MakeUnique<CellLayerViewerDomain>(*mRegistry, *mUploader, schema,
        CellLayerViewerDomain::ViewFn(fastdelegate::MakeDelegate(this, &CellLayersViewerTestStageModule::GetPickerView)));
    mDomain->RegisterCommands();

    Dia::API::RegisterCommand({
        Dia::Core::StringCRC("cellviewer.animate"),
        "Turn the CellLayersViewer test-stage temperature animation on or off (default off)",
        Dia::Core::StringCRC("cells"), "CluicheTest", "1.0.0",
        "cellviewer.animate 1",
        [this](const Dia::API::CommandArgs& args) -> int
        {
            if (args.positionalArgs.Size() < 1) { return 2; }
            mAnimate = (args.positionalArgs[0][0] == '1');
            return 0;
        }
    });

    // JSON form for the E2E/DebugServer channel (the CLI form takes a positional arg, which JSON params never fill).
    Dia::API::CommandInfoJson animateJson;
    animateJson.name = Dia::Core::StringCRC("cellviewer.set_animate");
    animateJson.description = "JSON form of cellviewer.animate: params {\"enabled\": true|false}; returns {enabled}";
    animateJson.category = Dia::Core::StringCRC("cells");
    animateJson.owner = "CluicheTest";
    animateJson.callback = [this](const Json::Value& params) -> Json::Value
    {
        Json::Value out(Json::objectValue);
        if (!params.isObject() || !params["enabled"].isBool())
        {
            out["error"] = "missing or non-bool param 'enabled'";
            return out;
        }
        mAnimate = params["enabled"].asBool();
        out["enabled"] = mAnimate;
        return out;
    };
    Dia::API::RegisterCommandJson(animateJson);
    mCommandRegistered = true;

    if (auto* vd = mVisualDebuggerRef.Get())
    {
        vd->RegisterDomain(*mDomain);
        mDomainRegistered = true;
        vd->GetLayerManager().Register(&mDebugLayer, 10, Dia::Core::StringCRC("CellLayersViewerTestStage"));
        mLayerRegistered = true;
    }
    else
    {
        DIA_LOG_ERROR("CluicheTest", "CellLayersViewerTestStage: VisualDebuggerModule unavailable - the viewer cannot draw or appear in the panel");
        return false;
    }

    mFittedGrid = nullptr;
    DIA_LOG_INFO("CluicheTest", "CellLayersViewerTestStage: viewer hosted (%u layers, %u overlays, %u grids)",
        mRegistry->LayerCount(), mRegistry->OverlayCount(), mRegistry->GridCount());
    return true;
}

void CellLayersViewerTestStageModule::TeardownViewer()
{
    if (auto* vd = mVisualDebuggerRef.Get())
    {
        if (mLayerRegistered)
            vd->GetLayerManager().Unregister(mDebugLayer.GetLayerName());
        if (mDomainRegistered && mDomain)
            vd->UnregisterDomain(*mDomain);
    }
    mLayerRegistered = false;
    mDomainRegistered = false;

    if (mCommandRegistered)
    {
        Dia::API::UnregisterCommand(Dia::Core::StringCRC("cellviewer.animate"));
        Dia::API::UnregisterCommandJson(Dia::Core::StringCRC("cellviewer.set_animate"));
        mCommandRegistered = false;
    }

    mDomain.Reset();    // unregisters its DiaAPI commands
    mUploader.Reset();  // releases the CellField layers it holds

    if (mRegistry != nullptr)
    {
        mRegistry->UnregisterOverlay(Dia::Core::StringCRC("grid_edges"));
        if (mLayersRegistered)
        {
            mRegistry->Unregister(Dia::Core::StringCRC(kLayerTemperature));
            mRegistry->Unregister(Dia::Core::StringCRC(kLayerRainfall));
            mRegistry->Unregister(Dia::Core::StringCRC(kLayerBiome));
            mRegistry->Unregister(Dia::Core::StringCRC(kLayerElevation));
        }
    }
    mLayersRegistered = false;
    mRegistry = nullptr;

    if (mCameraSaved)
    {
        if (auto* cam = mCameraRef.Get())
        {
            cam->GetActiveCamera().SetPosition(Vector2D(mSavedCamX, mSavedCamY));
            cam->GetActiveCamera().SetZoom(mSavedZoom);
        }
        mCameraSaved = false;
    }
    mFittedGrid = nullptr;
}

void CellLayersViewerTestStageModule::DrawGridEdges(Dia::Core::IDebugDraw& draw, const void* /*selection*/)
{
    const Dia::Core::RGBA colour(255, 220, 0, 255);
    const Vector2D a = mBig.boundsMin;
    const Vector2D b = mBig.boundsMax;
    draw.RequestDraw(Vector2D(a.x, a.y), Vector2D(b.x, a.y), colour);
    draw.RequestDraw(Vector2D(b.x, a.y), Vector2D(b.x, b.y), colour);
    draw.RequestDraw(Vector2D(b.x, b.y), Vector2D(a.x, b.y), colour);
    draw.RequestDraw(Vector2D(a.x, b.y), Vector2D(a.x, a.y), colour);
}

Dia::CellLayers::CellPickerView CellLayersViewerTestStageModule::GetPickerView()
{
    Dia::CellLayers::CellPickerView v;
    if (auto* cam = mCameraRef.Get())
    {
        v.viewportWidth  = cam->GetWindowSize().x;
        v.viewportHeight = cam->GetWindowSize().y;
        v.cameraX = cam->GetActiveCamera().GetPosition().x;
        v.cameraY = cam->GetActiveCamera().GetPosition().y;
        v.zoom    = cam->GetActiveCamera().GetZoom();
    }
    return v;
}

void CellLayersViewerTestStageModule::FitCameraToActiveGrid()
{
    if (!mDomain)
        return;
    const Dia::Graphics::CellGridDesc* active = mDomain->ActiveGrid();
    if (active == nullptr || active == mFittedGrid)
        return;
    const GridStorage* g = (active == &mBig.desc) ? &mBig : ((active == &mSmall.desc) ? &mSmall : nullptr);
    auto* cam = mCameraRef.Get();
    if (g == nullptr || cam == nullptr)
        return;
    const Vector2D win = cam->GetWindowSize();
    if (win.x <= 1.0f || win.y <= 1.0f)
        return; // window size not known yet: retry next frame

    auto& c = cam->GetActiveCamera();
    if (!mCameraSaved)
    {
        mSavedCamX = c.GetPosition().x;
        mSavedCamY = c.GetPosition().y;
        mSavedZoom = c.GetZoom();
        mCameraSaved = true;
    }
    const float bw = g->boundsMax.x - g->boundsMin.x;
    const float bh = g->boundsMax.y - g->boundsMin.y;
    const float zoom = 0.9f * (std::min)(win.x / bw, win.y / bh);
    c.SetPosition(Vector2D((g->boundsMin.x + g->boundsMax.x) * 0.5f, (g->boundsMin.y + g->boundsMax.y) * 0.5f));
    c.SetZoom(zoom);
    mFittedGrid = active;
}

void CellLayersViewerTestStageModule::ViewerDebugLayer::Draw(Dia::Core::IDebugDraw& draw)
{
    // Drives the viewer on SimPU: base resolve, version-gated upload, CellField draw request, overlays, hover.
    if (mModule->mDomain)
        mModule->mDomain->Update(draw);
}

#endif // DIA_DEBUG

} // namespace CluicheTest

namespace { using CellLayersViewerTestStageModule_ = CluicheTest::CellLayersViewerTestStageModule; }
DIA_MODULE(CellLayersViewerTestStageModule_);
DIA_DESCRIBE(CellLayersViewerTestStageModule_::kTypeId, "Hosts the DiaCellLayers viewer over synthetic layers on two hex grids.");
