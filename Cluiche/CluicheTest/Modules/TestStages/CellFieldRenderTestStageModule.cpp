#include "Modules/TestStages/CellFieldRenderTestStageModule.h"

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaAutomation/AutomationService.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaObservation/Capture/DiaCapture.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaGraphics/CellField/CellFieldRegistry.h>
#include <DiaGraphics/Frame/SpriteDrawCommand.h>
#include <DiaPathfinding/HexTopology.h>

#include <windows.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace CluicheTest {

namespace
{
    using Dia::Core::CellFieldDrawRequest;
    using Dia::Core::CellFieldLayerHandle;
    using Dia::Graphics::CellFieldRegistry;
    using Dia::Graphics::CellFieldValueKind;
    using Dia::Maths::Vector2D;
    using Dia::Pathfinding::CellCoord;
    using Dia::Pathfinding::HexTopology;

    constexpr uint32_t Pack(uint32_t r, uint32_t g, uint32_t b, uint32_t a = 255u)
    {
        return (r << 24) | (g << 16) | (b << 8) | a;
    }

    // Scene A: AC9 draw order.
    constexpr float    kSizeA        = 20.0f;
    constexpr uint32_t kFieldColourA = Pack(0, 200, 0);
    constexpr uint32_t kLineColour   = Pack(255, 255, 0);
    constexpr uint32_t kSpriteColour = Pack(200, 40, 200);

    // Scene B..: 12x8 hex grid.
    constexpr int      kGridW       = 12;
    constexpr int      kGridH       = 8;
    constexpr float    kSizeB       = 24.0f;
    constexpr uint32_t kBackdrop    = Pack(20, 20, 40);
    constexpr uint32_t kCamCellDark = Pack(40, 40, 40);
    constexpr uint32_t kCamCellHot  = Pack(240, 120, 0);
    constexpr uint32_t kCircleWhite = Pack(255, 255, 255);
    constexpr int      kCamCellCol  = 5;
    constexpr int      kCamCellRow  = 4;

    struct HexStorage
    {
        std::vector<float>   centres;
        std::vector<int32_t> neighbours;
    };

    Dia::Graphics::CellGridDesc BuildHexDesc(const HexTopology& topo, float size, HexStorage& s)
    {
        const uint32_t count = topo.CellCount();
        s.centres.assign(2u * count, 0.0f);
        s.neighbours.assign(6u * count, -1);
        for (uint32_t i = 0; i < count; ++i)
        {
            const Vector2D c = HexTopology::CellCenter(topo.ToCoord(i), size);
            s.centres[2u * i]      = c.x;
            s.centres[2u * i + 1u] = c.y;
            for (uint32_t d = 0; d < 6u; ++d)
            {
                const uint32_t n = topo.NeighbourAt(i, d);
                s.neighbours[6u * i + d] = (n == Dia::Pathfinding::kNoCell) ? -1 : static_cast<int32_t>(n);
            }
        }
        Dia::Graphics::CellGridDesc desc;
        desc.topology   = Dia::Core::StringCRC("hex.flat.oddq");
        desc.width      = static_cast<uint16_t>(topo.Width());
        desc.height     = static_cast<uint16_t>(topo.Height());
        desc.cellSize   = size;
        desc.centresXY  = s.centres.data();
        desc.neighbours = s.neighbours.data();
        return desc;
    }

