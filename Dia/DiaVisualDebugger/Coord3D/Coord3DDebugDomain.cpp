#include "Coord3DDebugDomain.h"

#ifdef DIA_DEBUG

#include "Coord3DOriginDrawer.h"
#include "Coord3DAxesDrawer.h"
#include "Coord3DGridDrawer.h"
#include "Coord3DCameraDrawer.h"

#include <DiaVisualDebugger/DebugLayerManager.h>
#include <DiaDebugDraw/Domain/DebugGroupAccents.h>
#include <DiaCore/DebugDraw/DebugLayerNames.h>
#include <DiaGraphics3D/FrameData3D.h>
#include <DiaGraphics3D/Camera3D.h>

namespace Dia::Debug
{

namespace
{
    const char* const kDrawerLabels[Coord3DDebugDomain::kDrawerCount] =
    {
        "Origin",
        "Axes",
        "Grid",
        "Camera",
    };

    // Priorities preserved from pre-migration VisualDebuggerModule wiring.
    const int kDrawerPriorities[Coord3DDebugDomain::kDrawerCount] =
    {
        50, 51, 52, 53
    };

    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kCmdSetScale("setScale");
    const Dia::Core::StringCRC kScaleKeyDebugScale("debugScale");
}

Coord3DDebugDomain::Coord3DDebugDomain() = default;
Coord3DDebugDomain::~Coord3DDebugDomain() = default;

// Identity
Dia::Core::StringCRC Coord3DDebugDomain::GetDomainId() const  { return Dia::Core::StringCRC("Coord3D"); }
const char* Coord3DDebugDomain::GetDisplayName() const        { return "Coord3D"; }
const char* Coord3DDebugDomain::GetDescription() const        { return "3D coordinate system — origin, axes, camera frustum"; }
Dia::Core::StringCRC Coord3DDebugDomain::GetGroup() const     { return Dia::Core::StringCRC("CoreDebug"); }
Dia::Core::RGBA Coord3DDebugDomain::GetAccentColour() const   { return Dia::VisualDebugger::DebugGroupAccents::kCoreDebug; }

// Lifecycle
void Coord3DDebugDomain::Register(DebugLayerManager& mgr)
{
    if (mLayerManager != nullptr) return;

    mOriginDrawer = std::make_unique<Coord3DOriginDrawer>(mgr);
    mAxesDrawer   = std::make_unique<Coord3DAxesDrawer>(mgr);
    mGridDrawer   = std::make_unique<Coord3DGridDrawer>(mgr);
    mCameraDrawer = std::make_unique<Coord3DCameraDrawer>(mgr);

    // RegisterWithoutDraw: these 3D drawers are driven by DrawCoord3D(), not
    // the 2D frame pass inside DebugLayerManager::Draw().
    for (int i = 0; i < kDrawerCount; ++i)
        mgr.RegisterWithoutDraw(GetDrawer(i), kDrawerPriorities[i], Dia::Debug::LayerNames::kCoord3DStageTag);

    // Coord3D overlays are opt-in — disable all layers after registration.
    mgr.DisableLayer(Dia::Debug::LayerNames::kCoord3DOrigin);
    mgr.DisableLayer(Dia::Debug::LayerNames::kCoord3DAxes);
    mgr.DisableLayer(Dia::Debug::LayerNames::kCoord3DGrid);
    mgr.DisableLayer(Dia::Debug::LayerNames::kCoord3DCamera);

    mLayerManager = &mgr;
}

void Coord3DDebugDomain::Unregister(DebugLayerManager& mgr)
{
    for (int i = 0; i < kDrawerCount; ++i)
        if (IVisualDebugger* d = GetDrawer(i)) mgr.Unregister(d->GetLayerName());
    mOriginDrawer.reset(); mAxesDrawer.reset(); mGridDrawer.reset(); mCameraDrawer.reset();
    mLayerManager = nullptr;
}

// Panel bridge
void Coord3DDebugDomain::GetJSONState(Json::Value& out)
{
    Json::Value drawers(Json::arrayValue);
    for (int i = 0; i < kDrawerCount; ++i)
    {
        const IVisualDebugger* drawer = GetDrawer(i);
        Json::Value entry(Json::objectValue);
        entry["name"]    = kDrawerLabels[i];
        entry["enabled"] = (mLayerManager != nullptr && drawer != nullptr)
                         ? mLayerManager->IsLayerEnabled(drawer->GetLayerName())
                         : false;
        drawers.append(entry);
    }
    out["drawers"] = drawers;

    Json::Value stats(Json::objectValue);
    if (mLayerManager != nullptr)
    {
        const Dia::Graphics3D::Camera3D& cam = mLayerManager->GetCamera3D();
        Json::Value eye(Json::arrayValue);
        eye.append(cam.eye.x); eye.append(cam.eye.y); eye.append(cam.eye.z);
        Json::Value fwd(Json::arrayValue);
        fwd.append(cam.forward.x); fwd.append(cam.forward.y); fwd.append(cam.forward.z);
        stats["eye"]    = eye;
        stats["dir"]    = fwd;
        stats["fovDeg"] = cam.fovYDeg;
        stats["nearZ"]  = cam.nearZ;
        stats["farZ"]   = cam.farZ;
    }
    out["stats"] = stats;
}

void Coord3DDebugDomain::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
{
    if (mLayerManager == nullptr) return;
    if (cmd == kCmdToggle)
    {
        if (!args.isMember("drawer") || !args["drawer"].isString()) return;
        const Dia::Core::StringCRC layerName = ResolveLayerName(args["drawer"].asCString());
        if (layerName == Dia::Core::StringCRC::kZero) return;
        if (mLayerManager->IsLayerEnabled(layerName)) mLayerManager->DisableLayer(layerName);
        else mLayerManager->EnableLayer(layerName);
        return;
    }
    if (cmd == kCmdSetScale)
    {
        if (!args.isMember("value") || !args["value"].isNumeric()) return;
        const Dia::Core::StringCRC key = args.isMember("key") && args["key"].isString()
                                       ? Dia::Core::StringCRC(args["key"].asCString())
                                       : kScaleKeyDebugScale;
        if (key == kScaleKeyDebugScale)
            mLayerManager->SetDebugScale(static_cast<float>(args["value"].asDouble()));
    }
}

// Drawer access
int Coord3DDebugDomain::GetDrawerCount() const { return mOriginDrawer ? kDrawerCount : 0; }

IVisualDebugger* Coord3DDebugDomain::GetDrawer(int index)
{
    switch (index)
    {
    case 0: return mOriginDrawer.get();
    case 1: return mAxesDrawer.get();
    case 2: return mGridDrawer.get();
    case 3: return mCameraDrawer.get();
    default: return nullptr;
    }
}

void Coord3DDebugDomain::DrawCoord3D(Dia::Graphics3D::FrameData3D& frame)
{
    if (mLayerManager == nullptr) return;
    if (mOriginDrawer && mLayerManager->IsLayerEnabled(Dia::Debug::LayerNames::kCoord3DOrigin))
        mOriginDrawer->Draw(frame);
    if (mAxesDrawer && mLayerManager->IsLayerEnabled(Dia::Debug::LayerNames::kCoord3DAxes))
        mAxesDrawer->Draw(frame);
    if (mGridDrawer && mLayerManager->IsLayerEnabled(Dia::Debug::LayerNames::kCoord3DGrid))
        mGridDrawer->Draw(frame);
    if (mCameraDrawer && mLayerManager->IsLayerEnabled(Dia::Debug::LayerNames::kCoord3DCamera))
        mCameraDrawer->Draw(frame);
}

Dia::Core::StringCRC Coord3DDebugDomain::ResolveLayerName(const char* drawerName) const
{
    if (drawerName == nullptr || drawerName[0] == '\0') return Dia::Core::StringCRC::kZero;
    Coord3DDebugDomain* self = const_cast<Coord3DDebugDomain*>(this);
    const Dia::Core::StringCRC requested(drawerName);
    for (int i = 0; i < kDrawerCount; ++i)
    {
        const IVisualDebugger* drawer = self->GetDrawer(i);
        if (drawer == nullptr) continue;
        if (requested == Dia::Core::StringCRC(kDrawerLabels[i]) ||
            requested == drawer->GetLayerName())
            return drawer->GetLayerName();
    }
    return Dia::Core::StringCRC::kZero;
}

} // namespace Dia::Debug

#endif // DIA_DEBUG
