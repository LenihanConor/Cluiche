#include "DiaMaths/Core/Random.h"

#include "DiaMaths/Core/CoreMaths.h"
#include "DiaMaths/Core/Trigonometry.h"
#include "DiaMaths/Vector/Vector2D.h"
#include "DiaMaths/Shape/2D/Circle2D.h"
#include "DiaMaths/Shape/2D/AARect2D.h"

#include <random>
#include <chrono>

namespace Dia
{
	namespace Maths
	{
		namespace Random
		{
			//---------------------------------------------------------------------------------------------------------------------------------
			// Thread-Safe Implementation using C++11 <random>
			//
			// Uses std::mt19937 (Mersenne Twister) - higher quality than rand()
			// Each thread has its own generator (thread_local) - no synchronization needed
			// Much better statistical properties than old rand()
			//---------------------------------------------------------------------------------------------------------------------------------

			// Thread-local random engine - each thread gets its own independent generator
			// This provides thread safety without any locking overhead
			static thread_local std::mt19937* s_generator = nullptr;

			// Flag to track if global seed was set (for reproducibility)
			static unsigned int s_globalSeed = 0;
			static bool s_globalSeedSet = false;

			//-----------------------------------------------------------------------------
			// Internal: Get or create thread-local generator
			static std::mt19937& GetGenerator()
			{
				if (s_generator == nullptr)
				{
					// First access on this thread - create generator
					unsigned int seed;

					if (s_globalSeedSet)
					{
						// Use global seed (for reproducibility)
						seed = s_globalSeed;
					}
					else
					{
						// Use high-resolution clock for unique seed
						auto now = std::chrono::high_resolution_clock::now();
						seed = static_cast<unsigned int>(now.time_since_epoch().count());
					}

					s_generator = new std::mt19937(seed);
				}

				return *s_generator;
			}

			//-----------------------------------------------------------------------------
			// Initialize the random number generator with a seed
			// If seed is 0, uses high-resolution clock for non-deterministic randomness
			// NOTE: This sets the seed for the CALLING THREAD and any NEW threads created after
			//       Existing threads keep their current generator state
			void Initialize(unsigned int seed)
			{
				if (seed == 0)
				{
					// Use high-resolution clock for better entropy than time()
					auto now = std::chrono::high_resolution_clock::now();
					seed = static_cast<unsigned int>(now.time_since_epoch().count());
					s_globalSeedSet = false;
				}
				else
				{
					// User provided seed - store for reproducibility
					s_globalSeed = seed;
					s_globalSeedSet = true;
				}

				// Reset current thread's generator
				if (s_generator != nullptr)
				{
					delete s_generator;
					s_generator = nullptr;
				}
			}

			//-----------------------------------------------------------------------------
			int RandomInt()
			{
				std::uniform_int_distribution<int> dist(0, INT_MAX);
				return dist(GetGenerator());
			}

			//-----------------------------------------------------------------------------
			int RandomRange(int min, int max)
			{
				if (min > max)
				{
					Swap(min, max);
				}

				if (min == max)
				{
					return min;
				}

				std::uniform_int_distribution<int> dist(min, max);
				return dist(GetGenerator());
			}

			//-----------------------------------------------------------------------------
			float RandomUnit()
			{
				std::uniform_real_distribution<float> dist(0.0f, 1.0f);
				return dist(GetGenerator());
			}

			//-----------------------------------------------------------------------------
			float RandomRange(float min, float max)
			{
				if (min > max)
				{
					Swap(min, max);
				}

				float t = RandomUnit();
				return min + (max - min) * t;
			}

			//-----------------------------------------------------------------------------
			float RandomBilateral()
			{
				return RandomRange(-1.0f, 1.0f);
			}

			//-----------------------------------------------------------------------------
			bool RandomChance(float probability)
			{
				probability = Clamp(probability, 0.0f, 1.0f);
				return RandomUnit() < probability;
			}

			//-----------------------------------------------------------------------------
			// Generate random point inside circle with uniform distribution
			// IMPORTANT: We use sqrt(random) for radius to get uniform distribution!
			//
			// Why sqrt? Area increases with r^2, so:
			//   - Without sqrt: points cluster near center (linear radius sampling)
			//   - With sqrt: uniform distribution across the entire circle area
			//
			// Algorithm:
			//   1. Random angle (0 to 2π)
			//   2. Random radius using sqrt(random) for uniform density
			//   3. Convert polar coordinates (r, θ) to Cartesian (x, y)
			Vector2D RandomPointInCircle(const Circle2D& circle)
			{
				float angle = RandomRange(0.0f, PI_2);
				float radius = Dia::Maths::SquareRoot(RandomUnit()) * circle.GetRadius();

				// Convert polar to Cartesian: x = r*cos(θ), y = r*sin(θ)
				float cosAngle = Dia::Maths::Cos(angle);
				float sinAngle = Dia::Maths::Sin(angle);

				return circle.GetCenter() + Vector2D(cosAngle * radius, sinAngle * radius);
			}

			//-----------------------------------------------------------------------------
			Vector2D RandomPointOnCircle(const Circle2D& circle)
			{
				float angle = RandomRange(0.0f, PI_2);
				float radius = circle.GetRadius();

				float cosAngle = Dia::Maths::Cos(angle);
				float sinAngle = Dia::Maths::Sin(angle);

				return circle.GetCenter() + Vector2D(cosAngle * radius, sinAngle * radius);
			}

			//-----------------------------------------------------------------------------
			Vector2D RandomPointInRect(const AARect2D& rect)
			{
				const Vector2D& bottomLeft = rect.GetBottomLeft();
				const Vector2D& topRight = rect.GetTopRight();

				float x = RandomRange(bottomLeft.x, topRight.x);
				float y = RandomRange(bottomLeft.y, topRight.y);

				return Vector2D(x, y);
			}

			//-----------------------------------------------------------------------------
			Vector2D RandomUnitVector()
			{
				float angle = RandomRange(0.0f, PI_2);
				float cosAngle = Dia::Maths::Cos(angle);
				float sinAngle = Dia::Maths::Sin(angle);

				return Vector2D(cosAngle, sinAngle);
			}

			//-----------------------------------------------------------------------------
			// Weighted random selection from an array of weights
			//
			// Algorithm:
			//   1. Sum all weights to get total
			//   2. Pick random value in [0, total]
			//   3. Walk through weights until we exceed the random value
			//
			// Example: weights = {10, 30, 60}, total = 100
			//   Random value 0-10   → returns 0
			//   Random value 10-40  → returns 1
			//   Random value 40-100 → returns 2
			//
			// This gives each element probability proportional to its weight
			int WeightedRandom(const float* weights, int count)
			{
				if (count <= 0 || weights == nullptr)
				{
					return -1;
				}

				// Calculate total weight (sum of all weights)
				float totalWeight = 0.0f;
				for (int i = 0; i < count; ++i)
				{
					totalWeight += weights[i];
				}

				// If total weight is 0 or negative, selection is impossible
				if (totalWeight <= 0.0f)
				{
					return -1;
				}

				// Pick random value in [0, totalWeight]
				float randomValue = RandomRange(0.0f, totalWeight);

				// Walk through weights, accumulating until we pass randomValue
				// The index where we pass it is our selected element
				float currentWeight = 0.0f;
				for (int i = 0; i < count; ++i)
				{
					currentWeight += weights[i];
					if (randomValue <= currentWeight)
					{
						return i;
					}
				}

				// Floating point rounding might cause us to miss by tiny amount
				// Return last index as safe fallback
				return count - 1;
			}
		}
	}
}
