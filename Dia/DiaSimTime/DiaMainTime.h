#pragma once
#include <DiaApplicationFlow/MainModule.h>
#include <DiaApplicationFlow/PUAffinity.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia::SimTime {

    // Trivial MainModule presence on MainPU. ProcessingUnit::Update() already computes
    // and caches MainTimeContext directly for kMain/kAny affinity (no cross-PU handoff
    // needed, unlike RenderTimeContext) — this module exists for manifest completeness
    // and symmetry with DiaRenderTime, not because MainTimeContext needs active publishing.
    class DiaMainTime : public Dia::ApplicationFlow::MainModule
    {
    public:
        static const Dia::Core::StringCRC kTypeId;
        static constexpr Dia::ApplicationFlow::PUAffinity kAllowedPUs = Dia::ApplicationFlow::PUAffinity::kMain;
        explicit DiaMainTime(const Dia::Core::StringCRC& instanceId);

    protected:
        Dia::ApplicationFlow::StartResult DoStart() override;
        void DoUpdate(const Dia::SimTime::MainTimeContext& ctx) override;
        Dia::ApplicationFlow::StopResult DoStop() override;
    };

} // namespace Dia::SimTime
