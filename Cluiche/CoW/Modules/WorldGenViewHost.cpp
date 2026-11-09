#ifdef DIA_DEBUG

#include "Modules/WorldGenViewHost.h"
#include "Modules/WorldGenPointerFrame.h"

#include "Modules/Camera2DModule.h"
#include "Modules/InputStreamModule.h"

#include <DiaCellLayers/CellPicker.h>
#include <DiaCellLayers/SelectionTools.h>
#include <DiaInput/EKey.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaValueSchema/ValueSchemaService.h>

#include <cstdio>
#include <cstring>

namespace CoW {

using Dia::CellLayers::CellLayerDesc;
using Dia::CellLayers::CellOverlayDesc;
using Dia::CellLayers::CellRegisterResult;
using Dia::CellLayers::CellValueKind;
using Dia::Core::StringCRC;
using Dia::Maths::Vector2D;
using namespace WorldGeneration;

namespace
{
const char* const kDetailProviderName = "Terrain";
const float       kFrameDt            = 1.0f / 60.0f;
const float       kLabelFontSize      = 12.0f;

// Result sink for the flush update that releases the uploader's CellField layers (nothing is registered, so nothing is drawn).
struct NullDebugDraw : public Dia::Core::IDebugDraw
{
    Vector2D mouse;
    void RequestDraw(const Vector2D&, float, Dia::Core::RGBA, Dia::Core::RGBA) override {}
    void RequestDraw(const Vector2D&, const Vector2D&, Dia::Core::RGBA) override {}
    void RequestDrawPoint(const Vector2D&, Dia::Core::RGBA) override {}
    void RequestDrawRect(const Vector2D&, const Vector2D&, Dia::Core::RGBA, Dia::Core::RGBA) override {}
    void RequestDrawArc(const Vector2D&, float, float, float, Dia::Core::RGBA) override {}
    void RequestDrawRay(const Vector2D&, const Vector2D&, float, Dia::Core::RGBA) override {}
    void RequestDraw(const Vector2D&, const Vector2D&, const Vector2D&, Dia::Core::RGBA, Dia::Core::RGBA) override {}
    void RequestDrawText(const Vector2D&, const char*, float, Dia::Core::RGBA) override {}
    void RequestDrawLine3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, Dia::Core::RGBA) override {}
    void RequestDrawRay3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, float, Dia::Core::RGBA) override {}
    void RequestDrawBox3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, Dia::Core::RGBA) override {}
    void RequestDrawSphere3D(const Dia::Maths::Vector3D&, float, Dia::Core::RGBA) override {}
    void RequestDrawArrow3D(const Dia::Maths::Vector3D&, const Dia::Maths::Vector3D&, float, float, Dia::Core::RGBA) override {}
    void RequestDrawCellField(const Dia::Core::CellFieldDrawRequest&) override {}
    uint32_t DroppedCount() const override { return 0; }
    const Vector2D& GetMousePixel() const override { return mouse; }
};

// 0xRRGGBBAA -> "#rrggbb".
std::string Rgb(uint32_t rgba8)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02x%02x%02x", (rgba8 >> 24) & 0xFFu, (rgba8 >> 16) & 0xFFu, (rgba8 >> 8) & 0xFFu);
    return buf;
}

// The generated world's dimensions once one exists (SetSize only changes the session's pending dimensions until the next Tick).
GridDimensions TargetDims(const WorldGenSession& s)
{
    return s.Revision() > 0 ? s.World().dimensions : s.Dimensions();
}

bool SameDims(GridDimensions a, GridDimensions b) { return a.width == b.width && a.height == b.height; }
} // namespace

const char* const WorldGenViewHost::kOverlayRivers     = "rivers";
const char* const WorldGenViewHost::kOverlayNeighbours = "selection.neighbours";
const char* const WorldGenViewHost::kDefaultModeAlias  = "terrain";

