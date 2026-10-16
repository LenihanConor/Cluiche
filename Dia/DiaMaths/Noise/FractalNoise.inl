#pragma once

// Template definitions for FractalNoise2D / RidgedNoise2D / Warp2D / WarpedNoise2D.
// Included only by FractalNoise.cpp, which explicitly instantiates them for every NoiseType
// (Ridged: Value, Gradient, Simplex). Noise.h remains the only declaring header.

#include "DiaMaths/Noise/Noise.h"
#include "DiaMaths/Noise/NoiseHash.h"

namespace Dia
{
	namespace Maths
	{
		namespace NoiseDetail
		{
			// Seed derivation for the two independent warp displacement fields.
			constexpr uint32_t kWarpSeedX = 0x68E31DA4u;
			constexpr uint32_t kWarpSeedY = 0xB5297A4Du;

			inline int ClampOctaves(int octaves)
			{
				if (octaves < 1) return 1;
				if (octaves > kMaxNoiseOctaves) return kMaxNoiseOctaves;
				return octaves;
			}

			// Worley output -> [-1, 1] with fixed, documented mappings (distances are in cell units):
			//   F1         natural range [0, ~1]   : 2 * min(F1, 1) - 1
			//   F2         natural range [0, ~1.5] : F2 - 1, clamped (F2 in [0, 2] -> [-1, 1])
			//   F2MinusF1  natural range [0, ~1]   : 2 * min(F2 - F1, 1) - 1
			//   CellValue  range [0, 1)            : 2 * v - 1
			inline float WorleyToSigned(const WorleyResult& w, WorleyOutput out)
			{
				float v;
				switch (out)
				{
				case WorleyOutput::F1:        v = w.f1 * 2.0f - 1.0f; break;
				case WorleyOutput::F2:        v = w.f2 - 1.0f; break;
				case WorleyOutput::F2MinusF1: v = w.f2MinusF1 * 2.0f - 1.0f; break;
				default:                      v = w.cellValue * 2.0f - 1.0f; break;
				}
				return Clamp1(v);
			}

			// Single-octave sample of the base type; result in [-1, 1].
			template <NoiseType Base>
			inline float SampleBase(uint32_t seed, float x, float y, const FractalParams& params)
			{
				if constexpr (Base == NoiseType::Value)
				{
					return ValueNoise2D(seed, x, y);
				}
				else if constexpr (Base == NoiseType::Gradient)
				{
					return GradientNoise2D(seed, x, y);
				}
				else if constexpr (Base == NoiseType::Simplex)
				{
					return SimplexNoise2D(seed, x, y);
				}
				else
				{
					return WorleyToSigned(WorleyNoise2D(seed, x, y, params.jitter), params.worley);
				}
			}
		}

		template <NoiseType Base>
		float FractalNoise2D(uint32_t seed, float x, float y, const FractalParams& params)
		{
			const int octaves = NoiseDetail::ClampOctaves(params.octaves);

			float sum = 0.0f;
			float amp = 1.0f;
			float freq = params.frequency;
			for (int i = 0; i < octaves; ++i)
			{
				const uint32_t octaveSeed = seed + static_cast<uint32_t>(i) * kOctaveSeedStride;
				sum += amp * NoiseDetail::SampleBase<Base>(octaveSeed, x * freq, y * freq, params);
				amp *= params.gain;
				freq *= params.lacunarity;
			}

			if (params.normalise)
			{
				const float total = FractalMaxAmplitude(params);
				if (total > 0.0f)
				{
					sum = sum / total;
				}
				sum = NoiseDetail::Clamp1(sum);
			}
			return sum;
		}

		template <NoiseType Base>
		float RidgedNoise2D(uint32_t seed, float x, float y, const RidgedParams& params)
		{
			static_assert(Base != NoiseType::Worley, "RidgedNoise2D is not supported over Worley (use F2MinusF1 for crest lines)");

			const FractalParams& fp = params.fractal;
			const int octaves = NoiseDetail::ClampOctaves(fp.octaves);
			int sharpness = params.sharpness;
			if (sharpness < 1) sharpness = 1;
			if (sharpness > 4) sharpness = 4;

			float sum = 0.0f;
			float amp = 1.0f;
			float freq = fp.frequency;
			for (int i = 0; i < octaves; ++i)
			{
				const uint32_t octaveSeed = seed + static_cast<uint32_t>(i) * kOctaveSeedStride;
				const float n = NoiseDetail::SampleBase<Base>(octaveSeed, x * freq, y * freq, fp);
				const float a = (n < 0.0f) ? -n : n;
				const float base = 1.0f - a;
				float r = base;
				for (int k = 1; k < sharpness; ++k)
				{
					r *= base;
				}
				sum += amp * r;
				amp *= fp.gain;
				freq *= fp.lacunarity;
			}

			if (fp.normalise)
			{
				const float total = FractalMaxAmplitude(fp);
				if (total > 0.0f)
				{
					sum = sum / total;
				}
				if (sum < 0.0f) sum = 0.0f;
				if (sum > 1.0f) sum = 1.0f;
			}
			return sum;
		}

		template <NoiseType WarpBase>
		Warp2DResult Warp2D(uint32_t seed, float x, float y, float strength, const FractalParams& warp)
		{
			if (strength == 0.0f)
			{
				return Warp2DResult{ x, y };
			}
			const float dx = FractalNoise2D<WarpBase>(seed ^ NoiseDetail::kWarpSeedX, x, y, warp);
			const float dy = FractalNoise2D<WarpBase>(seed ^ NoiseDetail::kWarpSeedY, x, y, warp);
			return Warp2DResult{ x + dx * strength, y + dy * strength };
		}

		template <NoiseType WarpBase, NoiseType NoiseBase>
		float WarpedNoise2D(uint32_t seed, float x, float y, float strength,
		                    const FractalParams& warp, const FractalParams& noise)
		{
			const Warp2DResult w = Warp2D<WarpBase>(seed, x, y, strength, warp);
			return FractalNoise2D<NoiseBase>(seed, w.x, w.y, noise);
		}
	}
}
