#pragma once

#include <cfloat>

namespace Dia
{
    namespace ScalarField
    {

        // Parameters for the uniform decay propagation policy.
        // All values are applied per-tick to every non-blocked cell.
        struct UniformDecayParams
        {
            float diffusionFactor = 0.8f;   // multiplier applied to each neighbour contribution
            float decayRate       = 0.05f;  // subtracted from each cell per tick before diffusion
        };

        // Propagation policy implementing uniform diffusion and decay.
        //
        // Per-cell formula (before field-level clamp):
        //   newValue = clamp(current * (1 - decayRate)
        //                    + neighbourSum * diffusionFactor * staticModifier,
        //                    0, FLT_MAX)
        //
        // The field-level [min, max] clamp is applied by DiaScalarField::Tick()
        // after ComputeCell returns.
        struct UniformDecayPolicy
        {
            explicit UniformDecayPolicy(UniformDecayParams params = {})
                : mParams(params)
            {}

            // Compute the new value for a single cell.
            //
            // @param cell            Linear cell index (informational; not used by this policy).
            // @param current         Value in the read buffer for this cell.
            // @param staticModifier  Pre-baked per-cell multiplier [0, 1].
            // @param neighbourSum    Sum of read-buffer values of all neighbours.
            // @return New value to write into the write buffer (clamped to [0, FLT_MAX]).
            float ComputeCell(int /*cell*/, float current,
                              float staticModifier,
                              float neighbourSum) const
            {
                const float decayed  = current * (1.0f - mParams.decayRate);
                const float diffused = neighbourSum * mParams.diffusionFactor * staticModifier;
                const float result   = decayed + diffused;
                return result < 0.0f ? 0.0f : result;
            }

            const UniformDecayParams& GetParams() const { return mParams; }

        private:
            UniformDecayParams mParams;
        };

    } // namespace ScalarField
} // namespace Dia
