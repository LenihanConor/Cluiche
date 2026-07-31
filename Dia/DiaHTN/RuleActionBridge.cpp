#include "RuleActionBridge.h"

#include <DiaObservation/Log/DiaLog.h>

#include <cstring>

namespace Dia
{
    namespace HTN
    {
        // The bridge trampoline. Called as OperatorBinding::CaptureFn.
        // capture: the RuleActionFn packed as a uintptr_t.
        // Returns kSucceeded unconditionally — RuleActionFns are fire-and-forget (SD-003).
        static TaskResult BridgeTrampoline(
            uintptr_t capture,
            void* operatorContext,
            const Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 8>& /*params*/)
        {
            Dia::Rules::RuleActionFn action;
            std::memcpy(&action, &capture, sizeof(action));
            if (action)
                action(operatorContext);
            return TaskResult::kSucceeded;
        }

        void RegisterRuleActionAsOperator(Dia::Core::StringCRC operatorId,
                                          Dia::Rules::RuleActionFn action,
                                          OperatorRegistry& registry)
        {
            uintptr_t capture = 0;
            std::memcpy(&capture, &action, sizeof(action));

            OperatorBinding binding;
            binding.plainFn   = nullptr;
            binding.captureFn = &BridgeTrampoline;
            binding.capture   = capture;

            registry.Register(operatorId, binding);

            DIA_LOG_DEBUG("HTN", "htn.bridge.register: op=%u", operatorId.Value());
        }

    } // namespace HTN
} // namespace Dia
