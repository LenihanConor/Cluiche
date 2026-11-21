#pragma fp_contract(off)

#include "DiaMaths/Noise/Noise.h"
#include "DiaMaths/Noise/NoiseHash.h"

namespace Dia
{
	namespace Maths
	{
		namespace
		{
			// Max |sum| for unit gradients is sqrt(2)/2; scale so the extreme reaches ~1.
			constexpr float kGradientScale2D = 1.4142135f;

			inline float Corner(uint32_t seed, int ix, int iy, float dx, float dy)
			{
				float gx, gy;
				NoiseDetail::Gradient2D(NoiseDetail::Hash2D(seed, ix, iy), gx, gy);
				return gx * dx + gy * dy;
			}
		}

		float GradientNoise2D(uint32_t seed, float x, float y)
		{
			using namespace NoiseDetail;

			x = ClampCoord(x);
			y = ClampCoord(y);

			const int ix = FloorToInt(x);
			const int iy = FloorToInt(y);
			const float fx = x - static_cast<float>(ix);
			const float fy = y - static_cast<float>(iy);

			const float n00 = Corner(seed, ix,     iy,     fx,        fy);
			const float n10 = Corner(seed, ix + 1, iy,     fx - 1.0f, fy);
			const float n01 = Corner(seed, ix,     iy + 1, fx,        fy - 1.0f);
			const float n11 = Corner(seed, ix + 1, iy + 1, fx - 1.0f, fy - 1.0f);

			const float u = Fade(fx);
			const float v = Fade(fy);

			const float a = n00 + (n10 - n00) * u;
			const float b = n01 + (n11 - n01) * u;
			return Clamp1((a + (b - a) * v) * kGradientScale2D);
		}
	}
}
