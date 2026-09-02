#pragma once
#include <DiaApplicationFlow/Module.h>
#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaCore/SimTime/SimTimeContext.h>

namespace Dia { namespace ApplicationFlow {

    // Base for all MainPU modules. The framework tick entry DoUpdate(float) is sealed and
    // FORWARDS to the typed virtual with the wall-clock MainTimeContext (see Task 1.4).
    class MainModule : public Module
    {
    public:
        explicit MainModule(const Dia::Core::StringCRC& instanceId) : Module(instanceId) {}

    protected:
        virtual void DoUpdate(const Dia::SimTime::MainTimeContext& ctx) = 0;

    private:
        // Sealed: framework tick entry. GetMainTimeContext() is added to ProcessingUnit in
        // DiaSimTime plan Task 1.4 (same hard-cutover branch as this file).
        void DoUpdate(float /*deltaTime*/) final
        {
            DoUpdate(GetProcessingUnit()->GetMainTimeContext());
        }
    };

}} // namespace Dia::ApplicationFlow