void WorldGenViewHost::LayerBinding::Read(float* out, uint32_t count)
{
    const WorldGenSession& s = host->mSession;
    if (static_cast<uint32_t>(s.World().CellCount()) != count)
    {
        for (uint32_t i = 0; i < count; ++i) out[i] = 0.0f;   // grid and world disagree for a frame: show nothing, never overrun
        return;
    }
    desc->read(s, out, count);
}

uint64_t WorldGenViewHost::LayerBinding::Version()
{
    return WorldLayerSource::Version(host->mSession);
}

WorldGenViewHost::WorldGenViewHost(WorldGenSession& session, const Deps& deps)
    : mSession(session)
    , mDeps(deps)
{
    RefreshPorts();
    mGrid.emplace(TargetDims(mSession));
    mBindings.resize(WorldLayerSource::Count());
    for (uint32_t i = 0; i < WorldLayerSource::Count(); ++i)
    {
        mBindings[i].host = this;
        mBindings[i].desc = &WorldLayerSource::At(i);
    }
}

WorldGenViewHost::~WorldGenViewHost()
{
    Teardown();
}

void WorldGenViewHost::RefreshPorts()
{
    using Cluiche::AppFlow::Camera2DModule;
    using Cluiche::AppFlow::InputStreamModule;
    if (Camera2DModule* cam = mDeps.camera)
    {
        if (!mDeps.windowSize)
            mDeps.windowSize = [cam]() { return cam->GetWindowSize(); };
        if (!mDeps.getCamera)
            mDeps.getCamera = [cam](float& x, float& y, float& z)
            {
                x = cam->GetActiveCamera().GetPosition().x;
                y = cam->GetActiveCamera().GetPosition().y;
                z = cam->GetActiveCamera().GetZoom();
            };
        if (!mDeps.setCamera)
            mDeps.setCamera = [cam](float x, float y, float z)
            {
                cam->GetActiveCamera().SetPosition(Vector2D(x, y));
                cam->GetActiveCamera().SetZoom(z);
            };
    }
    if (InputStreamModule* in = mDeps.input)
    {
        if (!mDeps.readInput)
            mDeps.readInput = [in](InputSnapshot& snap)
            {
                using Dia::Input::EKey;
                snap.pointer = BuildWorldGenPointerFrame(*in);
                WorldGeneration::CameraInput& c = snap.camera;
                c.keyW = in->IsKeyDown(EKey::W);
                c.keyA = in->IsKeyDown(EKey::A);
                c.keyS = in->IsKeyDown(EKey::S);
                c.keyD = in->IsKeyDown(EKey::D);
                c.keyUp = in->IsKeyDown(EKey::Up);
                c.keyDown = in->IsKeyDown(EKey::Down);
                c.keyLeft = in->IsKeyDown(EKey::Left);
                c.keyRight = in->IsKeyDown(EKey::Right);
                c.keyPlus = in->IsKeyDown(EKey::Add) || in->IsKeyDown(EKey::Equal);
                c.keyMinus = in->IsKeyDown(EKey::Subtract) || in->IsKeyDown(EKey::Dash);
                c.keyQ = in->IsKeyDown(EKey::Q);
                c.keyE = in->IsKeyDown(EKey::E);
                c.keyHome = in->WasKeyPressed(EKey::Home);
                c.wheelDelta = in->GetMouseWheelDelta();
                return true;
            };
    }
}

const char* WorldGenViewHost::DefaultBaseId() const
{
    for (uint32_t i = 0; i < WorldLayerSource::Count(); ++i)
        if (WorldLayerSource::At(i).defaultBase)
            return WorldLayerSource::At(i).id;
    return WorldLayerSource::At(0).id;
}

