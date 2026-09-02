#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaCore/SimTime/SimTimeContext.h>

namespace Dia { namespace ApplicationFlow {

    // Base for all RenderPU modules. The framework tick entry DoUpdate(float) is sealed
    // and FORWARDS to the typed virtual, pulling the RenderTimeContext the owning
    // ProcessingUnit cached this Update() (see Task 1.4). It is NOT empty — an empty body
    // would mean the module never ticks. Mirrors the TestStageModuleBase pattern (seal the
    // framework virtual, expose a typed one). RenderTimeContext carries wall-clock frameDt
    // plus a READ-ONLY sim-time snapshot; render modules never advance the sim clock.
    class RenderModule : public Module
    {
    public:
        explicit RenderModule(const Dia::Core::StringCRC& instanceId) : Module(instanceId) {}

    protected:
        virtual void DoUpdate(const Dia::SimTime::RenderTimeContext& ctx) = 0;

    private:
        // Sealed: framework tick entry. Pulls the context the PU cached this Update()
        // and forwards. NOT empty — GetRenderTimeContext() is added to ProcessingUnit in
        // DiaSimTime plan Task 1.4 (same hard-cutover branch as this file).
        void DoUpdate(float /*deltaTime*/) final
        {
            DoUpdate(GetProcessingUnit()->GetRenderTimeContext());
        }
    };

}} // namespace Dia::ApplicationFlow
