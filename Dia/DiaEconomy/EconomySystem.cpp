#include "DiaEconomy/EconomySystem.h"
#include "DiaEconomy/EconomyInstance.h"
#include "DiaEconomy/EconomySchema.h"
#include <DiaObservation/Log/DiaLog.h>
#include <DiaObservation/Trace/DiaTrace.h>
#include <DiaObservation/Profile/DiaProfile.h>
#include <DiaCore/Core/Assert.h>
#include <cmath>

namespace Dia { namespace Economy {

    // -----------------------------------------------------------------------
    // Earn
    // -----------------------------------------------------------------------
    TransactionResult EconomySystem::Earn(EconomyInstance& instance,
                                          Dia::Core::StringCRC resource_name,
                                          float amount)
    {
        if (!instance.HasResource(resource_name))
        {
            DIA_ASSERT(false, "EconomySystem::Earn: unknown resource");
            return TransactionResult::UnknownResource;
        }

        const float oldVal  = instance.GetValue(resource_name);
        const float maxVal  = instance.GetMaximum(resource_name);
        const float newVal  = oldVal + amount;
        const bool  clamped = newVal > maxVal;

        instance.SetValue_Internal(resource_name, newVal); // SetValue_Internal clamps internally

        const float actual = instance.GetValue(resource_name);
        const float delta  = actual - oldVal;

        PoolChangedEvent pce;
        pce.instance      = &instance;
        pce.resource_name = resource_name;
        pce.new_value     = actual;
        pce.delta         = delta;
        mObserverSubject.NotifyPoolChanged(pce);

        if (clamped)
        {
            DIA_LOG_INFO("Economy", "EconomySystem::Earn clamped: resource='%s' requested=%.2f actual=%.2f",
                         resource_name.AsChar(), amount, delta);

            TransactionClampedEvent tce;
            tce.instance          = &instance;
            tce.resource_name     = resource_name;
            tce.requested_amount  = amount;
            tce.actual_amount     = delta;
            mObserverSubject.NotifyTransactionClamped(tce);

            if (oldVal < maxVal) // only fire on transition (wasn't already at max)
            {
                mObserverSubject.NotifyPoolReachedMaximum(instance, resource_name);
            }
        }

        return clamped ? TransactionResult::Clamped : TransactionResult::Success;
    }

    // -----------------------------------------------------------------------
    // Spend
    // -----------------------------------------------------------------------
    TransactionResult EconomySystem::Spend(EconomyInstance& instance,
                                           Dia::Core::StringCRC resource_name,
                                           float amount)
    {
        if (!instance.HasResource(resource_name))
        {
            DIA_ASSERT(false, "EconomySystem::Spend: unknown resource");
            return TransactionResult::UnknownResource;
        }

        const float oldVal  = instance.GetValue(resource_name);
        const float minVal  = instance.GetMinimum(resource_name);
        const float newVal  = oldVal - amount;
        const bool  clamped = newVal < minVal;

        instance.SetValue_Internal(resource_name, newVal); // SetValue_Internal clamps internally

        const float actual = instance.GetValue(resource_name);
        const float delta  = actual - oldVal;

        PoolChangedEvent pce;
        pce.instance      = &instance;
        pce.resource_name = resource_name;
        pce.new_value     = actual;
        pce.delta         = delta;
        mObserverSubject.NotifyPoolChanged(pce);

        if (clamped)
        {
            DIA_LOG_INFO("Economy", "EconomySystem::Spend clamped: resource='%s' requested=%.2f actual=%.2f",
                         resource_name.AsChar(), amount, delta);

            TransactionClampedEvent tce;
            tce.instance          = &instance;
            tce.resource_name     = resource_name;
            tce.requested_amount  = amount;
            tce.actual_amount     = delta;
            mObserverSubject.NotifyTransactionClamped(tce);

            if (oldVal > minVal) // only fire on transition (wasn't already at min)
            {
                mObserverSubject.NotifyPoolReachedMinimum(instance, resource_name);
            }
        }

        return clamped ? TransactionResult::Clamped : TransactionResult::Success;
    }

    // -----------------------------------------------------------------------
    // Transfer
    // -----------------------------------------------------------------------
    TransactionResult EconomySystem::Transfer(EconomyInstance& from,
                                              EconomyInstance& to,
                                              Dia::Core::StringCRC resource_name,
                                              float amount)
    {
        const TransactionResult spendResult = Spend(from, resource_name, amount);
        if (spendResult == TransactionResult::UnknownResource)
        {
            return spendResult;
        }

        Earn(to, resource_name, amount);

        TransferCompletedEvent te;
        te.from_instance  = &from;
        te.to_instance    = &to;
        te.resource_name  = resource_name;
        te.amount         = amount;
        mObserverSubject.NotifyTransferCompleted(te);

        return spendResult; // returns result of the deduct phase
    }

