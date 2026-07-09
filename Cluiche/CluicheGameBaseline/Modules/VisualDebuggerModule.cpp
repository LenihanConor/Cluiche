#include "Modules/VisualDebuggerModule.h"

#ifdef DIA_DEBUG

#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaVisualDebugger/Coord2D/Coord2DOriginDrawer.h>
#include <DiaVisualDebugger/Coord2D/Coord2DAxesDrawer.h>
#include <DiaVisualDebugger/Coord2D/Coord2DGridDrawer.h>
#include <DiaVisualDebugger/Coord2D/Coord2DBoundsDrawer.h>
#include <DiaVisualDebugger/Coord2D/Coord2DCursorDrawer.h>
#include <DiaVisualDebugger/Coord3D/Coord3DOriginDrawer.h>
#include <DiaVisualDebugger/Coord3D/Coord3DAxesDrawer.h>
#include <DiaVisualDebugger/Coord3D/Coord3DGridDrawer.h>
#include <DiaVisualDebugger/Coord3D/Coord3DCameraDrawer.h>
#include <DiaVisualDebugger/DebugLayerNames.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC VisualDebuggerModule::kTypeId("VisualDebuggerModule");

VisualDebuggerModule::VisualDebuggerModule(const Dia::Core::StringCRC& instanceId)
    : Module(instanceId)
{}

Dia::ApplicationFlow::StartResult VisualDebuggerModule::DoStart()
{
    mLayerManagerService.Register(mLayerManager);
    mLayerManager.SetDebugScale(50.0f);
    mLayerManager.RegisterDiaAPICommands();
    RegisterCoord2DDrawers();
    RegisterCoord3DDrawers();
    return Dia::ApplicationFlow::StartResult::kReady;
}

void VisualDebuggerModule::DoUpdate(float /*dt*/)
{
    DIA_TRACE_ZONE("VisualDebuggerModule.Update", Dia::Observation::Trace::Category::kDiaApplicationFlow);

    auto* app = GetApplication();
    if (app)
    {
        Dia::Core::StringCRC currentStage = app->GetCurrentStage();
        if (currentStage != mLastKnownStage)
        {
            if (!(mLastKnownStage == Dia::Core::StringCRC()))
                mLayerManager.SetStageActive(mLastKnownStage, false);
            mLayerManager.SetStageActive(currentStage, true);
            mLastKnownStage = currentStage;
        }
    }

    const Dia::Camera2D::Camera2D&  camera     = mCameraRef ? mCameraRef->GetActiveCamera() : Dia::Camera2D::Camera2D{};
    const Dia::Maths::Vector2D&     windowSize = mCameraRef ? mCameraRef->GetWindowSize()   : Dia::Maths::Vector2D{1400.0f, 1000.0f};

    mLayerManager.SetViewport(camera, windowSize);

    mFrame.Clear();
    mFrame.SetCamera(camera);
    mFrame.SetWindowSize(windowSize);

    if (mInputRef.Get())
    {
        mFrame.SetMousePixel(Dia::Maths::Vector2D(
            static_cast<float>(mInputRef->GetMouseX()),
            static_cast<float>(mInputRef->GetMouseY())));
    }

    mLayerManager.Draw(mFrame);
    mRenderOutput.Write(mFrame, Dia::Core::TimeAbsolute::Zero());
}

Dia::ApplicationFlow::StopResult VisualDebuggerModule::DoStop()
{
    mLayerManager.ClearDynamicLayers();
    UnregisterCoord2DDrawers();
    UnregisterCoord3DDrawers();
    mLastKnownStage = Dia::Core::StringCRC();
    mFrame.Clear();
    mRenderOutput.Write(mFrame, Dia::Core::TimeAbsolute::Zero());
    return Dia::ApplicationFlow::StopResult::kDone;
}

void VisualDebuggerModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mRenderOutput.Connect(app);
    mLayerManagerService.Connect(app);
}

