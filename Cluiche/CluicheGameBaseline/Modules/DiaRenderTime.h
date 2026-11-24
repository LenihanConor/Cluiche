#pragma once
#include <DiaApplicationFlow/RenderModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaStreams/StreamReader.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/SimTime/SimTimeContext.h>

namespace Dia::SimTime {

    // RenderPU module: the sole reader of the sim-time FrameStream and the sole producer
    // of RenderTimeContext, pushed into the owning ProcessingUnit via the framework-internal
    // SetRenderTimeContext(). Sibling RenderModules read whatever was pushed last tick via
    // GetRenderTimeContext() — one-tick-stale by design, no manifest-order dependency.
    class DiaRenderTime : public Dia::ApplicationFlow::RenderModule
    {
    public:
        static const Dia::Core::StringCRC kTypeId;
        static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kRender;
        explicit DiaRenderTime(const Dia::Core::StringCRC& instanceId);

    protected:
        Dia::ApplicationFlow::StartResult DoStart() override;
        void DoUpdate(const Dia::SimTime::RenderTimeContext& ctx) override;
        Dia::ApplicationFlow::StopResult DoStop() override;
        void OnConnectStreams(Dia::ApplicationFlow::Application& app) override;

    private:
        Dia::ApplicationFlow::StreamReader<Dia::SimTime::SimTimeContext> mSimTimeReader{this, Dia::Core::StringCRC("SimTime")};
        uint64_t mRenderFrameCounter = 0;
    };

} // namespace Dia::SimTime
