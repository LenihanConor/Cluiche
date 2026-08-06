#pragma once

#include <DiaTriggerScript/ITriggerActionHandler.h>
#include <DiaTriggerScript/TriggerDef.h>
#include <DiaCondition/ConditionExpr.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia { namespace TriggerScript { namespace Testing {

    // Records all dispatched action Execute() calls for assertion in tests.
    class MockActionHandler : public ITriggerActionHandler
    {
    public:
        struct Call
        {
            Dia::Core::StringCRC triggerId;
        };

        const Dia::Core::Containers::DynamicArrayC<Call, 32>& GetCalls() const { return mCalls; }
        void Clear() { mCalls.RemoveAll(); }

        void Execute(const ActionContext& ctx) override
        {
            if (!mCalls.IsFull())
            {
                Call c;
                c.triggerId = ctx.triggerId;
                mCalls.Add(c);
            }
        }

    private:
        Dia::Core::Containers::DynamicArrayC<Call, 32> mCalls;
    };

    // Build a minimal one-shot state TriggerDef with a single action for unit tests.
    // Caller moves the ConditionExpr in; actionType names the handler to dispatch to.
    inline TriggerDef MakeStateTrigger(
        Dia::Core::StringCRC         id,
        Dia::Condition::ConditionExpr condition,
        Dia::Core::StringCRC         actionType,
        bool                         oneShot = true)
    {
        TriggerDef def;
        def.id      = id;
        def.type    = TriggerType::kState;
        def.oneShot = oneShot;
        def.state.condition = std::move(condition);

        ActionDef action;
        action.actionType = actionType;
        def.actions.Add(action);

        return def;
    }

}}} // namespace Dia::TriggerScript::Testing
