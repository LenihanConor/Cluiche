#pragma once

#include <math.h>
#include <float.h>
#include "DiaMaths/Core/MathsDefines.h"

namespace Dia
{
	namespace Maths
	{
		namespace Float
		{
			float	Max					();
			float	Min					();
			float	MinPositive			();
	
			float	FPower				(float number, float exponent);
			float	FPower				(float number, int exponent);
			float	FSquare				(float number);
			float	FSquareRoot			(float number);
			float	FLog				(float x);
			float	FLog10				(float x);
			float	FLogBase			(float x, float base);
			float	FExp				(float x);
			float	FAbs				(float x);
			float	FNegative			(float x);
			float	FFloor				(float x);
			float	FCeiling			(float x);
			float	FRound				(float x);
			float	FMod				(float x, float y);
			float	FSelect				(float compare, float x, float y);
			float	FTruncate			(float number, int decimel);

			bool FEqual					(float a, float b, float epsilon = FLOAT_EPSILON);
			bool FLess					(float a, float b, float epsilon = FLOAT_EPSILON);
			bool FGreater				(float a, float b, float epsilon = FLOAT_EPSILON);
			bool FLessEqual				(float a, float b, float epsilon = FLOAT_EPSILON);
			bool FGreaterEqual			(float a, float b, float epsilon = FLOAT_EPSILON);

			bool FEqualRelative			(float a, float b, float epsilon = FLOAT_EPSILON);
			bool FLessRelative			(float a, float b, float epsilon = FLOAT_EPSILON);
			bool FGreaterRelative		(float a, float b, float epsilon = FLOAT_EPSILON);
			bool FLessEqualRelative		(float a, float b, float epsilon = FLOAT_EPSILON);
			bool FGreaterEqualRelative	(float a, float b, float epsilon = FLOAT_EPSILON);

			bool FInRange				(float minVal, float maxVal, float f, float epsilon = FLOAT_EPSILON);
			bool FInRangeRelative		(float minVal, float maxVal, float f, float epsilon = FLOAT_EPSILON);

			bool FIsValid				(float val);
		}
	}
};

#include "DiaMaths/Core/FloatMaths.inl"
