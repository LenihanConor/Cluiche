// RulesPropagationPolicy.h
// Optional header-only adaptor — gates per-cell propagation through a
// user-supplied modifier function, implementing dirty-cell wavefront
// propagation driven by rule evaluation results.
//
// OPTIONAL ADAPTOR — not part of DiaScalarField.vcxproj build.
// Consumers who use DiaCondition / RuleSet as the modifier source must
// add DiaRules to their own project dependencies.
//
// Usage example:
//   auto modifierFn = [&rulesContext](int cellIndex) -> float {
//       return rulesContext.EvaluateModifier(cellIndex);
//   };
//   RulesPropagationPolicy policy(modifierFn, 0.8f, 0.05f);
//   DiaScalarField<SquareFieldTopology, decltype(policy)> field(topology, policy);

#pragma once
#include <DiaScalarField/CellIndex.h>

namespace Dia
{
    namespace ScalarField
    {
        namespace Adaptors
        {
            // Propagation policy that gates per-cell computation through a user-supplied
            // modifier function.  The modifier maps a linear cell index to a float in
            // [0, 1].  Cells where modifier returns 0 are skipped (current value
            // preserved), implementing dirty-cell gating for rule-driven propagation.
            //
            // ModifierFn must be callable as: float fn(int cellIndex)
            //
            // ComputeCell signature matches DiaScalarField's Policy requirement:
            //   float ComputeCell(int cell, float current, float staticModifier, float neighbourSum) const
            //
            // When modifier > 0:
            //   result = current * (1 - decayRate) + neighbourSum * diffusionFactor * modifier * staticModifier
            // When modifier <= 0 (dirty-cell gate):
            //   result = current  (cell is skipped, value is preserved unchanged)
            template<typename ModifierFn>
            struct RulesPropagationPolicy
            {
                RulesPropagationPolicy(ModifierFn fn, float diffusionFactor = 0.8f, float decayRate = 0.05f)
                    : mModifierFn(fn)
                    , mDiffusionFactor(diffusionFactor)
                    , mDecayRate(decayRate)
                {}

                float ComputeCell(int cell, float current, float staticModifier, float neighbourSum) const
                {
                    const float modifier = mModifierFn(cell);
                    if (modifier <= 0.0f)
                        return current;  // dirty-cell gate: skip this cell, preserve current value

                    const float decayed  = current * (1.0f - mDecayRate);
                    const float diffused = neighbourSum * mDiffusionFactor * modifier * staticModifier;
                    const float result   = decayed + diffused;
                    return result < 0.0f ? 0.0f : result;
                }

                ModifierFn mModifierFn;
                float      mDiffusionFactor;
                float      mDecayRate;
            };

            // Deduction guides (C++17+) — allow construction without explicit template argument:
            //   RulesPropagationPolicy policy([](int idx){ return idx % 2 == 0 ? 1.0f : 0.0f; });
            template<typename Fn>
            RulesPropagationPolicy(Fn, float, float) -> RulesPropagationPolicy<Fn>;
            template<typename Fn>
            RulesPropagationPolicy(Fn) -> RulesPropagationPolicy<Fn>;

        } // namespace Adaptors
    } // namespace ScalarField
} // namespace Dia