bool WorldGenViewHost::RegisterLayers()
{
    if (mDeps.registry == nullptr)
        return false;
    for (uint32_t i = 0; i < WorldLayerSource::Count(); ++i)
    {
        const WorldLayerDesc& wd = WorldLayerSource::At(i);
        CellLayerDesc d;
        d.id = StringCRC(wd.id);
        d.schemaKey = StringCRC(wd.schemaKey);
        d.group.Append(wd.group);
        d.kind = wd.kind == WorldLayerKind::Category ? CellValueKind::Category : CellValueKind::Float;
        d.grid = &mGrid->Desc();
        d.read = fastdelegate::MakeDelegate(&mBindings[i], &LayerBinding::Read);
        d.version = fastdelegate::MakeDelegate(&mBindings[i], &LayerBinding::Version);
        const CellRegisterResult r = mDeps.registry->Register(d);
        if (r != CellRegisterResult::Ok)
        {
            DIA_LOG_ERROR("CoW", "WorldGenViewHost: registering layer '%s' failed (%d)", wd.id, static_cast<int>(r));
            UnregisterLayers();
            return false;
        }
    }
    mLayersRegistered = true;
    if (mDeps.registry->Details().Register(&mGrid->Desc(), kDetailProviderName,
            fastdelegate::MakeDelegate(this, &WorldGenViewHost::BuildCellDetails)) != Dia::CellLayers::CellDetailRegisterResult::Ok)
    {
        DIA_LOG_ERROR("CoW", "WorldGenViewHost: registering the cell detail provider failed");
        UnregisterLayers();
        return false;
    }
    mDetailsRegistered = true;
    return true;
}

void WorldGenViewHost::UnregisterLayers()
{
    if (mDeps.registry == nullptr)
        return;
    if (mDetailsRegistered && mGrid)
        mDeps.registry->Details().Unregister(&mGrid->Desc(), kDetailProviderName);
    mDetailsRegistered = false;
    // Unregister is a no-op for ids that were never registered (partial failure path).
    for (uint32_t i = 0; i < WorldLayerSource::Count(); ++i)
        mDeps.registry->Unregister(StringCRC(WorldLayerSource::At(i).id));
    mLayersRegistered = false;
}

bool WorldGenViewHost::RegisterOverlays()
{
    if (mDeps.registry == nullptr)
        return false;
    CellOverlayDesc rivers;
    rivers.id = StringCRC(kOverlayRivers);
    rivers.group.Append("World");
    rivers.label.Append("Rivers");
    rivers.draw = fastdelegate::MakeDelegate(this, &WorldGenViewHost::DrawRivers);
    CellOverlayDesc neighbours;
    neighbours.id = StringCRC(kOverlayNeighbours);
    neighbours.group.Append("Selection");
    neighbours.label.Append("Neighbours 0-5");
    neighbours.draw = fastdelegate::MakeDelegate(this, &WorldGenViewHost::DrawNeighbours);
    if (mDeps.registry->RegisterOverlay(rivers) != CellRegisterResult::Ok ||
        mDeps.registry->RegisterOverlay(neighbours) != CellRegisterResult::Ok)
    {
        DIA_LOG_ERROR("CoW", "WorldGenViewHost: registering overlays failed");
        UnregisterOverlays();
        return false;
    }
    mOverlaysRegistered = true;
    return true;
}

void WorldGenViewHost::UnregisterOverlays()
{
    if (mDeps.registry == nullptr)
        return;
    mDeps.registry->UnregisterOverlay(StringCRC(kOverlayRivers));
    mDeps.registry->UnregisterOverlay(StringCRC(kOverlayNeighbours));
    mOverlaysRegistered = false;
}