    // Bounding-box centre of the whole hex grid in world space.
    Vector2D GridCentre(int w, int h, float size)
    {
        const float halfH = std::sqrt(3.0f) * size * 0.5f;
        const Vector2D first = HexTopology::CellCenter(CellCoord{0, 0}, size);
        const Vector2D lastCol = HexTopology::CellCenter(CellCoord{w - 1, h - 1}, size);
        const float minX = first.x - size;
        const float maxX = lastCol.x + size;
        const float minY = first.y - halfH;
        float maxY = lastCol.y + halfH;
        const Vector2D otherCol = HexTopology::CellCenter(CellCoord{w > 1 ? 1 : 0, h - 1}, size);
        if (otherCol.y + halfH > maxY) maxY = otherCol.y + halfH;
        return Vector2D((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);
    }

    std::string ProbePath(const char* name)
    {
        char exePath[512] = {};
        GetModuleFileNameA(nullptr, exePath, sizeof(exePath) - 1);
        char* lastSlash = strrchr(exePath, '\\');
        if (!lastSlash) lastSlash = strrchr(exePath, '/');
        if (lastSlash) *(lastSlash + 1) = '\0';
        char path[768];
        snprintf(path, sizeof(path), "%s../../../../out/CluicheTest/captures/run/%s", exePath, name);
        return path;
    }

    std::string Fmt(const char* f, ...)
    {
        char buf[512];
        va_list args;
        va_start(args, f);
        vsnprintf(buf, sizeof(buf), f, args);
        va_end(args);
        return buf;
    }

    // Scene geometry shared between BuildScenes and WriteProbes.
    struct Geo
    {
        Vector2D centreA;
        Vector2D overlapProbe, spriteProbe, lineOverField, lineOverSprite;
        float    lineY = 0.0f;
        Vector2D spriteCentre;
        Vector2D centreB;
        Vector2D nwCell, seCell;
        Vector2D camCell;
    };
    Geo gGeo; // single stage instance on SimPU
}

const Dia::Core::StringCRC CellFieldRenderTestStageModule::kTypeId("CellFieldRenderTestStageModule");

CellFieldRenderTestStageModule::CellFieldRenderTestStageModule(const Dia::Core::StringCRC& instanceId)
    : TestStageModuleBase(instanceId)
{}

CellFieldRenderTestStageModule::~CellFieldRenderTestStageModule() = default;

Dia::Core::StringCRC CellFieldRenderTestStageModule::GetStageName() const
{
    return Dia::Core::StringCRC("CellFieldRenderTestStage");
}

const Dia::Core::StringCRC* CellFieldRenderTestStageModule::GetCheckpointNames(unsigned int& outCount) const
{
    static const Dia::Core::StringCRC names[] = {
        Dia::Core::StringCRC("test.cell_field_render.passed")
    };
    outCount = 1;
    return names;
}

bool CellFieldRenderTestStageModule::AreDependenciesReady()
{
    return mRegistryStream.IsAvailable();
}

void CellFieldRenderTestStageModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    TestStageModuleBase::OnConnectStreams(app);
    mRegistryStream.Connect(app);
    mTextureStream.Connect(app);
    mSceneOut.Connect(app);
    mFence.Connect(app);
}

void CellFieldRenderTestStageModule::Fail(const char* why)
{
    DIA_LOG_ERROR("CluicheTest", "CellFieldRenderTestStage: %s", why);
    mDone = true;
    ReportFailed();
}

