#pragma once

#include <DiaMaths/Vector/Vector2D.h>
#include <random>
#include <mutex>

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
			// The seed is immutable after construction (no re-seeding)
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

		private:
			std::mt19937 mGenerator;  // Mersenne Twister generator
			std::mutex mMutex;        // Per-stream lock for thread safety
		};
	}
}

#include "DiaMaths/Core/RandomStream.inl"