bool WorldGenViewHost::Build()
{
    if (mDomain)
        return true;
    if (mDeps.registry == nullptr || mDeps.cellField == nullptr)
    {
        DIA_LOG_ERROR("CoW", "WorldGenViewHost: CellLayerRegistry or CellFieldRegistry unavailable");
        return false;
    }
    if (!RegisterLayers() || !RegisterOverlays())
    {
        UnregisterLayers();
        return false;
    }

    mUploader = Dia::Core::MakeUnique<Dia::CellLayers::LayerUploader>(*mDeps.cellField, mDeps.schema);
    mDomain = Dia::Core::MakeUnique<Dia::CellLayers::CellLayerViewerDomain>(*mDeps.registry, *mUploader, mDeps.schema,
        Dia::CellLayers::CellLayerViewerDomain::ViewFn(fastdelegate::MakeDelegate(this, &WorldGenViewHost::PickerView)));
    mDomain->ToggleOverlay(StringCRC(kOverlayRivers));       // both overlays start on
    mDomain->ToggleOverlay(StringCRC(kOverlayNeighbours));

    if (mDeps.registrar != nullptr)
    {
        mCommandsRegistered = mDomain->RegisterCommands(*mDeps.registrar);
        if (!mCommandsRegistered)
            DIA_LOG_ERROR("CoW", "WorldGenViewHost: sim-hosted viewer command registration failed");
    }
    else
    {
        DIA_LOG_WARNING("CoW", "WorldGenViewHost: no command registrar - cells.* commands not served");
    }

    mNeedFit = true;
    mUploadedRevision = 0;
    mSeenUploads = 0;
    mUploadFrames = 0;
    DIA_LOG_INFO("CoW", "WorldGenViewHost: viewer hosted (%u layers, %u overlays, %u grids)",
        mDeps.registry->LayerCount(), mDeps.registry->OverlayCount(), mDeps.registry->GridCount());
    return true;
}

void WorldGenViewHost::Teardown()
{
    if (mDomain && mCommandsRegistered && mDeps.registrar != nullptr)
        mDomain->UnregisterCommands(*mDeps.registrar);   // handlers capture the domain
    mCommandsRegistered = false;

    if (mDeps.registry != nullptr)
    {
        if (mOverlaysRegistered)
            UnregisterOverlays();
        if (mLayersRegistered || mDetailsRegistered)
            UnregisterLayers();
    }

    mDomain.Reset();
    mUploader.Reset();   // releases the CellField layers it holds

    if (mCameraSaved && mDeps.setCamera)
        mDeps.setCamera(mSavedX, mSavedY, mSavedZoom);
    mCameraSaved = false;
}

void WorldGenViewHost::OnSizeChanged()
{
    const GridDimensions target = TargetDims(mSession);
    if (SameDims(target, mGrid->Dimensions()))
        return;

    int selX = 0, selY = 0;
    const bool hadSelection = mDomain && GetSelection(selX, selY);
    const StringCRC base = mDomain ? mDomain->ActiveBase() : StringCRC::kZero;

    if (mDomain)
    {
        // Unregister, then run one empty-state viewer update so the uploader drops the CellField layers sized for the old grid.
        UnregisterLayers();
        NullDebugDraw sink;
        mDomain->Update(sink);
    }
    mGrid.emplace(target);   // in place: same desc address, so the registry's selection entry is reused (and reset by its size check)
    ++mGridGeneration;
    mNeedFit = true;
    mRiverRevision = 0xFFFFFFFFu;
    mOutlineCell = -1;
    mDetailCell = -1;
    mLegendValid = false;

    if (mDomain)
    {
        if (!RegisterLayers())
        {
            DIA_LOG_ERROR("CoW", "WorldGenViewHost: re-registering layers after a size change failed");
            return;
        }
        if (base != StringCRC::kZero)
            mDomain->SetBase(base);
        if (hadSelection && selX < target.width && selY < target.height)
            SelectCell(selX, selY);
        DIA_LOG_INFO("CoW", "WorldGenViewHost: grid rebuilt for %dx%d%s", target.width, target.height,
            hadSelection ? (HasSelection() ? " (selection kept)" : " (selection cleared)") : "");
    }
}

Dia::CellLayers::CellPickerView WorldGenViewHost::PickerView() const
{
    Dia::CellLayers::CellPickerView v;
    if (mDeps.windowSize)
    {
        const Vector2D win = mDeps.windowSize();
        v.viewportWidth = win.x;
        v.viewportHeight = win.y;
    }
    v.cameraX = mCamera.State().centreX;
    v.cameraY = mCamera.State().centreY;
    v.zoom = mCamera.State().zoom;
    return v;
}

void WorldGenViewHost::FitCamera()
{
    if (!mDeps.windowSize || !mGrid)
        return;
    const Vector2D win = mDeps.windowSize();
    if (win.x <= 1.0f || win.y <= 1.0f)
        return;   // window size not known yet: retry next frame
    mCamera.FitToGrid(win.x, win.y, mGrid->BoundsMin(), mGrid->BoundsMax(), 0.1f);
    mFitW = win.x;
    mFitH = win.y;
    mNeedFit = false;
}

