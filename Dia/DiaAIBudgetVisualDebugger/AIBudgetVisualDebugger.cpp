////////////////////////////////////////////////////////////////////////////////
// Filename: AIBudgetVisualDebugger.cpp
// Description: IDebugDomain implementation for DiaAIBudget.
// System spec: docs/specs/applications/dia/systems/diaaibudgetvisualdebugger/diaaibudgetvisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#include "AIBudgetVisualDebugger.h"

#ifdef DIA_DEBUG

#include <DiaAIBudget/AIBudgetScheduler.h>
#include <DiaVisualDebugger/Domain/DebugGroupAccents.h>

namespace
{
    const Dia::Core::StringCRC kCmdToggle("toggle");
    const Dia::Core::StringCRC kDrawerBudgetBar("BudgetBar");
    const Dia::Core::StringCRC kDrawerSystemTimings("SystemTimings");
}

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
            const bool budgetBarEnabled     = mBudgetBarEnabled.load();
            const bool systemTimingsEnabled = mSystemTimingsEnabled.load();

            // --- drawers ---
            Json::Value drawers(Json::arrayValue);
            {
                Json::Value entry(Json::objectValue);
                entry["name"]    = "BudgetBar";
                entry["enabled"] = budgetBarEnabled;
                drawers.append(entry);
            }
            {
                Json::Value entry(Json::objectValue);
                entry["name"]    = "SystemTimings";
                entry["enabled"] = systemTimingsEnabled;
                drawers.append(entry);
            }
            out["drawers"] = drawers;

            // --- stats ---
            const float usedMs   = mResult.usedMs;
            const float budgetMs = mScheduler.GetLastBudgetMs();

            Json::Value stats(Json::objectValue);
            stats["usedMs"]           = usedMs;
            stats["budgetMs"]         = budgetMs;
            stats["systemsRun"]       = mResult.systemsRun;
            stats["systemsDeferred"]  = mResult.systemsDeferred;
            stats["registeredCount"]  = mScheduler.GetRegisteredCount();

            if (budgetBarEnabled)
            {
                int fillPct = 0;
                if (budgetMs > 0.0f)
                {
                    const int raw = static_cast<int>(usedMs / budgetMs * 100.0f + 0.5f);
                    fillPct = raw < 0 ? 0 : (raw > 100 ? 100 : raw);
                }
                stats["fillPct"] = fillPct;
            }

            out["stats"] = stats;

            // --- systems (only when SystemTimings drawer is enabled) ---
#ifdef DIA_DEBUG
            if (systemTimingsEnabled)
            {
                Json::Value systems(Json::arrayValue);
                for (unsigned int i = 0; i < mResult.perSystem.Size(); ++i)
                {
                    const auto& entry = mResult.perSystem[i];
                    Json::Value sys(Json::objectValue);
                    sys["id"]     = entry.systemId.AsChar();
                    sys["timeMs"] = entry.timeMs;
                    sys["ran"]    = entry.ran;
                    systems.append(sys);
                }
                out["systems"] = systems;
            }
#endif
        }

        void AIBudgetVisualDebugger::OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args)
        {
            if (cmd == kCmdToggle)
            {
                if (!args.isMember("drawer") || !args["drawer"].isString()) return;
                const Dia::Core::StringCRC drawerName(args["drawer"].asCString());
                if (drawerName == kDrawerBudgetBar)
                    mBudgetBarEnabled.store(!mBudgetBarEnabled.load());
                else if (drawerName == kDrawerSystemTimings)
                    mSystemTimingsEnabled.store(!mSystemTimingsEnabled.load());
                return;
            }
            // "setScale" and all other commands — no-op, no crash
        }

    } // namespace AIBudget
} // namespace Dia

#endif // DIA_DEBUG