void CellFieldRenderTestStageModule::BuildScenes()
{
    CellFieldRegistry& reg = mRegistryStream.Get();

    // ----- Palettes -----
    const uint32_t palA[]   = { kFieldColourA };
    const uint32_t pal6[]   = { Pack(200, 80, 80), Pack(80, 200, 80), Pack(80, 80, 200),
                                Pack(200, 200, 80), Pack(80, 200, 200), Pack(200, 80, 200) };
    const uint32_t palCam[] = { kCamCellDark, kCamCellHot };
    const Dia::Core::StringCRC palAId("cfr.palA"), pal6Id("cfr.pal6"), palCamId("cfr.palCam");
    reg.RegisterPalette(palAId, palA, 1, palA[0]);
    reg.RegisterPalette(pal6Id, pal6, 6, pal6[0]);
    reg.RegisterPalette(palCamId, palCam, 2, palCam[0]);

    // ----- Scene A layer: 4x4 hex, one colour -----
    {
        HexTopology topo(4, 4);
        HexStorage st;
        auto desc = BuildHexDesc(topo, kSizeA, st);
        if (!reg.CreateLayer(desc, CellFieldValueKind::Category, mLayerA).ok) { Fail("CreateLayer A failed"); return; }
        std::vector<float> v(topo.CellCount(), 0.0f);
        reg.Update(mLayerA, v.data(), static_cast<uint32_t>(v.size()));
    }

    // ----- Grid B layers: 12x8 hex -----
    HexTopology topoB(kGridW, kGridH);
    HexStorage stB;
    const auto descB = BuildHexDesc(topoB, kSizeB, stB);
    const uint32_t countB = topoB.CellCount();
    const Vector2D centreB = GridCentre(kGridW, kGridH, kSizeB);

    std::vector<float> pattern(countB), uniform(countB, 0.0f), camVals(countB, 0.0f), hill(countB), flat(countB, 10.0f);
    const float sigma = 100.0f;
    const float peak  = 80.0f;
    for (uint32_t i = 0; i < countB; ++i)
    {
        const CellCoord c = topoB.ToCoord(i);
        pattern[i] = static_cast<float>((c.x + 2 * c.y) % 6);
        const Vector2D p = HexTopology::CellCenter(c, kSizeB);
        const float dx = p.x - centreB.x, dy = p.y - centreB.y;
        hill[i] = peak * std::exp(-(dx * dx + dy * dy) / (2.0f * sigma * sigma));
    }
    camVals[topoB.ToIndex(CellCoord{kCamCellCol, kCamCellRow})] = 1.0f;

    auto makeLayer = [&](CellFieldValueKind kind, const std::vector<float>& vals, CellFieldLayerHandle& out) -> bool
    {
        if (!reg.CreateLayer(descB, kind, out).ok) return false;
        return reg.Update(out, vals.data(), countB).ok;
    };
    if (!makeLayer(CellFieldValueKind::Category, pattern, mLayerPattern) ||
        !makeLayer(CellFieldValueKind::Category, uniform, mLayerUniform) ||
        !makeLayer(CellFieldValueKind::Category, camVals, mLayerCam) ||
        !makeLayer(CellFieldValueKind::Float, hill, mElevHill) ||
        !makeLayer(CellFieldValueKind::Float, flat, mElevFlat))
    {
        Fail("CreateLayer for grid B failed");
        return;
    }

    // ----- Geometry bookkeeping -----
    gGeo.centreA = GridCentre(4, 4, kSizeA);
    gGeo.lineY = gGeo.centreA.y + 30.0f;
    gGeo.spriteCentre = Vector2D(gGeo.centreA.x + 80.0f, gGeo.centreA.y);
    gGeo.overlapProbe = HexTopology::CellCenter(CellCoord{1, 1}, kSizeA);
    gGeo.spriteProbe = Vector2D(gGeo.centreA.x + 150.0f, gGeo.centreA.y - 60.0f);
    gGeo.lineOverField = Vector2D(5.0f, gGeo.lineY);
    gGeo.lineOverSprite = Vector2D(gGeo.centreA.x + 150.0f, gGeo.lineY);
    gGeo.centreB = centreB;
    gGeo.nwCell = HexTopology::CellCenter(CellCoord{2, 2}, kSizeB);
    gGeo.seCell = HexTopology::CellCenter(CellCoord{9, 5}, kSizeB);
    gGeo.camCell = HexTopology::CellCenter(CellCoord{kCamCellCol, kCamCellRow}, kSizeB);

    // ----- Scenes -----
    mSceneCount = 0;
    auto add = [&](const char* tag, float cx, float cy, float zoom) -> Scene&
    {
        Scene& s = mScenes[mSceneCount++];
        s = Scene();
        s.tag = tag;
        s.camX = cx;
        s.camY = cy;
        s.zoom = zoom;
        return s;
    };

    {   // 0: draw order
        Scene& s = add("CellFieldRender_draworder", gGeo.centreA.x + 40.0f, gGeo.centreA.y, 1.0f);
        s.drawField = true;
        s.request.layer = mLayerA;
        s.request.colourMap = palAId.Value();
        s.drawSprite = true;
        s.spritePos = gGeo.spriteCentre;
        s.spriteScale = 8.0f;
        s.drawLine = true;
        s.lineA = Vector2D(gGeo.centreA.x - 100.0f, gGeo.lineY);
        s.lineB = Vector2D(gGeo.centreA.x + 250.0f, gGeo.lineY);
    }

    auto fieldB = [&](Scene& s, CellFieldLayerHandle layer, uint32_t palette)
    {
        s.drawField = true;
        s.request.layer = layer;
        s.request.colourMap = palette;
        s.request.inset = 0.1f;
        s.request.backdropColour = kBackdrop;
    };
    auto shade = [&](Scene& s, CellFieldLayerHandle elev, float strength, float azimuth)
    {
        s.request.elevationLayer = elev;
        s.request.hillshadeStrength = strength;
        s.request.lightAzimuthDeg = azimuth;
        s.request.lightElevationDeg = 45.0f;
        s.request.elevationScale = 1.0f;
    };

    {   // 1: golden
        Scene& s = add("CellFieldRender_golden", centreB.x, centreB.y, 1.0f);
        fieldB(s, mLayerPattern, pal6Id.Value());
        shade(s, mElevHill, 0.8f, 315.0f);
    }
    {   // 2: same layers, light from the south-east: no value upload expected
        Scene& s = add("CellFieldRender_light_se", centreB.x, centreB.y, 1.0f);
        fieldB(s, mLayerPattern, pal6Id.Value());
        shade(s, mElevHill, 0.8f, 135.0f);
    }
    {   // 3: constant elevation, uniform colour, no inset => uniform shade
        Scene& s = add("CellFieldRender_flat_shaded", centreB.x, centreB.y, 1.0f);
        fieldB(s, mLayerUniform, pal6Id.Value());
        s.request.inset = 0.0f;
        s.request.backdropColour = 0;
        shade(s, mElevFlat, 0.8f, 315.0f);
    }
    {   // 4: hillshade strength 0
        Scene& s = add("CellFieldRender_noshade_s0", centreB.x, centreB.y, 1.0f);
        fieldB(s, mLayerPattern, pal6Id.Value());
        shade(s, mElevHill, 0.0f, 315.0f);
    }
    {   // 5: no elevation layer at all
        Scene& s = add("CellFieldRender_noshade_noelev", centreB.x, centreB.y, 1.0f);
        fieldB(s, mLayerPattern, pal6Id.Value());
    }
    const float camPos[3][3] = {
        { centreB.x,          centreB.y,          1.0f  },
        { centreB.x + 60.0f,  centreB.y - 40.0f,  1.5f  },
        { centreB.x - 100.0f, centreB.y + 50.0f,  0.75f },
    };
    const char* camTags[3] = { "CellFieldRender_cam0", "CellFieldRender_cam1", "CellFieldRender_cam2" };
    for (int i = 0; i < 3; ++i)
    {
        Scene& s = add(camTags[i], camPos[i][0], camPos[i][1], camPos[i][2]);
        fieldB(s, mLayerCam, palCamId.Value());
        s.request.backdropColour = 0;
        s.drawCircle = true;
        s.circlePos = gGeo.camCell;
        s.circleRadius = 6.0f;
    }

    // Stale captures from a previous run would satisfy the "file written" wait.
    for (unsigned int i = 0; i < mSceneCount; ++i)
    {
        std::error_code ec;
        std::filesystem::remove(ProbePath((std::string(mScenes[i].tag) + ".png").c_str()), ec);
    }

    mBuilt = true;
}

