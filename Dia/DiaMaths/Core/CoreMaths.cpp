
#include "DiaMaths/Core/CoreMaths.h"

#include "DiaMaths/Core/MathsDefines.h"

#include <DiaCore/Core/Assert.h>
#include <math.h>

namespace Dia
{
	namespace Maths
	{
		//returns the square root of a number
		//-----------------------------------------------------------------------------
		float SquareRoot(const float& number)
		{
			DIA_ASSERT (number >= 0, "Can not sqrt a negative");

			return sqrtf(number);
		}

		//returns the factorial to it
		//-----------------------------------------------------------------------------
		int Factorial(const int& number)
		{
			DIA_ASSERT (number >= 0, "Can not sqrt a negative");
			
			if (number == 0)
			{
				return 1;
			}

			int number2 = 0;
			int numberx = number;
			for(int i = 0; i<(number-1); i++ )
			{
				number2 = number - (i+1);
				numberx *= number2;
				number2 = numberx;	
			}

			return numberx;
		}
	}
}