
#include "DiaMaths/Core/FloatMaths.h"

#include "DiaMaths/Core/MathsDefines.h"

#include <DiaCore/Core/Assert.h>
#include <math.h>

namespace Dia
{
	namespace Maths
	{
		namespace Float
		{
			//-----------------------------------------------------------------------------
			float FSquareRoot(float number)
			{
				DIA_ASSERT (number >= 0, "Can not sqrt a negative");

				return sqrtf(number);
			}

			//-----------------------------------------------------------------------------
			float FLog (float x)
			{
				DIA_ASSERT(x > 0, "No Log of 0");

				return logf(x);
			}

			//-----------------------------------------------------------------------------
			float FLog10 (float x)
			{
				DIA_ASSERT(x > 0, "No Log of 0");

				return log10f(x);
			}
			
			//-----------------------------------------------------------------------------
			float FLogBase (float x, float base)
			{
				DIA_ASSERT(x > 0, "No Log of 0");
				DIA_ASSERT(base > 0, "No Log of 0");

				return logf(x) / logf(base);
			}
		
			//-----------------------------------------------------------------------------
			float FTruncate( float number, int decimel )
			{
				DIA_ASSERT(decimel >= 0, "Must be positive");

				int multiple = 1;
				for( int i = 0; i < decimel; i++ )
				{
					multiple *= 10;
				}
				
				float sign = Dia::Maths::Sign(number);
				float temp = FFloor( sign * number * multiple ) / multiple;
				
				return sign * temp;
			}

			//-----------------------------------------------------------------------------
			bool FEqualRelative (float a, float b, float epsilon)	
			{ 
				float eps = epsilon * Dia::Maths::Max<float>(FAbs(a),FAbs(b)); 
				return (a >= (b-eps)) && (a <= (b+eps)); 
			}

			//-----------------------------------------------------------------------------
			bool FLessRelative (float a, float b, float epsilon)	
			{ 
				float eps = epsilon * Dia::Maths::Max<float>(FAbs(a),FAbs(b)); 
				return (a <= (b-eps)); 
			}

			//-----------------------------------------------------------------------------
			bool FGreaterRelative (float a, float b, float epsilon)	
			{ 
				float eps = epsilon * Dia::Maths::Max<float>(FAbs(a),FAbs(b)); 
				return (a >= (b+eps)); 
			}

			//-----------------------------------------------------------------------------
			bool FLessEqualRelative (float a, float b, float epsilon)	
			{ 
				float eps = epsilon * Dia::Maths::Max<float>(FAbs(a),FAbs(b)); 
				return (a <= (b+eps)); 
			}

			//-----------------------------------------------------------------------------
			bool FGreaterEqualRelative (float a, float b, float epsilon)	
			{ 
				float eps = epsilon * Dia::Maths::Max<float>(FAbs(a),FAbs(b)); 
				return (a >= (b-eps)); 
			}

			//-----------------------------------------------------------------------------
			bool FInRange (float minVal, float maxVal, float f, float epsilon)	
			{ 
				return (FGreaterEqual(f, minVal, epsilon) && FLessEqual(f, maxVal, epsilon)); 
			}

			//-----------------------------------------------------------------------------
			bool FInRangeRelative (float minVal, float maxVal, float f, float epsilon)	
			{ 
				return (FGreaterEqualRelative(f, minVal, epsilon) && FLessEqualRelative(f, maxVal, epsilon)); 
			}	
		}
	}
}