#pragma once

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		// Shuffle array in place using Fisher-Yates algorithm
		//
		// Algorithm:
		//   For each position i from end to start:
		//     Pick random position j in [0, i]
		//     Swap array[i] with array[j]
		//
		// This provides unbiased random permutation in O(n) time
		//-----------------------------------------------------------------------------
		template<typename T>
		void RandomStream::Shuffle(T* array, int count)
		{
			if (count <= 1 || array == nullptr)
			{
				return;
			}

			// Fisher-Yates shuffle
			for (int i = count - 1; i > 0; --i)
			{
				int j = RandomRange(0, i);

				// Swap array[i] with array[j]
				T temp = array[i];
				array[i] = array[j];
				array[j] = temp;
			}
		}
	}
}
