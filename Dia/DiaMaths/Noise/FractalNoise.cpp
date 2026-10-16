#pragma fp_contract(off)

#include "DiaMaths/Noise/FractalNoise.h"
#include "DiaMaths/Noise/FractalNoise.inl"

namespace Dia
{
	namespace Maths
	{
		// Total amplitude S = sum(gain^i), accumulated in octave order (same order as the fractal loops).
		float FractalMaxAmplitude(const FractalParams& params)
		{
			const int octaves = NoiseDetail::ClampOctaves(params.octaves);
			float s = 0.0f;
			float a = 1.0f;
			for (int i = 0; i < octaves; ++i)
			{
				s += a;
				a *= params.gain;
			}
			return s;
		}

		template float FractalNoise2D<NoiseType::Value>   (uint32_t, float, float, const FractalParams&);
		template float FractalNoise2D<NoiseType::Gradient>(uint32_t, float, float, const FractalParams&);
		template float FractalNoise2D<NoiseType::Simplex> (uint32_t, float, float, const FractalParams&);
		template float FractalNoise2D<NoiseType::Worley>  (uint32_t, float, float, const FractalParams&);

		template float RidgedNoise2D<NoiseType::Value>   (uint32_t, float, float, const RidgedParams&);
		template float RidgedNoise2D<NoiseType::Gradient>(uint32_t, float, float, const RidgedParams&);
		template float RidgedNoise2D<NoiseType::Simplex> (uint32_t, float, float, const RidgedParams&);

		template Warp2DResult Warp2D<NoiseType::Value>   (uint32_t, float, float, float, const FractalParams&);
		template Warp2DResult Warp2D<NoiseType::Gradient>(uint32_t, float, float, float, const FractalParams&);
		template Warp2DResult Warp2D<NoiseType::Simplex> (uint32_t, float, float, float, const FractalParams&);
		template Warp2DResult Warp2D<NoiseType::Worley>  (uint32_t, float, float, float, const FractalParams&);

#define DIA_NOISE_INSTANTIATE_WARPED(W, N) \
		template float WarpedNoise2D<NoiseType::W, NoiseType::N>(uint32_t, float, float, float, const FractalParams&, const FractalParams&);
#define DIA_NOISE_INSTANTIATE_WARPED_ROW(W) \
		DIA_NOISE_INSTANTIATE_WARPED(W, Value) DIA_NOISE_INSTANTIATE_WARPED(W, Gradient) \
		DIA_NOISE_INSTANTIATE_WARPED(W, Simplex) DIA_NOISE_INSTANTIATE_WARPED(W, Worley)
		DIA_NOISE_INSTANTIATE_WARPED_ROW(Value)
		DIA_NOISE_INSTANTIATE_WARPED_ROW(Gradient)
		DIA_NOISE_INSTANTIATE_WARPED_ROW(Simplex)
		DIA_NOISE_INSTANTIATE_WARPED_ROW(Worley)
#undef DIA_NOISE_INSTANTIATE_WARPED_ROW
#undef DIA_NOISE_INSTANTIATE_WARPED
	}
}
