#include "Modules/TestStages/CellLayersViewerTestStageModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaGraphics/CellField/CellFieldRegistry.h>
#include <DiaCellHistory/CellHistoryRecorder.h>
#include <DiaValueSchema/ValueSchemaService.h>
#include <DiaPathfinding/HexTopology.h>

#ifdef DIA_DEBUG
#include <DiaAPI/DiaAPI.h>
#include <DiaApplicationFlow/IApplicationInspectable.h>
#include <DiaApplicationFlow/Streams/ServiceStreamStore.h>
#include <DiaDebugServer/DebugServer.h>
#include "Modules/TestStages/CellLayersPointerFrame.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
#include <filesystem>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

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
    const char* const kDetailProviderName = "Stage";

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
    mHistoryStream.Connect(app);
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
    g.desc.id         = Dia::Core::StringCRC("test.hex");
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
#ifdef DIA_DEBUG
    mHistoryFrame = 0;
#endif
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
            {
                // Publishes the per-biome cell counts (index = schema enum order) so E2E can check category selection.
                std::vector<float> biome(static_cast<size_t>(kBigW) * kBigH, 0.0f);
                ReadBiome(biome.data(), static_cast<uint32_t>(biome.size()));
                uint32_t counts[4] = { 0, 0, 0, 0 };
                for (float v : biome)
                    ++counts[static_cast<int>(v) & 3];
                static thread_local char msg[96]; // CheckpointResult::message is a non-owning pointer
                snprintf(msg, sizeof(msg), "viewer hosted; biome_counts=%u,%u,%u,%u", counts[0], counts[1], counts[2], counts[3]);
                return { true, msg, 1.0f };
            }
            return { false, "pending", 0.0f };
        });
}

void CellLayersViewerTestStageModule::OnUpdate(float /*deltaTime*/)
{
    if (!mReady)
        return;

#ifdef DIA_DEBUG
    // Sim frame boundary: a queued snapshot take/restore runs here, before this frame mutates the animated state.
    if (mSnapshotStore)
        mSnapshotStore->RunFrameBoundary();
#endif

    if (mAnimate.load() && ++mAnimateCounter >= kAnimateEveryFrames)
    {
        mAnimateCounter = 0;
        ++mTempVersion;
    }

#ifdef DIA_DEBUG
    FitCameraToActiveGrid();
    SubmitPointerInput();
    SampleHistory();
    UpdateHistoryScrub();
    if (mHistoryTopics)
        mHistoryTopics->Tick();   // after sampling; UpdateHistoryScrub has already torn the topics down if the recorder went away
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

    if (mRegistry->Details().Register(&mBig.desc, kDetailProviderName,
            fastdelegate::MakeDelegate(this, &CellLayersViewerTestStageModule::BuildCellDetails)) != CellDetailRegisterResult::Ok)
    {
        DIA_LOG_ERROR("CluicheTest", "CellLayersViewerTestStage: registering the cell detail provider failed");
        return false;
    }
    mDetailsRegistered = true;

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
    BuildViewerCommands();
    BuildMirror(schema); // optional: a missing DebugServer only logs a warning

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
        out["enabled"] = mAnimate.load();
        return out;
    };
    Dia::API::RegisterCommandJson(animateJson);
    mCommandRegistered = true;
    RegisterInjectionCommands();   // after BuildViewerCommands: needs the registrar
    RegisterHistoryCommand();

    // The registry is read on Main by the debug panel and debug.domain_state, so it gets the snapshot adapter, never the live viewer.
    // The adapter forwards OnCommand (dispatched on SimPU) to the viewer.
    Cluiche::AppFlow::DomainStateRelayModule* relay = mStateRelayRef.Get();
    mPanelDomain = relay != nullptr ? relay->Attach(*mDomain) : nullptr;
    if (mPanelDomain == nullptr)
    {
        DIA_LOG_ERROR("CEL", "CellLayersViewerTestStage: state relay unavailable - the viewer cannot appear in the panel");
        return false;
    }

    if (auto* vd = mVisualDebuggerRef.Get())
    {
        vd->RegisterDomain(*mPanelDomain);
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
    mPointerFrameLogged = false;
    mInjectLogged = false;
    mPointerQueue.Clear();
    DIA_LOG_INFO("CluicheTest", "CellLayersViewerTestStage: viewer hosted (%u layers, %u overlays, %u grids)",
        mRegistry->LayerCount(), mRegistry->OverlayCount(), mRegistry->GridCount());
    return true;
}