void CellFieldRenderTestStageModule::ApplyScene(const Scene& scene)
{
#ifdef DIA_DEBUG
    if (auto* cam = mCameraRef.Get())
    {
        auto& c = cam->GetActiveCamera();
        if (!mCameraSaved)
        {
            mSavedCamX = c.GetPosition().x;
            mSavedCamY = c.GetPosition().y;
            mSavedZoom = c.GetZoom();
            mCameraSaved = true;
        }
        c.SetPosition(Vector2D(scene.camX, scene.camY));
        c.SetZoom(scene.zoom);
    }
#else
    (void)scene;
#endif
}

void CellFieldRenderTestStageModule::OnStart(Dia::Automation::AutomationService* service)
{
    mBuilt = false;
    mDone = false;
    mActiveScene = -1;
    mPhase = Phase::Settle;
    mPhaseFrames = 0;
    mPhaseFence = 0;
    mSceneCount = 0;
    mCameraSaved = false;
    mAllCaptured = false;
    mWrittenFrames = 0;
    mRetries = 0;

#ifdef DIA_DEBUG
    if (auto* vd = mVisualDebuggerRef.Get())
    {
        vd->GetLayerManager().Register(&mDebugLayer, 10, Dia::Core::StringCRC("CellFieldRenderTestStage"));
        mLayerRegistered = true;
    }
    else
    {
        DIA_LOG_WARNING("CluicheTest", "CellFieldRenderTestStage: VisualDebuggerModule unavailable - no field will be drawn");
    }
#endif

    service->RegisterCheckpoint(this, Dia::Core::StringCRC("test.cell_field_render.passed"),
        [this]() -> Dia::Automation::CheckpointResult {
            if (mAllCaptured)
                return { true, "all scenes captured", 1.0f };
            return { false, "pending", 0.0f };
        });
}

