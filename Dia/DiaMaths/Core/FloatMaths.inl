#include "DiaMaths/Core/CoreMaths.h"

namespace Dia
{
	namespace Maths
	{
		namespace Float
		{
			//---------------------------------------------------------------------------------------------------------------------------------
			// Implementation
			//---------------------------------------------------------------------------------------------------------------------------------
			
			//-----------------------------------------------------------------------------
			inline float Max()
			{
				return FLT_MAX;
			}

			//-----------------------------------------------------------------------------
			inline float Min()
			{
				return -FLT_MAX;
			}

			//-----------------------------------------------------------------------------
			inline float MinPositive()
			{
				return FLT_MIN;
			}

			//-----------------------------------------------------------------------------
			inline float FPower(float number, float exponent)
			{
				return powf(number, exponent);
			}
			
			//-----------------------------------------------------------------------------
			inline float FPower(float number, int exponent)
			{
				return powf(number, static_cast<float>(exponent));
			}

			//-----------------------------------------------------------------------------
			inline float FSquare(float number)
			{
				return powf(number, 2);
			}

			//-----------------------------------------------------------------------------
			inline float FExp (float x)
			{
				return expf (x);
			}

			//-----------------------------------------------------------------------------
			inline float FAbs (float x)
			{
				return fabsf(x);
			}

			//-----------------------------------------------------------------------------
			inline float FNegative (float x)
			{
				return (-1.0f * fabsf(x));
			}

			//-----------------------------------------------------------------------------
			inline float FFloor (float x)
			{
				return floorf(x);
			}

			//-----------------------------------------------------------------------------
			inline float FCeiling (float f)
			{ 
				return ceil(f); 
			}
			
			//-----------------------------------------------------------------------------
			inline float FRound(float x)
			{
				return FFloor(x + 0.5f);
			}
			
			//-----------------------------------------------------------------------------
			inline float FMod(float x, float y)
			{
				return fmodf(x, y);
			}
			
			//-----------------------------------------------------------------------------
			inline float FSelect(float compare, float x, float y)
			{
				return (compare >= 0.0f) ? x : y;
			}

			//-----------------------------------------------------------------------------
			inline bool FEqual (float a, float b, float epsilon)	
			{ 
				float c = FAbs (a - b);
				return (c < epsilon); 
			}

			//-----------------------------------------------------------------------------
			inline bool FLess (float a, float b, float epsilon)	
			{ 
				return (a <= (b-epsilon)); 
			}

			//-----------------------------------------------------------------------------
			inline bool FGreater (float a, float b, float epsilon)	
			{ 
				return (a >= (b+epsilon)); 
			}

			//-----------------------------------------------------------------------------
			inline bool FLessEqual (float a, float b, float epsilon)	
			{ 
				return (a < (b+epsilon)); 
			}

			//-----------------------------------------------------------------------------
			inline bool FGreaterEqual (float a, float b, float epsilon)	
			{
				return (a > (b-epsilon)); 
			}

			//-----------------------------------------------------------------------------
			inline bool FIsValid(float val) 
			{
				return (val != static_cast<float>(0xFFFFFFFF));
			};
		}
	}
}