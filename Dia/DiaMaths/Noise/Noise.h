#pragma once

#include <cstdint>

namespace Dia
{
	namespace Maths
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Seeded Noise - pure, bit-identical (seed, x, y[, z]) -> float functions.
		// No global state, no allocation, safe from any thread. See docs/specs/applications/dia/systems/diamaths/seeded-noise.md
		//---------------------------------------------------------------------------------------------------------------------------------

		enum class NoiseType : uint8_t { Value, Gradient, Simplex, Worley };

		// ---- Single-octave 2D base noises ----------------------------------------------------------------------------------------------
		float ValueNoise2D   (uint32_t seed, float x, float y);   // [-1, 1]
		float GradientNoise2D(uint32_t seed, float x, float y);   // [-1, 1]; exactly 0 at integer lattice points
		float SimplexNoise2D (uint32_t seed, float x, float y);   // [-1, 1]; OpenSimplex2S 2D

		// Worley: feature point per unit cell, jittered by 'jitter' in [0, 1] (0 = regular grid).
		// Searches the 5 x 5 cells around the sample, so f1 and f2 are exact for any jitter.
		// Distances are in cell units (scale the input to change feature size).
		struct WorleyResult
		{
			float    f1;            // distance to nearest feature point
			float    f2;            // distance to second nearest
			float    f2MinusF1;     // f2 - f1 (>= 0; 0 on cell borders: crest lines)
			uint32_t cellId;        // hash id of the nearest feature point's cell (stable, seed dependent)
			float    cellValue;     // cellId mapped to [0, 1) (exact)
		};
		WorleyResult WorleyNoise2D(uint32_t seed, float x, float y, float jitter = 1.0f);

		// ---- Fractal / ridged / warp over a base type (compile-time template parameter) ------------------------------------------------
		enum class WorleyOutput : uint8_t { F1, F2, F2MinusF1, CellValue };

		struct FractalParams
		{
			WorleyOutput worley     = WorleyOutput::F2MinusF1; // used only when the base is NoiseType::Worley (output remapped to [-1, 1])
			float        jitter     = 1.0f;                    // Worley only
			int          octaves    = 4;                       // 1..kMaxNoiseOctaves
			float        frequency  = 1.0f;                    // multiplies x, y before octave 0
			float        lacunarity = 2.0f;                    // frequency multiplier per octave
			float        gain       = 0.5f;                    // amplitude multiplier per octave
			bool         normalise  = true;                    // lower octaves stay fixed only when false
		};
		constexpr int      kMaxNoiseOctaves  = 12;
		constexpr uint32_t kOctaveSeedStride = 0x9E3779B9u;

		// Total amplitude S = sum(gain^i, i < octaves): the range bound when normalise == false.
		float FractalMaxAmplitude(const FractalParams& params);

		// fBm. normalise == true: result in [-1, 1]. normalise == false: result in [-S, S].
		template <NoiseType Base>
		float FractalNoise2D(uint32_t seed, float x, float y, const FractalParams& params);

		// Ridged multifractal over Value, Gradient or Simplex (static_assert(Base != Worley)). Per octave
		// r = (1 - |n|)^sharpness, summed with the same amplitudes. Result in [0, 1] when
		// normalise == true (1 on a ridge crest), [0, S] otherwise.
		struct RidgedParams
		{
			FractalParams fractal;
			int           sharpness = 2;   // integer 1..4, repeated multiplication (no std::pow)
		};
		template <NoiseType Base>
		float RidgedNoise2D(uint32_t seed, float x, float y, const RidgedParams& params);

		// Domain warp: displaces the point by two independent fractal noises scaled by 'strength'
		// (input units). Feed the result to any noise call.
		struct Warp2DResult { float x; float y; };
		template <NoiseType WarpBase>
		Warp2DResult Warp2D(uint32_t seed, float x, float y, float strength, const FractalParams& warp);

		// Convenience: FractalNoise2D<NoiseBase> sampled at the warped point.
		template <NoiseType WarpBase, NoiseType NoiseBase>
		float WarpedNoise2D(uint32_t seed, float x, float y, float strength,
		                    const FractalParams& warp, const FractalParams& noise);

		// ---- 3D gradient noise (time-evolving fields: pass time as z) ------------------------------------------------------------------
		float Noise3D(uint32_t seed, float x, float y, float z);                                      // [-1, 1]; 0 at integer lattice points
		float FractalNoise3D(uint32_t seed, float x, float y, float z, const FractalParams& params);  // gradient only
	}
}