void WorldGenViewHost::Update(const Dia::SimTime::SimTimeContext& ctx)
{
    if (!mDomain)
        return;
    OnSizeChanged();

    if (mDeps.windowSize)
    {
        const Vector2D win = mDeps.windowSize();
        if (win.x != mFitW || win.y != mFitH)
            mNeedFit = true;   // window resized: refit
    }
    if (mNeedFit)
        FitCamera();

    InputSnapshot snap;
    const bool haveInput = mDeps.readInput && mDeps.readInput(snap) && snap.pointer.valid;
    if (haveInput)
    {
        WorldGeneration::CameraInput in = snap.camera;
        in.dtSeconds = (ctx.timeScale > 0.0f) ? ctx.gameDt.AsFloatInSeconds() / ctx.timeScale : kFrameDt;   // unscaled: pan works while paused or slowed
        if (in.dtSeconds <= 0.0f || in.dtSeconds > 0.25f)
            in.dtSeconds = kFrameDt;
        in.mouseX = snap.pointer.x;
        in.mouseY = snap.pointer.y;
        in.dragActive = snap.pointer.middle || snap.pointer.right;
        if (in.dragActive && mHavePrevMouse)
        {
            in.dragDeltaX = snap.pointer.x - mPrevMouseX;
            in.dragDeltaY = snap.pointer.y - mPrevMouseY;
        }
        mPrevMouseX = snap.pointer.x;
        mPrevMouseY = snap.pointer.y;
        mHavePrevMouse = true;
        mCamera.Update(in);
    }

    if (mDeps.setCamera)
    {
        if (!mCameraSaved && mDeps.getCamera)
        {
            mDeps.getCamera(mSavedX, mSavedY, mSavedZoom);
            mCameraSaved = true;
        }
        mDeps.setCamera(mCamera.State().centreX, mCamera.State().centreY, mCamera.State().zoom);
    }

    if (haveInput)
    {
        mDomain->SubmitPointerFrame(snap.pointer, PickerView());
        if (!mFirstPointerLogged)
        {
            mFirstPointerLogged = true;
            DIA_LOG_DEBUG("CoW", "WorldGenViewHost: first pointer frame submitted to the viewer");
        }
    }
}

void WorldGenViewHost::Draw(Dia::Core::IDebugDraw& draw)
{
    if (!mDomain)
        return;
    OnSizeChanged();   // a Tick may have changed the size since the last Update
    mDomain->Update(draw);
    const uint64_t uploads = mUploader->Uploads();
    if (uploads != mSeenUploads)
    {
        mSeenUploads = uploads;
        ++mUploadFrames;
        mUploadedRevision = WorldLayerSource::Version(mSession);
    }
}

// ---- Overlays ----------------------------------------------------------------------------------------------------

bool WorldGenViewHost::SelectedCell(const void* selectionView, int& cell) const
{
    const auto* view = static_cast<const Dia::CellLayers::ICellSelectionView*>(selectionView);
    if (view == nullptr || view->Count() != 1)
        return false;
    int found = -1;
    view->ForEach([](uint32_t idx, void* user) { *static_cast<int*>(user) = static_cast<int>(idx); }, &found);
    cell = found;
    return found >= 0;
}

void WorldGenViewHost::DrawRivers(Dia::Core::IDebugDraw& draw, const void* /*selection*/)
{
    if (mSession.Revision() == 0)
        return;
    if (mRiverRevision != mSession.Revision() || mRiverGridGeneration != mGridGeneration)
    {
        mRiverSegments = BuildRiverSegments(mSession.World(), *mGrid);
        mRiverRevision = mSession.Revision();
        mRiverGridGeneration = mGridGeneration;
        ++mRiverRebuilds;
    }
    const Dia::Core::RGBA colour(70, 150, 255, 255);
    for (const LineSegment& s : mRiverSegments)
        draw.RequestDraw(s.a, s.b, colour);
}

