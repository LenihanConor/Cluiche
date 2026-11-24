#pragma fp_contract(off)

#include "DiaMaths/Noise/Noise.h"
#include "DiaMaths/Noise/NoiseHash.h"

namespace Dia
{
	namespace Maths
	{
		float ValueNoise2D(uint32_t seed, float x, float y)
		{
			using namespace NoiseDetail;

			x = ClampCoord(x);
			y = ClampCoord(y);

			const int ix = FloorToInt(x);
			const int iy = FloorToInt(y);
			const float fx = x - static_cast<float>(ix);
			const float fy = y - static_cast<float>(iy);

			const float v00 = HashToSigned(Hash2D(seed, ix,     iy));
			const float v10 = HashToSigned(Hash2D(seed, ix + 1, iy));
			const float v01 = HashToSigned(Hash2D(seed, ix,     iy + 1));
			const float v11 = HashToSigned(Hash2D(seed, ix + 1, iy + 1));

			const float u = Fade(fx);
			const float v = Fade(fy);

			const float a = v00 + (v10 - v00) * u;
			const float b = v01 + (v11 - v01) * u;
			return Clamp1(a + (b - a) * v);
		}
	}
}