void CellFieldRenderTestStageModule::OnUpdate(float /*deltaTime*/)
{
    // Sprites travel on SimScene; the debug frame (field, line, circle) comes from the debug layer.
    mSceneFrame.Clear();

    if (!mBuilt && !mDone)
    {
        if (!mRegistryStream.IsAvailable())
            return;
        BuildScenes();
    }

    if (mBuilt && !mDone)
    {
        const Dia::Graphics::RenderFence* fence = mFence.FetchLatest();
        const uint64_t fenceNow = fence ? fence->presentedFrame : 0;

        // Sprite texture must be ready before the first scene starts.
        Dia::Graphics::ITexture* tex = nullptr;
        if (mTextureStream.IsAvailable())
            tex = mTextureStream.Get().LookupTexture(Dia::Core::StringCRC("texture.cfr_sprite"));
        const bool texReady = (tex != nullptr && tex->IsReady());

        if (mActiveScene < 0)
        {
            if (texReady && fence != nullptr)
            {
                mActiveScene = 0;
                mPhase = Phase::Settle;
                mPhaseFrames = 0;
            }
        }

        if (mActiveScene >= 0)
        {
            Scene& scene = mScenes[mActiveScene];

            if (mPhase == Phase::Settle)
            {
                if (mPhaseFrames == 0)
                {
                    ApplyScene(scene);
                    mPhaseFence = fenceNow;
                }
                ++mPhaseFrames;
                if (mPhaseFrames >= 12 && fenceNow >= mPhaseFence + 6)
                {
                    scene.uploadsAtCapture = mRegistryStream.Get().GetStats().uploads;
                    DIA_CAPTURE(scene.tag, "cellfield");
                    DIA_LOG_INFO("CluicheTest", "CellFieldRenderTestStage: captured '%s' (presented frame %llu)",
                        scene.tag, static_cast<unsigned long long>(fenceNow));
                    mPhase = Phase::WaitCapture;
                    mPhaseFence = fenceNow;
                    mPhaseFrames = 0;
                }
            }
            else if (mPhase == Phase::WaitCapture)
            {
                // Frame readback and PNG encoding are asynchronous and the canvas ring buffer is
                // small (requests are dropped while it is full), so wait for the capture file itself.
                ++mPhaseFrames;
                const std::string file = ProbePath((std::string(scene.tag) + ".png").c_str());
                std::error_code ec;
                const bool written = std::filesystem::exists(file, ec) && std::filesystem::file_size(file, ec) > 0;
                if (written)
                {
                    if (++mWrittenFrames >= 3)
                    {
                        mWrittenFrames = 0;
                        mRetries = 0;
                        if (static_cast<unsigned int>(mActiveScene) + 1 < mSceneCount)
                        {
                            ++mActiveScene;
                            mPhase = Phase::Settle;
                            mPhaseFrames = 0;
                        }
                        else
                        {
                            WriteProbes();
                            mPhase = Phase::Linger;
                            mPhaseFrames = 0;
                            mAllCaptured = true;
                        }
                    }
                }
                else if (mPhaseFrames >= 150)
                {
                    if (++mRetries > 3)
                    {
                        Fail("capture was never written");
                    }
                    else
                    {
                        DIA_LOG_WARNING("CluicheTest", "CellFieldRenderTestStage: re-requesting capture '%s'", scene.tag);
                        DIA_CAPTURE(scene.tag, "cellfield-retry");
                        mPhaseFrames = 0;
                    }
                }
            }
            else if (mPhase == Phase::Linger)
            {
                // Keep the stage (and its checkpoint) alive long enough for the E2E poll to see it.
                if (++mPhaseFrames >= 120)
                {
                    mPhase = Phase::Done;
                    mDone = true;
                    ReportPassed();
                }
            }
        }

        if (mActiveScene >= 0 && mScenes[mActiveScene].drawSprite && texReady)
        {
            const Scene& s = mScenes[mActiveScene];
            Dia::Graphics::SpriteDrawCommand cmd(tex, s.spritePos);
            cmd.scale = Vector2D(s.spriteScale, s.spriteScale);
            mSceneFrame.RequestDrawSprite(cmd);
        }
    }

    mSceneOut.Write(mSceneFrame, Dia::Core::TimeAbsolute::Zero());
}

