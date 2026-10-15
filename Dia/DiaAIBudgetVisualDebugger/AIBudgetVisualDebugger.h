////////////////////////////////////////////////////////////////////////////////
// Filename: AIBudgetVisualDebugger.h
// Description: IDebugDomain implementation for DiaAIBudget.
//              Panel-only (no world drawers). Emits budget fill bar and
//              per-system timing table. Entire module is #ifdef DIA_DEBUG guarded.
// System spec: docs/specs/applications/dia/systems/diaaibudgetvisualdebugger/diaaibudgetvisualdebugger.md
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifdef DIA_DEBUG

#include <DiaVisualDebugger/Domain/IDebugDomain.h>
#include <atomic>

namespace Dia { namespace AIBudget { class AIBudgetScheduler; struct AIBudgetResult; } }

namespace Dia
{
    namespace AIBudget
    {
        class AIBudgetVisualDebugger : public Dia::VisualDebugger::IDebugDomain
        {
        public:
            AIBudgetVisualDebugger(const AIBudgetScheduler& scheduler,
                                   const AIBudgetResult&    result);

            Dia::Core::StringCRC GetDomainId()     const override;
            const char*          GetDisplayName()  const override;
            const char*          GetDescription()  const override;
            Dia::Core::StringCRC GetGroup()        const override;
            Dia::Core::RGBA      GetAccentColour() const override;

            bool HasWorldDrawers() const override { return false; }

            void GetJSONState(Json::Value& out) override;
            void OnCommand(Dia::Core::StringCRC cmd, const Json::Value& args) override;

        private:
            const AIBudgetScheduler& mScheduler;
            const AIBudgetResult&    mResult;
            std::atomic<bool>        mBudgetBarEnabled{true};
            std::atomic<bool>        mSystemTimingsEnabled{true};
        };
    }
}

#endif // DIA_DEBUG
