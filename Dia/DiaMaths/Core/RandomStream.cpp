#include "DiaMaths/Core/RandomStream.h"
#include "DiaMaths/Core/CoreMaths.h"
#include "DiaMaths/Core/Trigonometry.h"

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		// Constructor - seeds the generator
		RandomStream::RandomStream(unsigned int seed)
			: mGenerator(seed)
		{
		}

		//-----------------------------------------------------------------------------
		int RandomStream::RandomInt()
		{
			std::lock_guard<std::mutex> lock(mMutex);
			std::uniform_int_distribution<int> dist(0, INT_MAX);
			return dist(mGenerator);
		}

		//-----------------------------------------------------------------------------
		int RandomStream::RandomRange(int min, int max)
		{
			if (min > max)
			{
				Swap(min, max);
			}

			if (min == max)
			{
				return min;
			}

			std::lock_guard<std::mutex> lock(mMutex);
			std::uniform_int_distribution<int> dist(min, max);
			return dist(mGenerator);
		}

		//-----------------------------------------------------------------------------
		float RandomStream::RandomUnit()
		{
			std::lock_guard<std::mutex> lock(mMutex);
			std::uniform_real_distribution<float> dist(0.0f, 1.0f);
			return dist(mGenerator);
		}

		//-----------------------------------------------------------------------------
		float RandomStream::RandomRange(float min, float max)
		{
			if (min > max)
			{
				Swap(min, max);
			}

			float t = RandomUnit();
			return min + (max - min) * t;
		}

		//-----------------------------------------------------------------------------
		float RandomStream::RandomBilateral()
		{
			return RandomRange(-1.0f, 1.0f);
		}

		//-----------------------------------------------------------------------------
		bool RandomStream::RandomChance(float probability)
		{
			probability = Clamp(probability, 0.0f, 1.0f);
			return RandomUnit() < probability;
		}

		//-----------------------------------------------------------------------------
		Vector2D RandomStream::RandomUnitVector()
		{
			float angle = RandomRange(0.0f, PI_2);
			float cosAngle = Dia::Maths::Cos(angle);
			float sinAngle = Dia::Maths::Sin(angle);

			return Vector2D(cosAngle, sinAngle);
		}

		//-----------------------------------------------------------------------------
		int RandomStream::WeightedRandom(const float* weights, int count)
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
