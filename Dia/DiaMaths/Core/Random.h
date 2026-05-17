#pragma once

namespace Dia
{
	namespace Maths
	{
		class Vector2D;

		//---------------------------------------------------------------------------------------------------------------------------------
		// Random Number Generation
		//
		// High-quality random number utilities for game development.
		// Uses Mersenne Twister (std::mt19937) - much better than old rand()
		//
		// ✅ THREAD-SAFE: Each thread has its own generator (thread_local storage)
		//               Safe to call from any thread without synchronization
		//
		// QUALITY: Mersenne Twister has excellent statistical properties:
		//   - Period: 2^19937-1 (won't repeat for billions of years)
		//   - Passes statistical tests (unlike old rand())
		//   - Suitable for gameplay, procedural generation, simulations
		//   - NOT suitable for cryptography (use crypto libraries for that)
		//
		// PERFORMANCE: Fast (similar to rand()), no locking overhead
		//
		// USAGE:
		//   Random::Initialize();           // Optional, call at startup for seeding
		//   float r = Random::RandomUnit(); // Thread-safe, works from any thread
		//---------------------------------------------------------------------------------------------------------------------------------

		namespace Random
		{
			// Initialize random seed (optional - auto-initializes on first use if not called)
			// Parameters:
			//   seed - Random seed value, or 0 to use high-resolution clock (default)
			//
			// BEHAVIOR:
			//   - Sets seed for current thread immediately
			//   - Sets default seed for NEW threads created after this call
			//   - Existing threads (created before Initialize) keep their current state
			//
			// USE CASES:
			//   - Call with seed=0 for non-deterministic random (different each run)
			//   - Call with specific seed for reproducible results (testing, replays)
			//   - Don't call at all and it auto-seeds on first use
			//
			// THREAD SAFETY: Safe to call from any thread
			void Initialize(unsigned int seed = 0);

			// Returns random integer in range [0, INT_MAX]
			// Range: 0 to 2,147,483,647 (much larger than old RAND_MAX)
			// Usage: Basic random integer when you need a large range
			// Thread Safety: Safe to call from any thread
			int RandomInt();

			// Returns random integer in range [min, max] (inclusive on both ends)
			// Parameters:
			//   min - Minimum value (inclusive)
			//   max - Maximum value (inclusive)
			// Usage: RandomRange(1, 6) for simulating a dice roll
			// Note: If min > max, they will be swapped automatically
			// Thread Safety: Safe to call from any thread
			// Quality: Uniform distribution (no modulo bias like old rand())
			int RandomRange(int min, int max);

			// Returns random float in range [0.0, 1.0] (normalized random)
			// This is the most commonly used random function
			// Usage: RandomUnit() < 0.5f to get a 50% chance
			// Thread Safety: Safe to call from any thread
			// Quality: Uniform distribution, excellent statistical properties
			float RandomUnit();

			// Returns random float in range [min, max]
			// Parameters:
			//   min - Minimum value (inclusive)
			//   max - Maximum value (inclusive)
			// Usage: RandomRange(0.0f, 360.0f) for a random angle
			// Note: If min > max, they will be swapped automatically
			// Thread Safety: Safe to call from any thread
			float RandomRange(float min, float max);

			// Returns random float in range [-1.0, 1.0]
			// Useful for random directions or offsets that can go both ways
			// Usage: Vector2D randomOffset(RandomBilateral(), RandomBilateral())
			float RandomBilateral();

			// Returns true with given probability [0.0, 1.0]
			// Parameters:
			//   probability - Chance of returning true (0.0 = never, 1.0 = always)
			// Usage: if (RandomChance(0.1f)) { DropRareItem(); } // 10% drop chance
			bool RandomChance(float probability);

			// Returns random unit vector (normalized random direction)
			// Result has magnitude of 1.0, pointing in a random direction
			// Usage: Random movement direction, random force application
			//        Vector2D randomVelocity = RandomUnitVector() * speed;
			Vector2D RandomUnitVector();

			// Returns random index from array of weights (weighted random selection)
			// Higher weight = higher probability of selection
			// Parameters:
			//   weights - Array of weight values (must be >= 0)
			//   count   - Number of elements in weights array
			// Returns: Index of selected element, or -1 if total weight is 0
			// Example: weights = {10.0f, 30.0f, 60.0f}
			//          Result has 10% chance of 0, 30% chance of 1, 60% chance of 2
			// Usage: Loot drop tables, random enemy spawn selection
			int WeightedRandom(const float* weights, int count);

			// Shuffle array in place using Fisher-Yates algorithm
			// Provides unbiased random permutation in O(n) time
			// Parameters:
			//   array - Array to shuffle (modified in place)
			//   count - Number of elements in array
			// Usage: Randomizing card deck, randomizing level generation order
			// Note: Works with any type T that supports copying/assignment
			template <typename T>
			void Shuffle(T* array, int count);
		}
	}
}

#include "DiaMaths/Core/Random.inl"