    // -----------------------------------------------------------------------
    // SetValue
    // -----------------------------------------------------------------------
    TransactionResult EconomySystem::SetValue(EconomyInstance& instance,
                                              Dia::Core::StringCRC resource_name,
                                              float value)
    {
        if (!instance.HasResource(resource_name))
        {
            DIA_ASSERT(false, "EconomySystem::SetValue: unknown resource");
            return TransactionResult::UnknownResource;
        }

        const float oldVal    = instance.GetValue(resource_name);
        const float minVal    = instance.GetMinimum(resource_name);
        const float maxVal    = instance.GetMaximum(resource_name);
        const bool  clamped   = value < minVal || value > maxVal;

        instance.SetValue_Internal(resource_name, value); // SetValue_Internal clamps internally

        const float actual = instance.GetValue(resource_name);
        const float delta  = actual - oldVal;

        PoolChangedEvent pce;
        pce.instance      = &instance;
        pce.resource_name = resource_name;
        pce.new_value     = actual;
        pce.delta         = delta;
        mObserverSubject.NotifyPoolChanged(pce);

        if (clamped)
        {
            TransactionClampedEvent tce;
            tce.instance          = &instance;
            tce.resource_name     = resource_name;
            tce.requested_amount  = value;
            tce.actual_amount     = actual;
            mObserverSubject.NotifyTransactionClamped(tce);
        }

        return clamped ? TransactionResult::Clamped : TransactionResult::Success;
    }

    // -----------------------------------------------------------------------
    // IsModifierConditionMet
    // -----------------------------------------------------------------------
    bool EconomySystem::IsModifierConditionMet(const ModifierDef& mod) const
    {
        if (mod.when_condition[0] == '\0')
        {
            return true;   // always-on modifier
        }
        if (!mConditionAdaptor)
        {
            return false;  // conditional modifier but no adaptor installed — skip
        }
        return mConditionAdaptor->Evaluate(mod.when_condition);
    }

    // -----------------------------------------------------------------------
    // RegisterDerivedResource
    // -----------------------------------------------------------------------
    void EconomySystem::RegisterDerivedResource(Dia::Core::StringCRC resource_name, DerivedResourceFn fn)
    {
        DerivedEntry entry;
        entry.key = resource_name;
        entry.fn  = fn;
        mDerivedResources.Add(entry);
    }

    // -----------------------------------------------------------------------
    // QueryDerived
    // -----------------------------------------------------------------------
    float EconomySystem::QueryDerived(const EconomyInstance& instance,
                                      Dia::Core::StringCRC resource_name) const
    {
        for (unsigned int i = 0; i < mDerivedResources.Size(); ++i)
        {
            if (mDerivedResources[i].key == resource_name)
                return mDerivedResources[i].fn(instance);
        }
        DIA_LOG_WARNING("Economy", "EconomySystem::QueryDerived: unregistered derived resource '%s'",
                        resource_name.AsChar());
        return 0.0f;
    }

    // -----------------------------------------------------------------------
    // Tick
    // -----------------------------------------------------------------------
    void EconomySystem::Tick(EconomyInstance& instance, float delta_seconds)
    {
        DIA_TRACE_ZONE("EconomySystem::Tick", Dia::Observation::Trace::Category::kNone);

        const EconomySchema* schema = instance.GetSchema();
        if (!schema)
        {
            return;
        }

        const unsigned int modCount = schema->GetModifierCount();

        {
            DIA_PROFILE_SCOPE("EconomySystem::IncomeModifiers", Dia::Observation::Profile::Category::kNone);

            // --- Income rules with multiply_income modifiers applied ---
            const unsigned int ruleCount = schema->GetIncomeRuleCount();
            for (unsigned int i = 0; i < ruleCount; ++i)
            {
                const IncomeRule& rule = schema->GetIncomeRuleByIndex(i);

                float incomeMultiplier = 1.0f;
                for (unsigned int m = 0; m < modCount; ++m)
                {
                    const ModifierDef& mod = schema->GetModifierByIndex(m);
                    if (!(mod.resource_name == rule.resource_name))                               continue;
                    if (!(mod.operation     == Dia::Core::StringCRC("multiply_income")))          continue;
                    if (!IsModifierConditionMet(mod))                                             continue;
                    incomeMultiplier *= mod.value;
                }

                const float earned      = rule.amount_per_second * delta_seconds * incomeMultiplier;
                const float accumulated = instance.GetIncomeAccumulator(rule.resource_name) + earned;
                const float whole       = floorf(accumulated);

                instance.SetIncomeAccumulator_Internal(rule.resource_name, accumulated - whole);

                if (whole >= 1.0f)
                {
                    Earn(instance, rule.resource_name, whole);
                }
            }

            // --- Flat income modifiers ---
            for (unsigned int m = 0; m < modCount; ++m)
            {
                const ModifierDef& mod = schema->GetModifierByIndex(m);
                if (!(mod.operation == Dia::Core::StringCRC("flat_income"))) continue;
                if (!IsModifierConditionMet(mod))                            continue;

                const float earned      = mod.value * delta_seconds;
                const float accumulated = instance.GetIncomeAccumulator(mod.resource_name) + earned;
                const float whole       = floorf(accumulated);

                instance.SetIncomeAccumulator_Internal(mod.resource_name, accumulated - whole);

                if (whole >= 1.0f)
                {
                    Earn(instance, mod.resource_name, whole);
                }
            }

            // --- multiply_cap modifiers ---
            for (unsigned int m = 0; m < modCount; ++m)
            {
                const ModifierDef& mod = schema->GetModifierByIndex(m);
                if (!(mod.operation == Dia::Core::StringCRC("multiply_cap"))) continue;
                if (!IsModifierConditionMet(mod))                             continue;

                const float baseCap      = instance.GetMaximum(mod.resource_name);
                const float effectiveCap = baseCap * mod.value;
                const float curVal       = instance.GetValue(mod.resource_name);

                if (curVal > effectiveCap)
                {
                    instance.SetValue_Internal(mod.resource_name, effectiveCap);
                }
            }
        }
    }

}} // namespace Dia::Economy