void CellFieldRenderTestStageModule::OnStop()
{
#ifdef DIA_DEBUG
    if (mLayerRegistered)
    {
        if (auto* vd = mVisualDebuggerRef.Get())
            vd->GetLayerManager().Unregister(mDebugLayer.GetLayerName());
        mLayerRegistered = false;
    }
    if (mCameraSaved)
    {
        if (auto* cam = mCameraRef.Get())
        {
            cam->GetActiveCamera().SetPosition(Vector2D(mSavedCamX, mSavedCamY));
            cam->GetActiveCamera().SetZoom(mSavedZoom);
        }
        mCameraSaved = false;
    }
#endif

    // Flush an empty scene frame so the sprite pointer does not outlive the stage.
    mSceneFrame.Clear();
    mSceneOut.Write(mSceneFrame, Dia::Core::TimeAbsolute::Zero());
    mActiveScene = -1;

    if (mBuilt && mRegistryStream.IsAvailable())
    {
        CellFieldRegistry& reg = mRegistryStream.Get();
        reg.Destroy(mLayerA);
        reg.Destroy(mLayerPattern);
        reg.Destroy(mLayerUniform);
        reg.Destroy(mLayerCam);
        reg.Destroy(mElevHill);
        reg.Destroy(mElevFlat);
    }
    mBuilt = false;
}