void WorldGenViewHost::DrawNeighbours(Dia::Core::IDebugDraw& draw, const void* selection)
{
    int cell = -1;
    if (!SelectedCell(selection, cell))
        return;
    if (mOutlineCell != cell || mOutlineGridGeneration != mGridGeneration)
    {
        mOutline = BuildNeighbourOutline(*mGrid, cell);
        mOutlineCell = cell;
        mOutlineGridGeneration = mGridGeneration;
    }
    const Dia::Core::RGBA line(255, 220, 0, 255);
    for (const LineSegment& s : mOutline.neighbourOutlines)
        draw.RequestDraw(s.a, s.b, line);
    for (int d = 0; d < 6; ++d)
    {
        if (!mOutline.hasLabel[d])
            continue;
        char label[2] = { static_cast<char>('0' + d), '\0' };
        draw.RequestDrawText(mOutline.labelAnchor[d], label, kLabelFontSize, Dia::Core::RGBA(255, 255, 255, 255));
    }
}

// ---- Detail provider ---------------------------------------------------------------------------------------------

void WorldGenViewHost::BuildCellDetails(uint32_t cellIndex, Dia::CellLayers::CellDetailBuilder& out)
{
    const int index = static_cast<int>(cellIndex);
    if (mDetailCell != index || mDetailRevision != mSession.Revision())
    {
        Json::Value args(Json::objectValue);
        args["index"] = index;
        mDetailCache = mSession.Cell(args);
        mDetailCell = index;
        mDetailRevision = mSession.Revision();
    }
    const Json::Value& c = mDetailCache;
    if (!c.isObject() || !c["success"].asBool())
        return;
    char text[48];
    out.Section("Terrain");
    snprintf(text, sizeof(text), "%d,%d", c["x"].asInt(), c["y"].asInt());
    out.Row("Cell", text);
    out.Row("Terrain", c["terrain"].asCString());
    out.RowValue(StringCRC("cow.worldgen.layers/elevation"), c["elevation"].asFloat());
    out.RowValue(StringCRC("cow.worldgen.layers/latitude"), c["latitude"].asFloat());
    out.Row("Water source", c["waterSource"].asCString());
    out.Row("Depth zone", c["depthZone"].asCString());
    snprintf(text, sizeof(text), "%u", c["landmass"].asUInt());
    out.Row("Landmass", text);
    if (c["downstream"].isNull())
        out.Row("Downstream", "none");
    else
    {
        snprintf(text, sizeof(text), "%u", c["downstream"].asUInt());
        out.Row("Downstream", text);
    }
}

// ---- IWorldGenView -----------------------------------------------------------------------------------------------

bool WorldGenViewHost::SelectCell(int x, int y)
{
    if (!mDomain || !mGrid)
        return false;
    const GridDimensions d = mGrid->Dimensions();
    if (x < 0 || y < 0 || x >= d.width || y >= d.height)
        return false;
    Dia::CellLayers::CellSelection* sel = mDeps.registry->SelectionFor(&mGrid->Desc());
    if (sel == nullptr)
        return false;
    return Dia::CellLayers::SelectionTools::SelectCell(*sel, x, y).ok;
}

void WorldGenViewHost::ClearSelection()
{
    if (!mDomain || !mGrid)
        return;
    if (Dia::CellLayers::CellSelection* sel = mDeps.registry->SelectionFor(&mGrid->Desc()))
        Dia::CellLayers::SelectionTools::ClearSelection(*sel);
}

bool WorldGenViewHost::HasSelection()
{
    int x, y;
    return GetSelection(x, y);
}

bool WorldGenViewHost::GetSelection(int& x, int& y)
{
    if (!mDomain || !mGrid)
        return false;
    const Dia::CellLayers::ICellSelectionView* view = mDeps.registry->SelectionView(&mGrid->Desc());
    if (view == nullptr || view->Count() == 0)
        return false;
    int first = -1;
    view->ForEach([](uint32_t idx, void* user)
    {
        int& f = *static_cast<int*>(user);
        if (f < 0) f = static_cast<int>(idx);
    }, &first);
    if (first < 0)
        return false;
    const int w = mGrid->Dimensions().width;
    x = first % w;
    y = first / w;
    return true;
}

