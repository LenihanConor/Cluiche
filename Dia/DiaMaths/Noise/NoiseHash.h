#pragma once

#include <cstdint>

namespace Dia
{
	namespace Maths
	{
		namespace NoiseDetail
		{
			//---------------------------------------------------------------------------------------------------------------------------------
			// Integer hashing and exact conversions shared by the noise implementations.
			// Integers only; float arithmetic limited to + - * so results are bit-identical on every build.
			//---------------------------------------------------------------------------------------------------------------------------------

			// Largest |coordinate| the lattice floor handles; inputs beyond are clamped.
			constexpr float kNoiseCoordLimit = 8388608.0f; // 2^23

			// 32-bit integer mix (lowbias32-style multiply/xorshift finaliser).
			inline uint32_t Mix32(uint32_t h)
			{
				h ^= h >> 16;
				h *= 0x7FEB352Du;
				h ^= h >> 15;
				h *= 0x846CA68Bu;
				h ^= h >> 16;
				return h;
			}

			inline uint32_t Hash2D(uint32_t seed, int ix, int iy)
			{
				uint32_t h = Mix32(seed ^ 0xA511E9B3u);
				h = Mix32(h ^ (static_cast<uint32_t>(ix) * 0x27D4EB2Fu));
				h = Mix32(h ^ (static_cast<uint32_t>(iy) * 0x165667B1u));
				return h;
			}

			// Exact: (h >> 8) is < 2^24, times 2^-24 gives [0, 1) with no rounding.
			inline float HashToUnit(uint32_t h)
			{
				return static_cast<float>(h >> 8) * 5.9604644775390625e-8f; // 2^-24
			}

			// Exact: [-1, 1) in steps of 2^-23.
			inline float HashToSigned(uint32_t h)
			{
				return static_cast<float>(h >> 8) * 1.1920928955078125e-7f - 1.0f; // 2^-23
			}

			// Clamp a coordinate to +-2^23 (keeps fractional parts and the lattice floor well defined). Comparisons only.
			inline float ClampCoord(float v)
			{
				if (v > kNoiseCoordLimit) v = kNoiseCoordLimit;
				if (v < -kNoiseCoordLimit) v = -kNoiseCoordLimit;
				return v;
			}

			// floor() via integer cast; the caller passes a ClampCoord()ed value.
			inline int FloorToInt(float v)
			{
				int i = static_cast<int>(v);
				if (static_cast<float>(i) > v) --i;
				return i;
			}

			// Quintic fade 6t^5 - 15t^4 + 10t^3, fixed evaluation order.
			inline float Fade(float t)
			{
				return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
			}

			inline float Clamp1(float v)
			{
				if (v > 1.0f) return 1.0f;
				if (v < -1.0f) return -1.0f;
				return v;
			}

			// 16 unit gradients at 22.5 degree steps (float literals, no sin/cos).
			constexpr float kGradients2D[16][2] =
			{
				{  1.0f,           0.0f           },
				{  0.92387953f,    0.38268343f    },
				{  0.70710678f,    0.70710678f    },
				{  0.38268343f,    0.92387953f    },
				{  0.0f,           1.0f           },
				{ -0.38268343f,    0.92387953f    },
				{ -0.70710678f,    0.70710678f    },
				{ -0.92387953f,    0.38268343f    },
				{ -1.0f,           0.0f           },
				{ -0.92387953f,   -0.38268343f    },
				{ -0.70710678f,   -0.70710678f    },
				{ -0.38268343f,   -0.92387953f    },
				{  0.0f,          -1.0f           },
				{  0.38268343f,   -0.92387953f    },
				{  0.70710678f,   -0.70710678f    },
				{  0.92387953f,   -0.38268343f    },
			};

			// Index uses the top 4 hash bits.
			inline uint32_t GradientIndex2D(uint32_t h)
			{
				return h >> 28;
			}

			inline void Gradient2D(uint32_t h, float& gx, float& gy)
			{
				const uint32_t i = GradientIndex2D(h);
				gx = kGradients2D[i][0];
				gy = kGradients2D[i][1];
			}

			// Feature point convention: the point of cell (cx, cy) is (cx + jitter * ux, cy + jitter * uy) with ux, uy in [0, 1)
			// derived from the cell hash. jitter = 0 therefore places every feature point exactly on the integer cell corner (a regular
			// grid), which makes f1 == 0 exactly when the sample sits on an integer corner. Distances are taken from the sample's cell
			// origin: dx = (ox + jitter * ux) - fx, with ox the cell offset in the search window and fx the sample's fraction in its cell.
			inline uint32_t WorleyCellHash(uint32_t seed, int cx, int cy)
			{
				return Hash2D(seed, cx, cy);
			}

			inline float WorleyJitterX(uint32_t cellHash)
			{
				return HashToUnit(cellHash);
			}

			inline float WorleyJitterY(uint32_t cellHash)
			{
				return HashToUnit(Mix32(cellHash ^ 0x68E31DA4u));
			}

			// Id is decorrelated from the jitter bits.
			inline uint32_t WorleyCellId(uint32_t cellHash)
			{
				return Mix32(cellHash + 0x9E3779B9u);
			}
		}
	}
}