void VisualDebuggerModule::RegisterCoord2DDrawers()
{
    mCoord2DOriginDrawer = std::make_unique<Dia::Debug::Coord2DOriginDrawer>(mLayerManager);
    mCoord2DAxesDrawer   = std::make_unique<Dia::Debug::Coord2DAxesDrawer>(mLayerManager);
    mCoord2DGridDrawer   = std::make_unique<Dia::Debug::Coord2DGridDrawer>(mLayerManager);
    mCoord2DBoundsDrawer = std::make_unique<Dia::Debug::Coord2DBoundsDrawer>(mLayerManager);
    mCoord2DCursorDrawer = std::make_unique<Dia::Debug::Coord2DCursorDrawer>(mLayerManager);

    mLayerManager.Register(mCoord2DOriginDrawer.get(), 50, Dia::Debug::LayerNames::kCoord2DStageTag);
    mLayerManager.Register(mCoord2DAxesDrawer.get(),   51, Dia::Debug::LayerNames::kCoord2DStageTag);
    mLayerManager.Register(mCoord2DGridDrawer.get(),   52, Dia::Debug::LayerNames::kCoord2DStageTag);
    mLayerManager.Register(mCoord2DBoundsDrawer.get(), 53, Dia::Debug::LayerNames::kCoord2DStageTag);
    mLayerManager.Register(mCoord2DCursorDrawer.get(), 54, Dia::Debug::LayerNames::kCoord2DStageTag);

    mLayerManager.DisableLayer(Dia::Debug::LayerNames::kCoord2DOrigin);
    mLayerManager.DisableLayer(Dia::Debug::LayerNames::kCoord2DAxes);
    mLayerManager.DisableLayer(Dia::Debug::LayerNames::kCoord2DGrid);
    mLayerManager.DisableLayer(Dia::Debug::LayerNames::kCoord2DBounds);
    mLayerManager.DisableLayer(Dia::Debug::LayerNames::kCoord2DCursor);
}

void VisualDebuggerModule::UnregisterCoord2DDrawers()
{
    mCoord2DOriginDrawer.reset();
    mCoord2DAxesDrawer.reset();
    mCoord2DGridDrawer.reset();
    mCoord2DBoundsDrawer.reset();
    mCoord2DCursorDrawer.reset();
}

void VisualDebuggerModule::RegisterCoord3DDrawers()
{
    mCoord3DOriginDrawer = std::make_unique<Dia::Debug::Coord3DOriginDrawer>(mLayerManager);
    mCoord3DAxesDrawer   = std::make_unique<Dia::Debug::Coord3DAxesDrawer>(mLayerManager);
    mCoord3DGridDrawer   = std::make_unique<Dia::Debug::Coord3DGridDrawer>(mLayerManager);
    mCoord3DCameraDrawer = std::make_unique<Dia::Debug::Coord3DCameraDrawer>(mLayerManager);

    mLayerManager.RegisterWithoutDraw(mCoord3DOriginDrawer.get(), 50, Dia::Debug::LayerNames::kCoord3DStageTag);
    mLayerManager.RegisterWithoutDraw(mCoord3DAxesDrawer.get(),   51, Dia::Debug::LayerNames::kCoord3DStageTag);
    mLayerManager.RegisterWithoutDraw(mCoord3DGridDrawer.get(),   52, Dia::Debug::LayerNames::kCoord3DStageTag);
    mLayerManager.RegisterWithoutDraw(mCoord3DCameraDrawer.get(), 53, Dia::Debug::LayerNames::kCoord3DStageTag);

    mLayerManager.DisableLayer(Dia::Debug::LayerNames::kCoord3DOrigin);
    mLayerManager.DisableLayer(Dia::Debug::LayerNames::kCoord3DAxes);
    mLayerManager.DisableLayer(Dia::Debug::LayerNames::kCoord3DGrid);
    mLayerManager.DisableLayer(Dia::Debug::LayerNames::kCoord3DCamera);
}

void VisualDebuggerModule::UnregisterCoord3DDrawers()
{
    mCoord3DOriginDrawer.reset();
    mCoord3DAxesDrawer.reset();
    mCoord3DGridDrawer.reset();
    mCoord3DCameraDrawer.reset();
}

void VisualDebuggerModule::SetCamera3D(const Dia::Graphics3D::Camera3D& camera)
{
    mLayerManager.SetCamera3D(camera);
}

void VisualDebuggerModule::DrawCoord3D(Dia::Graphics3D::FrameData3D& frame)
{
    if (mCoord3DOriginDrawer && mLayerManager.IsLayerEnabled(Dia::Debug::LayerNames::kCoord3DOrigin))
        mCoord3DOriginDrawer->Draw(frame);
    if (mCoord3DAxesDrawer && mLayerManager.IsLayerEnabled(Dia::Debug::LayerNames::kCoord3DAxes))
        mCoord3DAxesDrawer->Draw(frame);
    if (mCoord3DGridDrawer && mLayerManager.IsLayerEnabled(Dia::Debug::LayerNames::kCoord3DGrid))
        mCoord3DGridDrawer->Draw(frame);
    if (mCoord3DCameraDrawer && mLayerManager.IsLayerEnabled(Dia::Debug::LayerNames::kCoord3DCamera))
        mCoord3DCameraDrawer->Draw(frame);
}

} } // namespace Cluiche::AppFlow

namespace { using VisualDebuggerModule_ = Cluiche::AppFlow::VisualDebuggerModule; }
DIA_MODULE(VisualDebuggerModule_);
DIA_DESCRIBE(VisualDebuggerModule_::kTypeId, "Renders visual debug overlays: coordinate axes, grids, physics shapes, and velocity arrows.");

#endif // DIA_DEBUG
