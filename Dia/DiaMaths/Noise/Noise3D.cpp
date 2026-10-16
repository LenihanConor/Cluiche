#pragma fp_contract(off)

#include "DiaMaths/Noise/Noise3D.h"
#include "DiaMaths/Noise/NoiseHash.h"

namespace Dia
{
	namespace Maths
	{
		namespace
		{
			// The 12 classic edge vectors padded with 4 repeats to 16 entries (index = top 4 hash bits).
			constexpr float kGradients3D[16][3] =
			{
				{  1.0f,  1.0f,  0.0f }, { -1.0f,  1.0f,  0.0f }, {  1.0f, -1.0f,  0.0f }, { -1.0f, -1.0f,  0.0f },
				{  1.0f,  0.0f,  1.0f }, { -1.0f,  0.0f,  1.0f }, {  1.0f,  0.0f, -1.0f }, { -1.0f,  0.0f, -1.0f },
				{  0.0f,  1.0f,  1.0f }, {  0.0f, -1.0f,  1.0f }, {  0.0f,  1.0f, -1.0f }, {  0.0f, -1.0f, -1.0f },
				{  1.0f,  1.0f,  0.0f }, { -1.0f,  1.0f,  0.0f }, {  0.0f, -1.0f,  1.0f }, {  0.0f, -1.0f, -1.0f },
			};

			// Edge gradients have length sqrt(2); the measured extreme is ~1 so no extra scaling is needed (result is clamped).
			constexpr float kGradientScale3D = 1.0f;

			inline float Corner3(uint32_t seed, int ix, int iy, int iz, float dx, float dy, float dz)
			{
				const uint32_t i = NoiseDetail::Hash3D(seed, ix, iy, iz) >> 28;
				return kGradients3D[i][0] * dx + kGradients3D[i][1] * dy + kGradients3D[i][2] * dz;
			}

			// Same summation order as the public amplitude helper: sum(gain^i) accumulated in octave order.
			inline float TotalAmplitude(int octaves, float gain)
			{
				float s = 0.0f;
				float a = 1.0f;
				for (int i = 0; i < octaves; ++i)
				{
					s += a;
					a *= gain;
				}
				return s;
			}
		}

		float Noise3D(uint32_t seed, float x, float y, float z)
		{
			using namespace NoiseDetail;

			x = ClampCoord(x);
			y = ClampCoord(y);
			z = ClampCoord(z);

			const int ix = FloorToInt(x);
			const int iy = FloorToInt(y);
			const int iz = FloorToInt(z);
			const float fx = x - static_cast<float>(ix);
			const float fy = y - static_cast<float>(iy);
			const float fz = z - static_cast<float>(iz);

			const float n000 = Corner3(seed, ix,     iy,     iz,     fx,        fy,        fz);
			const float n100 = Corner3(seed, ix + 1, iy,     iz,     fx - 1.0f, fy,        fz);
			const float n010 = Corner3(seed, ix,     iy + 1, iz,     fx,        fy - 1.0f, fz);
			const float n110 = Corner3(seed, ix + 1, iy + 1, iz,     fx - 1.0f, fy - 1.0f, fz);
			const float n001 = Corner3(seed, ix,     iy,     iz + 1, fx,        fy,        fz - 1.0f);
			const float n101 = Corner3(seed, ix + 1, iy,     iz + 1, fx - 1.0f, fy,        fz - 1.0f);
			const float n011 = Corner3(seed, ix,     iy + 1, iz + 1, fx,        fy - 1.0f, fz - 1.0f);
			const float n111 = Corner3(seed, ix + 1, iy + 1, iz + 1, fx - 1.0f, fy - 1.0f, fz - 1.0f);

			const float u = Fade(fx);
			const float v = Fade(fy);
			const float w = Fade(fz);

			const float a0 = n000 + (n100 - n000) * u;
			const float b0 = n010 + (n110 - n010) * u;
			const float a1 = n001 + (n101 - n001) * u;
			const float b1 = n011 + (n111 - n011) * u;
			const float c0 = a0 + (b0 - a0) * v;
			const float c1 = a1 + (b1 - a1) * v;
			return Clamp1((c0 + (c1 - c0) * w) * kGradientScale3D);
		}

		float FractalNoise3D(uint32_t seed, float x, float y, float z, const FractalParams& params)
		{
			int octaves = params.octaves;
			if (octaves < 1) octaves = 1;
			if (octaves > kMaxNoiseOctaves) octaves = kMaxNoiseOctaves;

			float sum = 0.0f;
			float amp = 1.0f;
			float freq = params.frequency;
			for (int i = 0; i < octaves; ++i)
			{
				const uint32_t octaveSeed = seed + static_cast<uint32_t>(i) * kOctaveSeedStride;
				sum += amp * Noise3D(octaveSeed, x * freq, y * freq, z * freq);
				amp *= params.gain;
				freq *= params.lacunarity;
			}

			if (params.normalise)
			{
				const float total = TotalAmplitude(octaves, params.gain);
				if (total > 0.0f)
				{
					sum = sum / total;
				}
				sum = NoiseDetail::Clamp1(sum);
			}
			return sum;
		}
	}
}