#ifdef DIA_DEBUG
void CellFieldRenderTestStageModule::CellFieldDebugLayer::Draw(Dia::Core::IDebugDraw& draw)
{
    if (mModule->mActiveScene < 0 || !mModule->mBuilt)
        return;
    const Scene& s = mModule->mScenes[mModule->mActiveScene];

    if (s.drawField)
        draw.RequestDrawCellField(s.request);
    if (s.drawLine)
        draw.RequestDraw(s.lineA, s.lineB, Dia::Core::RGBA(255, 255, 0, 255));
    if (s.drawCircle)
        draw.RequestDraw(s.circlePos, s.circleRadius, Dia::Core::RGBA(255, 255, 255, 255), Dia::Core::RGBA(255, 255, 255, 255));
}
#endif

void CellFieldRenderTestStageModule::WriteProbes()
{
    std::string j = "{\n";
    j += Fmt("  \"hex_size_a\": %.4f,\n", kSizeA);
    j += Fmt("  \"hex_size_b\": %.4f,\n", kSizeB);
    j += Fmt("  \"grid_b\": [%d, %d],\n", kGridW, kGridH);
    j += "  \"scenes\": [\n";
    for (unsigned int i = 0; i < mSceneCount; ++i)
    {
        const Scene& s = mScenes[i];
        j += Fmt("    {\"tag\": \"%s\", \"cam\": [%.4f, %.4f], \"zoom\": %.4f, \"uploads\": %llu}%s\n",
            s.tag, s.camX, s.camY, s.zoom, static_cast<unsigned long long>(s.uploadsAtCapture),
            (i + 1 < mSceneCount) ? "," : "");
    }
    j += "  ],\n";
    j += Fmt("  \"draworder\": {\"overlap\": [%.4f, %.4f], \"field_rgba\": [0, 200, 0, 255], "
             "\"sprite_only\": [%.4f, %.4f], \"sprite_rgba\": [200, 40, 200, 255], "
             "\"line_over_field\": [%.4f, %.4f], \"line_over_sprite\": [%.4f, %.4f], \"line_rgba\": [255, 255, 0, 255]},\n",
        gGeo.overlapProbe.x, gGeo.overlapProbe.y, gGeo.spriteProbe.x, gGeo.spriteProbe.y,
        gGeo.lineOverField.x, gGeo.lineOverField.y, gGeo.lineOverSprite.x, gGeo.lineOverSprite.y);
    j += Fmt("  \"hill\": {\"centre\": [%.4f, %.4f], \"nw_cell\": [%.4f, %.4f], \"se_cell\": [%.4f, %.4f]},\n",
        gGeo.centreB.x, gGeo.centreB.y, gGeo.nwCell.x, gGeo.nwCell.y, gGeo.seCell.x, gGeo.seCell.y);
    j += "  \"palette6\": [[200,80,80],[80,200,80],[80,80,200],[200,200,80],[80,200,200],[200,80,200]],\n";
    j += Fmt("  \"cam\": {\"cell_world\": [%.4f, %.4f], \"cell_rgba\": [240, 120, 0, 255], \"circle_rgba\": [255, 255, 255, 255], \"circle_radius\": 6.0}\n",
        gGeo.camCell.x, gGeo.camCell.y);
    j += "}\n";

    const std::string path = ProbePath("CellFieldRender_probes.json");
    std::filesystem::create_directories(std::filesystem::path(path).parent_path());
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "wb") == 0 && f)
    {
        fwrite(j.data(), 1, j.size(), f);
        fclose(f);
        DIA_LOG_INFO("CluicheTest", "CellFieldRenderTestStage: wrote probes to %s", path.c_str());
    }
    else
    {
        DIA_LOG_ERROR("CluicheTest", "CellFieldRenderTestStage: could not write probes to %s", path.c_str());
    }
}

} // namespace CluicheTest

namespace { using CellFieldRenderTestStageModule_ = CluicheTest::CellFieldRenderTestStageModule; }
DIA_MODULE(CellFieldRenderTestStageModule_);
DIA_DESCRIBE(CellFieldRenderTestStageModule_::kTypeId, "Renders CellField layers for real and captures frames for pixel verification.");
