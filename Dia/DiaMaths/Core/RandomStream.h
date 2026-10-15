#pragma once

#include <DiaMaths/Vector/Vector2D.h>
#include <random>
#include <mutex>
#include <stdint.h>

namespace Dia
{
	namespace Maths
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// RandomStream - Independent Random Number Generator
		//
		// Provides a named, seedable random number generator with the same API as Random:: namespace.
		// Multiple streams can coexist independently, enabling deterministic multi-system gameplay.
		//
		// Thread Safety: Each stream has its own mutex, allowing concurrent access to different streams.
		//                Concurrent access to the SAME stream is safe but serialized.
		//
		// Usage:
		//   RandomStream stream(12345);           // Create with seed
		//   float r = stream.RandomUnit();        // Generate random float [0, 1]
		//   int dice = stream.RandomRange(1, 6);  // Generate random int [1, 6]
		//---------------------------------------------------------------------------------------------------------------------------------

		class RandomStream
		{
		public:
			// Construct a stream with the given seed
			// The seed is fixed at construction; Restore() exists for loading saved state only
			explicit RandomStream(unsigned int seed);

			// Returns random integer in range [0, INT_MAX]
			int RandomInt();

			// Returns random integer in range [min, max] (inclusive on both ends)
			// If min > max, they will be swapped automatically
			int RandomRange(int min, int max);

			// Returns random float in range [0.0, 1.0] (normalized random)
			float RandomUnit();

			// Returns random float in range [min, max]
			// If min > max, they will be swapped automatically
			float RandomRange(float min, float max);

			// Returns random float in range [-1.0, 1.0]
			float RandomBilateral();

			// Returns true with given probability [0.0, 1.0]
			bool RandomChance(float probability);

			// Returns random unit vector (normalized random direction)
			Vector2D RandomUnitVector();

			// Returns random index from array of weights (weighted random selection)
			// Higher weight = higher probability of selection
			// Returns -1 if total weight is 0 or input is invalid
			int WeightedRandom(const float* weights, int count);

			// Shuffle array in place using Fisher-Yates algorithm
			// Template implementation in RandomStream.inl
			template<typename T>
			void Shuffle(T* array, int count);

			// Seed given at construction (or by the last Restore)
			unsigned int GetSeed() const;

			// Number of raw generator invocations since seeding (not public API calls)
			uint64_t GetDrawCount() const;

			// Re-seed and fast-forward so subsequent draws match the original run.
			// For loading saved state only - not for gameplay re-seeding.
			void Restore(unsigned int seed, uint64_t drawCount);

		private:
			// URBG adapter that counts raw generator invocations
			struct CountingGenerator
			{
				using result_type = std::mt19937::result_type;
				static constexpr result_type min() { return std::mt19937::min(); }
				static constexpr result_type max() { return std::mt19937::max(); }
				result_type operator()() { ++mCount; return mEngine(); }

				std::mt19937 mEngine;
				uint64_t mCount = 0;
			};

			CountingGenerator mGenerator;  // Mersenne Twister generator (counted)
			unsigned int mSeed;            // Seed used to initialise mGenerator
			mutable std::mutex mMutex;     // Per-stream lock for thread safety
		};
	}
}

#include "DiaMaths/Core/RandomStream.inl"
