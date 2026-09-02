#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaCore/SimTime/SimTimeContext.h>

namespace Dia { namespace ApplicationFlow {

    // Base for all SimPU modules. The framework tick entry DoUpdate(float) is sealed
    // and FORWARDS to the typed virtual, pulling the context the owning ProcessingUnit
    // cached this Update() (see ProcessingUnit context injection, Task 1.4). It is NOT
    // empty — an empty body would mean the module never ticks. This mirrors the existing
    // TestStageModuleBase pattern (seal the framework virtual, expose a typed one).
    // PU placement (Sim vs Render vs Main) is a runtime check (TypeRegistry::GetAllowedPUs
    // + assert in ProcessingUnit::AddModule), not a compile-time one: modules attach to
    // PUs via manifest instanceId strings.
    class SimModule : public Module
    {
    public:
        explicit SimModule(const Dia::Core::StringCRC& instanceId) : Module(instanceId) {}

    protected:
        virtual void DoUpdate(const Dia::SimTime::SimTimeContext& ctx) = 0;

    private:
        // Sealed: framework tick entry. Pulls the context the PU cached this Update()
        // and forwards. NOT empty — GetSimTimeContext() is added to ProcessingUnit in
        // DiaSimTime plan Task 1.4 (same hard-cutover branch as this file).
        void DoUpdate(float /*deltaTime*/) final
        {
            DoUpdate(GetProcessingUnit()->GetSimTimeContext());
        }
    };

}} // namespace Dia::ApplicationFlow
