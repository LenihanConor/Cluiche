#include <DiaSimTime/DiaRenderTime.h>
#include <DiaApplicationFlow/Application.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>

namespace Dia::SimTime {

const Dia::Core::StringCRC DiaRenderTime::kTypeId("DiaRenderTime");

DiaRenderTime::DiaRenderTime(const Dia::Core::StringCRC& instanceId)
    : Dia::ApplicationFlow::RenderModule(instanceId)
{
}

Dia::ApplicationFlow::StartResult DiaRenderTime::DoStart()
{
    return Dia::ApplicationFlow::StartResult::kReady;
}

void DiaRenderTime::DoUpdate(const Dia::SimTime::RenderTimeContext& /*ctx*/)
{
    // NOTE: the incoming `ctx` (unused) is the RenderTimeContext the ProcessingUnit
    // cached from the PREVIOUS tick — one-tick-stale, per RenderModule's sealed
    // forwarding contract. This method's job is to compute the NEXT RenderTimeContext
    // from the sim-time stream and push it, not consume the one it was handed.
    const Dia::SimTime::SimTimeContext* simCtx = mSimTimeReader.FetchLatest();
    if (simCtx == nullptr)
        return;   // no publisher yet (DiaSimTimeModule lands in a later phase) — expected, not an error

    ++mRenderFrameCounter;
    Dia::SimTime::RenderTimeContext renderCtx{
        /*frameDt*/    0.0f,   // TODO(Phase 1.8 / later): real wall-clock render-frame delta once
                                // FrameStreamStore ring-buffer interpolation lands; for now this
                                // module only carries the sim-time snapshot forward.
        /*simTime*/    simCtx->gameTime,
        /*renderFrame*/ mRenderFrameCounter
    };
    GetProcessingUnit()->SetRenderTimeContext(renderCtx);
}

Dia::ApplicationFlow::StopResult DiaRenderTime::DoStop()
{
    return Dia::ApplicationFlow::StopResult::kDone;
}

void DiaRenderTime::OnConnectStreams(Dia::ApplicationFlow::Application& app)
{
    mSimTimeReader.Connect(app);
}

} // namespace Dia::SimTime

namespace { using DiaRenderTime_ = Dia::SimTime::DiaRenderTime; }
DIA_MODULE(DiaRenderTime_);
DIA_DESCRIBE(DiaRenderTime_::kTypeId, "Sole reader of the sim-time FrameStream on RenderPU; pushes RenderTimeContext into the owning ProcessingUnit each tick.");
