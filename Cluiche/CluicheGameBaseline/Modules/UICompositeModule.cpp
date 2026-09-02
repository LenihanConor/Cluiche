#include "Modules/UICompositeModule.h"
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>
#include <DiaCore/Time/TimeAbsolute.h>
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Metric/MetricRegistry.h>

namespace Cluiche { namespace AppFlow {

const Dia::Core::StringCRC UICompositeModule::kTypeId("UICompositeModule");

UICompositeModule::UICompositeModule(const Dia::Core::StringCRC& instanceId)
    : SimModule(instanceId)
{}

Dia::ApplicationFlow::StartResult UICompositeModule::DoStart()
{
    auto& metricReg = Dia::Observation::Metric::MetricRegistry::Instance();
    if (!mMetricUIFramesWritten)
        mMetricUIFramesWritten = metricReg.RegisterCounter(Dia::Core::StringCRC("ui_composite.frames_with_ui"));
    if (!mMetricEmptyFramesWritten)
        mMetricEmptyFramesWritten = metricReg.RegisterCounter(Dia::Core::StringCRC("ui_composite.frames_empty"));

    return Dia::ApplicationFlow::StartResult::kReady;
}

void UICompositeModule::DoUpdate(const Dia::SimTime::SimTimeContext& /*ctx*/)
{
    mFrame.Clear();

    // Gather this frame's layers. Producers run on the same SimPU thread, so
    // these fetches are race-free; a producer that ticks after us contributes
    // its frame one tick late (invisible at 30Hz, and never a partial frame).
    const Dia::Graphics::FrameData* sceneFrame = mSceneInput.FetchLatest();
    const Dia::Graphics::FrameData* debugFrame = mDebugInput.FetchLatest();

    // Choose the base frame — it carries camera/window plus its own draw lists.
    // When both scene geometry and a debug overlay are present (e.g. sprites +
    // VisualDebugger), use the debug frame as base (camera + debug primitives)
    // and append the scene's sprites, which live in a disjoint sub-buffer.
    if (debugFrame != nullptr)
    {
        mFrame.Copy(*debugFrame);
        if (sceneFrame != nullptr)
        {
            const Dia::Core::Containers::DynamicArrayC<Dia::Graphics::SpriteDrawCommand, 256>&
                sprites = sceneFrame->GetSprites();
            for (unsigned int i = 0; i < sprites.Size(); ++i)
                mFrame.RequestDrawSprite(sprites[i]);
        }
    }
    else if (sceneFrame != nullptr)
    {
        mFrame.Copy(*sceneFrame);
    }

    // Composite the Ultralight UI buffer on top.
    const Dia::UI::UIDataBuffer* uiBuffer = mUIInput.FetchLatest();
    static bool sLastHadUI = false;
    const bool hasUI = (uiBuffer && uiBuffer->GetBufferSize() > 0);

    if (hasUI)
    {
        mFrame.RequestDrawUI(*uiBuffer);
        if (mMetricUIFramesWritten)
            mMetricUIFramesWritten->Inc();
    }
    else if (mMetricEmptyFramesWritten)
    {
        mMetricEmptyFramesWritten->Inc();
    }

    // Log UI visibility state transitions
    if (hasUI != sLastHadUI)
    {
        DIA_LOG_INFO("Rendering", "UICompositeModule: UI state %s (size=%d)",
                     hasUI ? "WITH_UI" : "EMPTY",
                     uiBuffer ? uiBuffer->GetBufferSize() : 0);
        sLastHadUI = hasUI;
    }

    mRenderOutput.Write(mFrame, Dia::Core::TimeAbsolute::Zero());
}

Dia::ApplicationFlow::StopResult UICompositeModule::DoStop()
{
    mMetricUIFramesWritten = nullptr;
    mMetricEmptyFramesWritten = nullptr;
    return Dia::ApplicationFlow::StopResult::kDone;
}

void UICompositeModule::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mRenderOutput.Connect(app);
    mSceneInput.Connect(app);
    mDebugInput.Connect(app);
    mUIInput.Connect(app);
}

} } // namespace Cluiche::AppFlow

namespace { using UICompositeModule_ = Cluiche::AppFlow::UICompositeModule; }
DIA_MODULE(UICompositeModule_);
DIA_DESCRIBE(UICompositeModule_::kTypeId, "Composites UI layer outputs into the final render target each frame.");
