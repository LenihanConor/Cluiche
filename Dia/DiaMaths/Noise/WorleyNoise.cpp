#pragma fp_contract(off)

#include "DiaMaths/Noise/Noise.h"
#include "DiaMaths/Noise/NoiseHash.h"

#include <cmath>

namespace Dia
{
	namespace Maths
	{
		WorleyResult WorleyNoise2D(uint32_t seed, float x, float y, float jitter)
		{
			using namespace NoiseDetail;

			if (!(jitter > 0.0f)) jitter = 0.0f; // also maps NaN to 0
			if (jitter > 1.0f) jitter = 1.0f;

			x = ClampCoord(x);
			y = ClampCoord(y);

			const int ix = FloorToInt(x);
			const int iy = FloorToInt(y);
			const float fx = x - static_cast<float>(ix);
			const float fy = y - static_cast<float>(iy);

			float d1 = 3.0e38f;
			float d2 = 3.0e38f;
			uint32_t nearestHash = 0u;

			for (int oy = -2; oy <= 2; ++oy)
			{
				for (int ox = -2; ox <= 2; ++ox)
				{
					const uint32_t h = WorleyCellHash(seed, ix + ox, iy + oy);
					const float dx = (static_cast<float>(ox) + jitter * WorleyJitterX(h)) - fx;
					const float dy = (static_cast<float>(oy) + jitter * WorleyJitterY(h)) - fy;
					const float d = dx * dx + dy * dy;
					if (d < d1)
					{
						d2 = d1;
						d1 = d;
						nearestHash = h;
					}
					else if (d < d2)
					{
						d2 = d;
					}
				}
			}

			WorleyResult r;
			r.f1 = sqrtf(d1);
			r.f2 = sqrtf(d2);
			r.f2MinusF1 = r.f2 - r.f1;
			r.cellId = WorleyCellId(nearestHash);
			r.cellValue = HashToUnit(r.cellId);
			return r;
		}
	}
}