void CellLayersViewerTestStageModule::TeardownViewer()
{
    TeardownHistoryScrub();   // first: it unregisters from the debugger/registrar/registry/publisher that are torn down below
    mScrubBuildFailed = false;
    if (auto* vd = mVisualDebuggerRef.Get())
    {
        if (mLayerRegistered)
            vd->GetLayerManager().Unregister(mDebugLayer.GetLayerName());
        if (mDomainRegistered && mPanelDomain != nullptr)
            vd->UnregisterDomain(*mPanelDomain);
    }
    mLayerRegistered = false;
    mDomainRegistered = false;
    if (mPanelDomain != nullptr)
    {
        if (auto* relay = mStateRelayRef.Get())
            relay->Detach(*mDomain);   // drops the adapter and stops publishing before the viewer is destroyed
        mPanelDomain = nullptr;
    }

    if (mCommandRegistered)
    {
        Dia::API::UnregisterCommand(Dia::Core::StringCRC("cellviewer.animate"));
        Dia::API::UnregisterCommandJson(Dia::Core::StringCRC("cellviewer.set_animate"));
        mCommandRegistered = false;
    }

    UnregisterInjectionCommands();
    UnregisterHistoryCommand();
    mPointerQueue.Clear();

    TeardownMirror();   // queries must not outlive the viewer or the registry
    if (mMirrorRegistrar && mDomain)
        mDomain->UnregisterCommands(*mMirrorRegistrar);   // handlers capture the domain
    mMirrorRegistrar.Reset();
    mDomain.Reset();
    mUploader.Reset();  // releases the CellField layers it holds

    if (mRegistry != nullptr)
    {
        if (mDetailsRegistered)
            mRegistry->Details().Unregister(&mBig.desc, kDetailProviderName);
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
    mDetailsRegistered = false;
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

bool CellLayersViewerTestStageModule::BuildViewerCommands()
{
    // Sim-hosted commands (OnStart, SimPU): the handlers run on this PU beside the viewer; nothing goes into the DiaAPI CommandRegistry.
    Cluiche::AppFlow::SimQueryHostModule* host = mSimQueryHostRef.Get();
    if (host == nullptr)
    {
        DIA_LOG_WARNING("CEL", "CellLayersViewerTestStage: SimQueryHostModule unavailable - viewer commands not served");
        return false;
    }
    mMirrorRegistrar = Dia::Core::MakeUnique<MirrorRegistrar>(*host);
    if (!mDomain->RegisterCommands(*mMirrorRegistrar))
    {
        DIA_LOG_ERROR("CEL", "CellLayersViewerTestStage: sim-hosted viewer command registration failed");
        return false;
    }
    return true;
}

bool CellLayersViewerTestStageModule::BuildMirror(const Dia::ValueSchema::ValueSchemaService* schema)
{
    // Same lookup as ValueSchemaModule: the server pointer comes from the "DebugServerService" ServiceStream (published by
    // Main). No RTTI (/GR-), so static_cast on the payload type fixed by the stream id.
    auto* app = dynamic_cast<Dia::ApplicationFlow::IApplicationInspectable*>(GetApplication());
    auto* store = app ? app->FindStream(Dia::Core::StringCRC("DebugServerService")) : nullptr;
    if (store == nullptr || !store->IsCommitted())
    {
        DIA_LOG_WARNING("CEL", "CellLayersViewerTestStage: DebugServerService stream unavailable - editor mirror not served");
        return false;
    }
    auto* typed = static_cast<Dia::ApplicationFlow::ServiceStreamStore<Dia::DebugServer::DebugServer*>*>(store);
    Dia::DebugServer::DebugServer* server = typed->Get();
    if (server == nullptr)
    {
        DIA_LOG_WARNING("CEL", "CellLayersViewerTestStage: no DebugServer - editor mirror not served");
        return false;
    }

    mMirrorSink = Dia::Core::MakeUnique<Dia::CellLayers::DebugServerTopicSink>(*server);
    mMirrorPublisher = Dia::Core::MakeUnique<Dia::CellLayers::ViewStatePublisher>(*mRegistry, *mDomain, *mMirrorSink, mMirrorClock);
    mMirrorQueries = Dia::Core::MakeUnique<Dia::CellLayers::MirrorQueries>(*mRegistry, *mDomain, schema);

    // Sim-hosted queries (OnStart, SimPU): handlers run on this PU beside the viewer, so they need no lock. The Main-side
    // QueryRegistry never sees these names.
    Cluiche::AppFlow::SimQueryHostModule* host = mSimQueryHostRef.Get();
    if (host == nullptr)
    {
        DIA_LOG_WARNING("CEL", "CellLayersViewerTestStage: SimQueryHostModule unavailable - mirror queries not served");
        return false;
    }
    if (!mMirrorRegistrar)
        mMirrorRegistrar = Dia::Core::MakeUnique<MirrorRegistrar>(*host);
    if (!Dia::CellLayers::RegisterMirrorQueries(*mMirrorRegistrar, *mMirrorQueries))
    {
        DIA_LOG_ERROR("CEL", "CellLayersViewerTestStage: sim-hosted mirror query registration failed");
        return false;
    }
    DIA_LOG_INFO("CEL", "CellLayersViewerTestStage: editor mirror served (topic cells.view.state + 4 sim-hosted queries)");
    return true;
}

void CellLayersViewerTestStageModule::TeardownMirror()
{
    if (mMirrorRegistrar && mMirrorQueries)
        Dia::CellLayers::UnregisterMirrorQueries(*mMirrorRegistrar);
    mMirrorQueries.Reset();
    mMirrorPublisher.Reset();
    mMirrorSink.Reset();
}

void CellLayersViewerTestStageModule::BuildCellDetails(uint32_t cellIndex, Dia::CellLayers::CellDetailBuilder& out)
{
    // Deterministic from the index so E2E can assert it: "Cell" = "q,r" (row-major, grid A width) and a rainfall-keyed value.
    char text[32];
    snprintf(text, sizeof(text), "%u,%u", cellIndex % static_cast<uint32_t>(kBigW), cellIndex / static_cast<uint32_t>(kBigW));
    out.Section("Position");
    out.Row("Cell", text);
    out.RowValue(SchemaKey(kLayerRainfall), static_cast<float>(cellIndex % 97u));
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

void CellLayersViewerTestStageModule::SubmitPointerInput()
{
    // Feeds this frame's mouse/keyboard state to the viewer's selection gestures (consumed by mDomain->Update).
    // Nothing is submitted when the input module or camera is unavailable.
    if (!mDomain || !mDomainRegistered)
        return;
    if (mCameraRef.Get() == nullptr)
        return;

    // Test-only injection (E2E): one queued frame per Sim frame replaces the real input so edge detection sees each state.
    Dia::CellLayers::SelectionInput::PointerFrame injected;
    if (mPointerQueue.TryPop(injected))
    {
        if (!mInjectLogged)
        {
            mInjectLogged = true;
            DIA_LOG_DEBUG("CEL", "CellLayersViewerTestStage: injected pointer frames are being used instead of real input");
        }
        mDomain->SubmitPointerFrame(injected, GetPickerView());
        return;
    }

    const Cluiche::AppFlow::InputStreamModule* input = mInputRef.Get();
    if (input == nullptr)
        return;

    mDomain->SubmitPointerFrame(BuildPointerFrame(*input), GetPickerView());
    if (!mPointerFrameLogged)
    {
        mPointerFrameLogged = true;
        DIA_LOG_DEBUG("CEL", "CellLayersViewerTestStage: first pointer frame submitted to the viewer");
    }
}

bool CellLayersViewerTestStageModule::RegisterInjectionCommands()
{
    // Sim-hosted (SimPU): the queue is pushed by these handlers and popped by SubmitPointerInput, both on this PU, so no lock.
    // cell_pixel reads only the viewer and the camera ModuleRef, the same state SubmitPointerInput reads on SimPU.
    if (!mMirrorRegistrar)
        return false;
    mInjectCommandsRegistered = RegisterPointerInjectionCommands(*mMirrorRegistrar, mPointerQueue, mHoverOverride,
        [this](const Json::Value& params) -> Json::Value
        {
            Json::Value out(Json::objectValue);
            if (!params.isObject() || !params["q"].isIntegral())
            {
                out["error"] = "missing or non-integer param 'q'";
                return out;
            }
            if (!params["r"].isIntegral())
            {
                out["error"] = "missing or non-integer param 'r'";
                return out;
            }
            const Dia::Graphics::CellGridDesc* grid = mDomain ? mDomain->ActiveGrid() : nullptr;
            if (grid == nullptr)
            {
                out["error"] = "no active grid";
                return out;
            }
            if (mCameraRef.Get() == nullptr)
            {
                out["error"] = "no camera";
                return out;
            }
            const Dia::CellLayers::CellPickerView v = GetPickerView();
            const float zoom = v.zoom > 0.0f ? v.zoom : 1.0f; // mirrors CellPicker::ScreenToWorld
            const Vector2D w = HexTopology::CellCenter(CellCoord{ params["q"].asInt(), params["r"].asInt() }, grid->cellSize);
            out["x"] = (w.x - v.cameraX) * zoom + v.viewportWidth * 0.5f;
            out["y"] = (w.y - v.cameraY) * zoom + v.viewportHeight * 0.5f;
            return out;
        });
    if (!mInjectCommandsRegistered)
        DIA_LOG_ERROR("CEL", "CellLayersViewerTestStage: sim-hosted injection command registration failed");
    return mInjectCommandsRegistered;
}

void CellLayersViewerTestStageModule::UnregisterInjectionCommands()
{
    if (!mInjectCommandsRegistered)
        return;
    if (mMirrorRegistrar)
        UnregisterPointerInjectionCommands(*mMirrorRegistrar);
    mInjectCommandsRegistered = false;
    mHoverOverride = HoverOverride();
}

bool CellLayersViewerTestStageModule::RegisterHistoryCommand()
{
    // Hands CellHistoryModule a registrar over the sim-hosted query service so it can register cells.history.* (it does so on its
    // next update, once the recorder exists), and adds one stage-local command that emits a marker event through the recorder.
    Cluiche::AppFlow::SimQueryHostModule* host = mSimQueryHostRef.Get();
    Dia::CellHistory::CellHistoryModule* history = mHistoryModuleRef.Get();
    if (host == nullptr || history == nullptr)
    {
        DIA_LOG_WARNING("HIS", "CellLayersViewerTestStage: SimQueryHostModule or CellHistoryModule unavailable - history commands not served");
        return false;
    }
    if (!mHistoryRegistrar)
        mHistoryRegistrar = Dia::Core::MakeUnique<MirrorRegistrar>(*host);
    history->SetRegistrar(mHistoryRegistrar.Get());

    static const Dia::API::ValueParam kEmitParams[] = {
        { "note", "string", false, "Short note stored in the event payload (default empty)" },
        { "value", "float", false, "Number stored in the event payload (default 0)" } };
    Dia::DebugServer::SimHelpInfo help;
    help.description = "Test only: emit a cluichetest.celllayers.marker event into the cell history recorder. Returns {ok, sample, result}.";
    help.category = "test"; help.owner = "CluicheTest";
    help.params = kEmitParams; help.paramCount = 2;
    mHistoryCommandRegistered = mHistoryRegistrar->RegisterCommand("cluichetest.celllayers.history_emit", help,
        [this](const Json::Value& params) -> Json::Value
        {
            Json::Value out(Json::objectValue);
            Dia::CellHistory::CellHistoryRecorder* recorder = Dia::CellHistory::FindCellHistoryRecorder(mHistoryStream);
            if (recorder == nullptr)
            {
                out["error"] = "history recorder unavailable";
                return out;
            }
            Json::Value payload(Json::objectValue);
            if (params.isObject() && params["note"].isString())
                payload["note"] = params["note"].asString();
            if (params.isObject() && params["value"].isNumeric())
                payload["value"] = params["value"].asDouble();
            const std::string payloadText = Json::FastWriter().write(payload);
            Dia::CellHistory::HistoryEvent e;
            e.type = Dia::Core::StringCRC("cluichetest.celllayers.marker");
            e.payloadJson = payloadText.c_str();
            e.text = "marker";
            const Dia::CellHistory::EmitResult r = recorder->Emit(e);
            out["ok"] = (r == Dia::CellHistory::EmitResult::Ok);
            out["result"] = static_cast<int>(r);
            out["sample"] = recorder->SampleCount();   // the event is assigned to this (next) sample
            return out;
        });
    if (!mHistoryCommandRegistered)
        DIA_LOG_ERROR("HIS", "CellLayersViewerTestStage: history_emit command registration failed");
    return mHistoryCommandRegistered;
}

void CellLayersViewerTestStageModule::UnregisterHistoryCommand()
{
    if (mHistoryCommandRegistered && mHistoryRegistrar)
        mHistoryRegistrar->Unregister("cluichetest.celllayers.history_emit");   // handler captures this
    mHistoryCommandRegistered = false;
}

bool CellLayersViewerTestStageModule::BuildHistoryScrub()
{
    using namespace Dia::CellHistory;
    CellHistoryRecorder* recorder = FindCellHistoryRecorder(mHistoryStream);
    if (recorder == nullptr || mRegistry == nullptr || !mDomain || !mHistoryRegistrar)
        return false;   // not ready yet (the recorder appears on CellHistoryModule's first update)

    CellHistoryModule* module = mHistoryModuleRef.Get();
    const HistoryConfig* config = module != nullptr ? &module->GetConfig() : nullptr;
    const Dia::ValueSchema::ValueSchemaService* schema = mSchemaStream.IsAvailable() ? &mSchemaStream.Get() : nullptr;

    mScrubRecorder = recorder;
    mScrubView = Dia::Core::MakeUnique<HistoryViewState>(*recorder);
    mScrubViewCommands = Dia::Core::MakeUnique<HistoryViewCommands>(*recorder, *mScrubView, config);
    mScrubViewCommands->SetLayerRegistry(mRegistry);
    mScrubCommandsRegistered = mScrubViewCommands->Register(*mHistoryRegistrar);
    if (!mScrubCommandsRegistered)
        DIA_LOG_ERROR("HIS", "CellLayersViewerTestStage: sim-hosted cells.history.view/live/step/event/delta registration failed");

    mScrubSource = Dia::Core::MakeUnique<HistoryValueSource>(*mRegistry, *recorder, *mScrubView);
    mScrubSource->SetSchema(schema);
    if (!mScrubSource->Install())
        DIA_LOG_ERROR("HIS", "CellLayersViewerTestStage: HistoryValueSource install failed (another view source is installed) - scrubbing shows live values");

    mScrubAdapter = Dia::Core::MakeUnique<ViewHistorySourceAdapter>(*recorder, *mScrubView);
    if (mMirrorPublisher)
        mMirrorPublisher->SetHistorySource(mScrubAdapter.Get());

    mScrubPanelCommands = Dia::Core::MakeUnique<CellHistoryCommands>(*recorder);
    mScrubDomain = Dia::Core::MakeUnique<CellHistoryDomain>(*recorder, *mScrubView, *mScrubViewCommands, *mScrubPanelCommands,
        CellHistoryDomain::BaseLayerFn(fastdelegate::MakeDelegate(this, &CellLayersViewerTestStageModule::ActiveBaseLayer)),
        schema, config);
    mScrubDomain->SetLayerRegistry(mRegistry);   // delta mode: selection-restricted group table, selected-cell detail rows
    mScrubDomain->SetHoverFn(CellHistoryDomain::HoverCellFn(fastdelegate::MakeDelegate(this, &CellLayersViewerTestStageModule::HoveredCell)));
    if (mDomain)   // delta mode: the hover tooltip also shows the A / B side values (removed in TeardownHistoryScrub)
        mDomain->AddHoverExtraFn(Dia::CellLayers::CellLayerViewerDomain::HoverExtraFn(fastdelegate::MakeDelegate(mScrubDomain.Get(), &CellHistoryDomain::HoverExtra)));

    Cluiche::AppFlow::DomainStateRelayModule* relay = mStateRelayRef.Get();
    mScrubPanelDomain = relay != nullptr ? relay->Attach(*mScrubDomain) : nullptr;
    auto* vd = mVisualDebuggerRef.Get();
    if (mScrubPanelDomain == nullptr || vd == nullptr)
    {
        DIA_LOG_ERROR("HIS", "DomainStateRelay or VisualDebugger unavailable - the history timeline card cannot appear in the panel");
        return false;
    }
    vd->RegisterDomain(*mScrubPanelDomain);
    mScrubDomainRegistered = true;
    BuildSnapshots(*recorder, schema);
    DIA_LOG_INFO("HIS", "CellLayersViewerTestStage: history scrub wired (view commands, value source, timeline domain)");
    return true;
}

void CellLayersViewerTestStageModule::BuildSnapshots(Dia::CellHistory::CellHistoryRecorder& recorder, const Dia::ValueSchema::ValueSchemaService* schema)
{
    using namespace Dia::CellHistory;
    // Folder: <exeDir>/../../../../out/CluicheTest/snapshots (same base as the capture metrics of TestStageModuleBase).
    char exePath[512] = {};
    GetModuleFileNameA(nullptr, exePath, sizeof(exePath) - 1);
    std::filesystem::path folder = std::filesystem::path(exePath).parent_path() / ".." / ".." / ".." / ".." / "out" / "CluicheTest" / "snapshots";
    folder = folder.lexically_normal();

    mSnapshotStore = Dia::Core::MakeUnique<CellLayersSnapshotStore>(folder.string(),
        [this]() { CellLayersStageState s; s.tempVersion = mTempVersion; s.animateCounter = mAnimateCounter; s.historyFrame = mHistoryFrame; return s; },
        [this](const CellLayersStageState& s) { mTempVersion = s.tempVersion; mAnimateCounter = s.animateCounter; mHistoryFrame = s.historyFrame; });
    // One cache per history setup: the delta commands, value source, domain (timeline markers, group table, hover), manager (overwrite)
    // and commands (delete) all share it. Created after the store (it reads the store's folder) and destroyed before it.
    mSnapshotCache = Dia::Core::MakeUnique<SnapshotCellCache>(*mSnapshotStore);
    mSnapshotCache->SetViewState(mScrubView.Get());
    mScrubViewCommands->SetSnapshotCache(mSnapshotCache.Get());
    mScrubSource->SetSnapshotCache(mSnapshotCache.Get());
    mScrubDomain->SetSnapshotCache(mSnapshotCache.Get());
    mSnapshotManager = Dia::Core::MakeUnique<SnapshotManager>(recorder, *mRegistry, schema, *mSnapshotStore);
    mSnapshotManager->SetSnapshotCache(mSnapshotCache.Get());
    mSnapshotManager->SetViewState(mScrubView.Get());
    mSnapshotManager->SetWorld(GetStageName().AsChar());
    mSnapshotObservation = Dia::Core::MakeUnique<SnapshotObservation>();
    mSnapshotManager->SetObservation(mSnapshotObservation.Get());
    mSnapshotCommands = Dia::Core::MakeUnique<SnapshotCommands>(recorder, *mSnapshotManager, *mSnapshotStore);
    mSnapshotCommands->SetObservation(mSnapshotObservation.Get());
    mSnapshotCommands->SetSnapshotCache(mSnapshotCache.Get());
    mSnapshotCommandsRegistered = mSnapshotCommands->Register(*mHistoryRegistrar);
    if (!mSnapshotCommandsRegistered)
        DIA_LOG_ERROR("HIS", "CellLayersViewerTestStage: sim-hosted cells.snapshot.* / cells.history.snapshot registration failed");

    // cells.history.export: relative paths resolve under <exeDir>/../../../../out/CluicheTest/history. The world id is the stage name.
    snprintf(mWorldName, sizeof(mWorldName), "%s", GetStageName().AsChar());
    recorder.SetWorldIdProvider([](void* user) -> const char* { return static_cast<const char*>(user); }, mWorldName);
    const std::filesystem::path outDir = (std::filesystem::path(exePath).parent_path() / ".." / ".." / ".." / ".." / "out").lexically_normal();
    mExportObservation = Dia::Core::MakeUnique<ExportObservation>();
    mExportCommands = Dia::Core::MakeUnique<HistoryExportCommands>(recorder);
    mExportCommands->SetAppName("CluicheTest");
    mExportCommands->SetOutDir(outDir.string().c_str());
    mExportCommands->SetSnapshots(mSnapshotManager.Get());
    mExportCommands->SetObservation(mExportObservation.Get());
    mExportCommandsRegistered = mExportCommands->Register(*mHistoryRegistrar);
    if (!mExportCommandsRegistered)
        DIA_LOG_ERROR("HIS", "CellLayersViewerTestStage: sim-hosted cells.history.export registration failed");

    // cells.history.meta/aggregates/events/cells: catch-up queries, every reply chunked under the 16 KB sim-hosted limit.
    mQueryObservation = Dia::Core::MakeUnique<QueryObservation>();
    mHistoryQueries = Dia::Core::MakeUnique<HistoryQueries>(recorder);
    mHistoryQueries->SetAppName("CluicheTest");
    mHistoryQueries->SetSnapshots(mSnapshotManager.Get());
    mHistoryQueries->SetObservation(mQueryObservation.Get());
    mHistoryQueriesRegistered = mHistoryQueries->Register(*mHistoryRegistrar);
    if (!mHistoryQueriesRegistered)
        DIA_LOG_ERROR("HIS", "CellLayersViewerTestStage: sim-hosted cells.history.meta/aggregates/events/cells registration failed");

    BuildHistoryTopics(recorder);
}

bool CellLayersViewerTestStageModule::HistoryTopicPublisher::Publish(Dia::Core::StringCRC topic, const Json::Value& payload)
{
    const unsigned int before = mServer.GetOrderedDroppedCount();
    mServer.PublishTopicOrdered(topic, payload);   // any thread; queued FIFO, delivered by the server's Main tick
    return mServer.GetOrderedDroppedCount() == before;   // a full queue drops the NEW message and counts it
}

uint32_t CellLayersViewerTestStageModule::HistoryTopicPublisher::SubscriberCount() const
{
    return mServer.GetTopicSubscriberCount(Dia::Core::StringCRC("cells.history.aggregates"))
        + mServer.GetTopicSubscriberCount(Dia::Core::StringCRC("cells.history.events"));
}

void CellLayersViewerTestStageModule::BuildHistoryTopics(Dia::CellHistory::CellHistoryRecorder& recorder)
{
    // Same server lookup as BuildMirror: the pointer is published by Main on the "DebugServerService" ServiceStream.
    auto* app = dynamic_cast<Dia::ApplicationFlow::IApplicationInspectable*>(GetApplication());
    auto* store = app ? app->FindStream(Dia::Core::StringCRC("DebugServerService")) : nullptr;
    if (store == nullptr || !store->IsCommitted())
    {
        DIA_LOG_WARNING("HIS", "CellLayersViewerTestStage: DebugServerService stream unavailable - cells.history topics not published");
        return;
    }
    Dia::DebugServer::DebugServer* server = static_cast<Dia::ApplicationFlow::ServiceStreamStore<Dia::DebugServer::DebugServer*>*>(store)->Get();
    if (server == nullptr)
    {
        DIA_LOG_WARNING("HIS", "CellLayersViewerTestStage: no DebugServer - cells.history topics not published");
        return;
    }
    mTopicPublisher = Dia::Core::MakeUnique<HistoryTopicPublisher>(*server);
    mTopicObservation = Dia::Core::MakeUnique<Dia::CellHistory::TopicObservation>();
    mHistoryTopics = Dia::Core::MakeUnique<Dia::CellHistory::HistoryTopics>(recorder, *mTopicPublisher);
    mHistoryTopics->SetObservation(mTopicObservation.Get());
}

void CellLayersViewerTestStageModule::TeardownSnapshots()
{
    // Topics first: they borrow the recorder and the publisher (which borrows the server).
    mHistoryTopics.Reset();
    mTopicObservation.Reset();
    mTopicPublisher.Reset();
    // Export commands (they borrow the manager), then the snapshot commands (their handlers capture the manager and
    // store), then manager, observation, store. The recorder may already be withdrawn, so its world provider is not touched
    // (it points at mWorldName, which lives as long as the stage).
    if (mHistoryQueries && mHistoryQueriesRegistered && mHistoryRegistrar)
        mHistoryQueries->Unregister(*mHistoryRegistrar);
    mHistoryQueriesRegistered = false;
    mHistoryQueries.Reset();
    mQueryObservation.Reset();
    if (mExportCommands && mExportCommandsRegistered && mHistoryRegistrar)
        mExportCommands->Unregister(*mHistoryRegistrar);
    mExportCommandsRegistered = false;
    mExportCommands.Reset();
    mExportObservation.Reset();
    if (mSnapshotCommands && mSnapshotCommandsRegistered && mHistoryRegistrar)
        mSnapshotCommands->Unregister(*mHistoryRegistrar);
    mSnapshotCommandsRegistered = false;
    mSnapshotCommands.Reset();
    mSnapshotManager.Reset();
    mSnapshotObservation.Reset();
    // Detach the cache from its (still alive) users before it goes, then drop it ahead of the store it reads from.
    if (mSnapshotCache)
        mSnapshotCache->SetViewState(nullptr);
    if (mScrubViewCommands)
        mScrubViewCommands->SetSnapshotCache(nullptr);
    if (mScrubSource)
        mScrubSource->SetSnapshotCache(nullptr);
    if (mScrubDomain)
        mScrubDomain->SetSnapshotCache(nullptr);
    mSnapshotCache.Reset();
    mSnapshotStore.Reset();
}

void CellLayersViewerTestStageModule::TeardownHistoryScrub()
{
    // Reverse of BuildHistoryScrub. Nothing here touches the recorder, which can already be withdrawn from its stream.
    TeardownSnapshots();
    if (mScrubDomainRegistered)
    {
        if (auto* vd = mVisualDebuggerRef.Get())
            vd->UnregisterDomain(*mScrubPanelDomain);
        mScrubDomainRegistered = false;
    }
    if (mScrubPanelDomain != nullptr)
    {
        if (auto* relay = mStateRelayRef.Get())
            relay->Detach(*mScrubDomain);
        mScrubPanelDomain = nullptr;
    }
    if (mDomain && mScrubDomain)
        mDomain->RemoveHoverExtraFn(Dia::CellLayers::CellLayerViewerDomain::HoverExtraFn(fastdelegate::MakeDelegate(mScrubDomain.Get(), &Dia::CellHistory::CellHistoryDomain::HoverExtra)));
    mScrubDomain.Reset();
    mScrubPanelCommands.Reset();
    if (mMirrorPublisher)
        mMirrorPublisher->SetHistorySource(nullptr);
    mScrubAdapter.Reset();
    if (mScrubSource)
        mScrubSource->Uninstall();
    mScrubSource.Reset();
    if (mScrubViewCommands && mScrubCommandsRegistered && mHistoryRegistrar)
        mScrubViewCommands->Unregister(*mHistoryRegistrar);   // handlers capture the view commands and the view
    mScrubCommandsRegistered = false;
    mScrubViewCommands.Reset();
    mScrubView.Reset();
    mScrubRecorder = nullptr;
}

void CellLayersViewerTestStageModule::UpdateHistoryScrub()
{
    // SimPU, once per frame. Drops the scrub objects if the recorder was replaced or withdrawn.
    Dia::CellHistory::CellHistoryRecorder* recorder = Dia::CellHistory::FindCellHistoryRecorder(mHistoryStream);
    if (mScrubRecorder != nullptr && recorder != mScrubRecorder)
        TeardownHistoryScrub();
    if (mScrubRecorder == nullptr)
    {
        if (!mScrubBuildFailed && recorder != nullptr && mLayersRegistered && !BuildHistoryScrub() && mScrubRecorder != nullptr)
        {
            mScrubBuildFailed = true;   // partially built: undo it and do not retry this stage run
            TeardownHistoryScrub();
        }
        return;
    }
    mScrubSource->Tick();
    mScrubDomain->Update();
}

void CellLayersViewerTestStageModule::SampleHistory()
{
    // Samples the live layers every kHistorySampleEveryFrames Sim frames (SimPU, the recorder's PU). The first sample happens as
    // soon as the recorder is published.
    Dia::CellHistory::CellHistoryRecorder* recorder = Dia::CellHistory::FindCellHistoryRecorder(mHistoryStream);
    if (recorder == nullptr || !mLayersRegistered)
        return;
    if (mHistoryFrame++ % kHistorySampleEveryFrames != 0)
        return;
    char label[32];
    snprintf(label, sizeof(label), "F%u", mHistoryFrame - 1u);
    Dia::CellHistory::SampleInfo info;
    info.tick = mHistoryFrame - 1u;
    info.label = label;
    info.band = 0;
    recorder->Sample(info);
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
    {
        HoverOverrideDraw hoverDraw(draw, mModule->mHoverOverride);   // test-only: lets an E2E place the hover cursor
        mModule->mDomain->Update(hoverDraw);
        // After viewer.Update, outside any viewer lock.
        if (mModule->mMirrorPublisher)
            mModule->mMirrorPublisher->Update();
        if (mModule->mMirrorQueries)
            mModule->mMirrorQueries->NotifyFrame();
    }
}

#endif // DIA_DEBUG

} // namespace CluicheTest

namespace { using CellLayersViewerTestStageModule_ = CluicheTest::CellLayersViewerTestStageModule; }
DIA_MODULE(CellLayersViewerTestStageModule_);
DIA_DESCRIBE(CellLayersViewerTestStageModule_::kTypeId, "Hosts the DiaCellLayers viewer over synthetic layers on two hex grids.");