bool WorldGenViewHost::SetColourMode(const char* id)
{
    if (!mDomain || id == nullptr)
        return false;
    const char* resolved = (std::strcmp(id, kDefaultModeAlias) == 0) ? DefaultBaseId() : id;
    if (WorldLayerSource::Find(resolved) == nullptr)
        return false;
    return mDomain->SetBase(StringCRC(resolved));
}

const char* WorldGenViewHost::GetColourMode()
{
    if (mDomain)
    {
        const StringCRC active = mDomain->ActiveBase();
        for (uint32_t i = 0; i < WorldLayerSource::Count(); ++i)
            if (StringCRC(WorldLayerSource::At(i).id) == active)
                return WorldLayerSource::At(i).id;
    }
    return DefaultBaseId();
}

void WorldGenViewHost::ListColourModes(const std::function<void(const char* id)>& fn)
{
    for (uint32_t i = 0; i < WorldLayerSource::Count(); ++i)
        fn(WorldLayerSource::At(i).id);
}

bool WorldGenViewHost::SetOverlayEnabled(const char* id, bool enabled)
{
    if (!mDomain || id == nullptr)
        return false;
    if (std::strcmp(id, kOverlayRivers) != 0 && std::strcmp(id, kOverlayNeighbours) != 0)
        return false;
    const StringCRC crc(id);
    if (mDomain->IsOverlayEnabled(crc) != enabled)
        return mDomain->ToggleOverlay(crc);
    return true;
}

void WorldGenViewHost::ListOverlays(const std::function<void(const char* id, bool enabled, bool available)>& fn)
{
    if (!mDomain)
        return;
    const bool hasWorld = mSession.Revision() > 0;
    fn(kOverlayRivers, mDomain->IsOverlayEnabled(StringCRC(kOverlayRivers)), hasWorld);
    fn(kOverlayNeighbours, mDomain->IsOverlayEnabled(StringCRC(kOverlayNeighbours)), HasSelection());
}

void WorldGenViewHost::FillLegend(Json::Value& out)
{
    out = Json::Value(Json::nullValue);
    if (!mDomain)
        return;
    const uint32_t base = mDomain->ActiveBase().Value();
    const uint64_t uploads = mUploader->Uploads();
    if (!mLegendValid || mLegendBase != base || mLegendRevision != mSession.Revision() || mLegendUploads != uploads)
    {
        mLegendCache = Json::Value(Json::nullValue);
        Json::Value state(Json::objectValue);
        mDomain->GetJSONState(state);
        const Json::Value& l = state["legend"];
        if (l.isObject() && l["available"].asBool())
        {
            if (l["category"].asBool())
            {
                mLegendCache = Json::Value(Json::arrayValue);
                for (const Json::Value& sw : l["swatches"])
                {
                    Json::Value e(Json::objectValue);
                    e["label"] = sw["label"];
                    e["colour"] = Rgb(sw["rgba"].asUInt());
                    mLegendCache.append(e);
                }
            }
            else
            {
                mLegendCache = Json::Value(Json::objectValue);
                mLegendCache["min"] = l["min"];
                mLegendCache["max"] = l["max"];
                mLegendCache["ramp"] = l["ramp"];
                mLegendCache["unit"] = l["unit"];
                Json::Value stops(Json::arrayValue);
                for (const Json::Value& st : l["stops"])
                {
                    Json::Value e(Json::objectValue);
                    e["t"] = st["t"];
                    e["colour"] = Rgb(st["rgba"].asUInt());
                    stops.append(e);
                }
                mLegendCache["stops"] = stops;
            }
        }
        mLegendBase = base;
        mLegendRevision = mSession.Revision();
        mLegendUploads = uploads;
        mLegendValid = true;
    }
    out = mLegendCache;
}

} // namespace CoW

#endif // DIA_DEBUG
