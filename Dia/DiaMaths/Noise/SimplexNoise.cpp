#pragma fp_contract(off)

#include "DiaMaths/Noise/Noise.h"
#include "DiaMaths/Noise/NoiseHash.h"

// SimplexNoise2D: OpenSimplex2S 2D (the smoother variant), re-implemented from the published OpenSimplex2 definition
// (KdotJPG, CC0 / public domain): skewed square lattice, the 4 nearest lattice points of a sample contribute
// (2/3 - d^2)^4 * dot(gradient, offset). No code or tables are taken from patented simplex implementations.
// Integer hashing from NoiseHash.h; float arithmetic limited to + - * so results are bit-identical on every build.

namespace Dia
{
	namespace Maths
	{
		namespace
		{
			// Skew F2 = (sqrt(3) - 1) / 2; unskew G2 = (3 - sqrt(3)) / 6.
			constexpr float kSkew2D   = 0.36602540378f;
			constexpr float kUnskew2D = -0.21132486540f; // -G2
			constexpr float kRSquared = 2.0f / 3.0f;

			// Published OpenSimplex2S 2D normaliser. VERIFIED: N2 = 0.05481866495625118 matches the OpenSimplex2 reference
			// (the reference divides its unit gradient set by N2); the measured maximum of this implementation is checked
			// against >= 0.9 in TestNoiseSimplex (AC11).
			constexpr double kN2 = 0.05481866495625118;
			constexpr float  kInvN2 = static_cast<float>(1.0 / kN2);

			// The published 24-direction 2D gradient set (unit vectors): 8 at 22.5 + 45k degrees, 16 at 7.5/37.5/52.5/82.5 degrees
			// (+ quadrant mirrors). Written as float literals; no sin/cos.
			constexpr float kSimplexGradients2D[24][2] =
			{
				{  0.38268343f,  0.92387953f }, {  0.92387953f,  0.38268343f }, {  0.92387953f, -0.38268343f }, {  0.38268343f, -0.92387953f },
				{ -0.38268343f, -0.92387953f }, { -0.92387953f, -0.38268343f }, { -0.92387953f,  0.38268343f }, { -0.38268343f,  0.92387953f },
				{  0.13052619f,  0.99144486f }, {  0.60876143f,  0.79335334f }, {  0.79335334f,  0.60876143f }, {  0.99144486f,  0.13052619f },
				{  0.99144486f, -0.13052619f }, {  0.79335334f, -0.60876143f }, {  0.60876143f, -0.79335334f }, {  0.13052619f, -0.99144486f },
				{ -0.13052619f, -0.99144486f }, { -0.60876143f, -0.79335334f }, { -0.79335334f, -0.60876143f }, { -0.99144486f, -0.13052619f },
				{ -0.99144486f,  0.13052619f }, { -0.79335334f,  0.60876143f }, { -0.60876143f,  0.79335334f }, { -0.13052619f,  0.99144486f },
			};

			// One lattice point: (2/3 - d^2)^4 * dot(grad, d) when inside the radius, else 0.
			inline float Contribution(uint32_t seed, int ix, int iy, float dx, float dy)
			{
				const float a = kRSquared - dx * dx - dy * dy;
				if (!(a > 0.0f))
				{
					return 0.0f;
				}
				// Uniform index in [0, 24) from 24 hash bits: exact integer arithmetic.
				const uint32_t h = NoiseDetail::Hash2D(seed, ix, iy);
				const uint32_t gi = static_cast<uint32_t>((static_cast<uint64_t>(h >> 8) * 24u) >> 24);
				const float a2 = a * a;
				return (a2 * a2) * (kSimplexGradients2D[gi][0] * dx + kSimplexGradients2D[gi][1] * dy);
			}
		}

		float SimplexNoise2D(uint32_t seed, float x, float y)
		{
			using namespace NoiseDetail;

			x = ClampCoord(x);
			y = ClampCoord(y);

			// Skew into the lattice where the 4 candidate points form unit-square corners.
			const float s = kSkew2D * (x + y);
			const float xs = x + s;
			const float ys = y + s;

			const int xsb = FloorToInt(xs);
			const int ysb = FloorToInt(ys);
			const float xsi = xs - static_cast<float>(xsb);
			const float ysi = ys - static_cast<float>(ysb);

			// Unskew the base point back to input space; (dx0, dy0) is the offset from the base lattice point.
			const float t = (xsi + ysi) * kUnskew2D;
			const float dx0 = xsi + t;
			const float dy0 = ysi + t;

			// Offsets of the lattice step vectors in input space.
			const float u1 = 1.0f + 2.0f * kUnskew2D;      // (1,1) step
			const float u0 = kUnskew2D;                    // (0,1) / (1,0) step shared part

			float value = Contribution(seed, xsb, ysb, dx0, dy0);
			value += Contribution(seed, xsb + 1, ysb + 1, dx0 - u1, dy0 - u1);

			// The two remaining points depend on the sample's position inside the lattice cell.
			const float xmyi = xsi - ysi;
			if (t < kUnskew2D)
			{
				if (xsi + xmyi > 1.0f)
					value += Contribution(seed, xsb + 2, ysb + 1, dx0 - (3.0f * kUnskew2D + 2.0f), dy0 - (3.0f * kUnskew2D + 1.0f));
				else
					value += Contribution(seed, xsb, ysb + 1, dx0 - u0, dy0 - (u0 + 1.0f));

				if (ysi - xmyi > 1.0f)
					value += Contribution(seed, xsb + 1, ysb + 2, dx0 - (3.0f * kUnskew2D + 1.0f), dy0 - (3.0f * kUnskew2D + 2.0f));
				else
					value += Contribution(seed, xsb + 1, ysb, dx0 - (u0 + 1.0f), dy0 - u0);
			}
			else
			{
				if (xsi + xmyi < 0.0f)
					value += Contribution(seed, xsb - 1, ysb, dx0 + (1.0f + u0), dy0 + u0);
				else
					value += Contribution(seed, xsb + 1, ysb, dx0 - (u0 + 1.0f), dy0 - u0);

				if (ysi < xmyi)
					value += Contribution(seed, xsb, ysb - 1, dx0 + u0, dy0 + (u0 + 1.0f));
				else
					value += Contribution(seed, xsb, ysb + 1, dx0 - u0, dy0 - (u0 + 1.0f));
			}

			return Clamp1(value * kInvN2);
		}
	}
}
