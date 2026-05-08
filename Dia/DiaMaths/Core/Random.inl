namespace Dia
{
	namespace Maths
	{
		namespace Random
		{
			//---------------------------------------------------------------------------------------------------------------------------------
			// Template Implementation
			//---------------------------------------------------------------------------------------------------------------------------------

			// Shuffle array in place using Fisher-Yates algorithm
			// Time complexity: O(n)
			// Space complexity: O(1)
			template <typename T>
			inline void Shuffle(T* array, int count)
			{
				// Safety check for invalid input
				if (array == nullptr || count <= 1)
					return;

				// Fisher-Yates shuffle - unbiased random permutation
				// Loop backwards through array
				for (int i = count - 1; i > 0; --i)
				{
					// Pick random index from 0 to i (inclusive)
					int j = RandomRange(0, i);

					// Swap elements at i and j
					// This ensures each permutation has equal probability
					T temp = array[i];
					array[i] = array[j];
					array[j] = temp;
				}
			}
		}
	}
}
