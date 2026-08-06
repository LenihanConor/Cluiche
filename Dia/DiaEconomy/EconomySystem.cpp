#include "DiaEconomy/EconomySystem.h"
#include "DiaEconomy/EconomyInstance.h"
#include "DiaEconomy/EconomySchema.h"
#include <DiaObservation/Log/DiaLog.h>
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

        const float oldVal = instance.GetValue(resource_name);
        const float maxVal = instance.GetMaximum(resource_name);
        const float newVal = oldVal + amount;
        const bool  clamped = newVal > maxVal;

        instance.SetValue_Internal(resource_name, newVal); // SetValue_Internal clamps internally

        // TODO Task 5: fire OnPoolChanged(instance, resource_name, instance.GetValue(resource_name), delta)
        // TODO Task 5: if clamped fire OnPoolReachedMaximum; fire OnTransactionClamped

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

        const float oldVal = instance.GetValue(resource_name);
        const float minVal = instance.GetMinimum(resource_name);
        const float newVal = oldVal - amount;
        const bool  clamped = newVal < minVal;

        instance.SetValue_Internal(resource_name, newVal); // SetValue_Internal clamps internally

        // TODO Task 5: fire OnPoolChanged(instance, resource_name, instance.GetValue(resource_name), delta)
        // TODO Task 5: if clamped fire OnPoolReachedMinimum; fire OnTransactionClamped

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

        // TODO Task 5: fire OnTransferCompleted(from, to, resource_name, amount)

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

        const float minVal    = instance.GetMinimum(resource_name);
        const float maxVal    = instance.GetMaximum(resource_name);
        const bool  clamped   = value < minVal || value > maxVal;

        instance.SetValue_Internal(resource_name, value); // SetValue_Internal clamps internally

        // TODO Task 5: fire OnPoolChanged(instance, resource_name, instance.GetValue(resource_name), delta)
        // TODO Task 5: if clamped fire OnTransactionClamped

        return clamped ? TransactionResult::Clamped : TransactionResult::Success;
    }

    // -----------------------------------------------------------------------
    // Tick
    // -----------------------------------------------------------------------
    void EconomySystem::Tick(EconomyInstance& instance, float delta_seconds)
    {
        const EconomySchema* schema = instance.GetSchema();
        if (!schema)
        {
            return;
        }

        const unsigned int ruleCount = schema->GetIncomeRuleCount();
        for (unsigned int i = 0; i < ruleCount; ++i)
        {
            const IncomeRule& rule        = schema->GetIncomeRuleByIndex(i);
            const float       earned      = rule.amount_per_second * delta_seconds;
            const float       accumulated = instance.GetIncomeAccumulator(rule.resource_name) + earned;
            const float       whole       = floorf(accumulated);

            instance.SetIncomeAccumulator_Internal(rule.resource_name, accumulated - whole);

            if (whole >= 1.0f)
            {
                Earn(instance, rule.resource_name, whole);
            }
        }
    }

}} // namespace Dia::Economy
