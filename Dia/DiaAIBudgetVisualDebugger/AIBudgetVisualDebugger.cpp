////////////////////////////////////////////////////////////////////////////////
// Filename: AIBudgetVisualDebugger.cpp
// Description: IDebugDomain implementation for DiaAIBudget.
// System spec: docs/specs/applications/dia/systems/diaaibudgetvisualdebugger/diaaibudgetvisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#include "AIBudgetVisualDebugger.h"

#ifdef DIA_DEBUG

#include <DiaAIBudget/AIBudgetScheduler.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>

namespace Dia
{
    namespace AIBudget
    {
        AIBudgetVisualDebugger::AIBudgetVisualDebugger(const AIBudgetScheduler& scheduler,
                                                       const AIBudgetResult& result)
            : mScheduler(scheduler)
            , mResult(result)
        {}

        Dia::Core::StringCRC AIBudgetVisualDebugger::GetDomainId() const
        {
            return Dia::Core::StringCRC("aibudget");
        }

        const char* AIBudgetVisualDebugger::GetDisplayName() const
        {
            return "AI Budget";
        }

        const char* AIBudgetVisualDebugger::GetDescription() const
        {
            return "AI budget — scheduler fill bar, per-system run/deferred timings";
        }

        Dia::Core::StringCRC AIBudgetVisualDebugger::GetGroup() const
        {
            return Dia::Core::StringCRC("AIBehavior");
        }

        Dia::Core::RGBA AIBudgetVisualDebugger::GetAccentColour() const
        {
            return Dia::VisualDebugger::DebugGroupAccents::kAIBehavior;
        }

        void AIBudgetVisualDebugger::GetJSONState(Json::Value& out)
        {
            // TODO: implement in next task
            out["drawers"] = Json::Value(Json::arrayValue);
            out["stats"]   = Json::Value(Json::objectValue);
        }

        void AIBudgetVisualDebugger::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
        {
            // TODO: implement in next task
        }

    } // namespace AIBudget
} // namespace Dia

#endif // DIA_DEBUG
