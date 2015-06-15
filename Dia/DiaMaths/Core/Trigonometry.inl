
#include "DiaMaths/Core/MathsDefines.h"
#include "DiaMaths/Core/Angle.h"

#include <DiaCore/Core/Assert.h>

#include <math.h>

namespace Dia
{
	namespace Maths
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// Implementation
		//---------------------------------------------------------------------------------------------------------------------------------
		
		//convert degrees to radians
		//-----------------------------------------------------------------------------
		inline float DegToRadians(float angle)
		{
			return angle * DEGREETORADII;
		}

		//converts radians to degree
		//-----------------------------------------------------------------------------
		inline float RadiansToDeg(float angle)
		{
			return angle * RADIITODEGREE;
		}

		//-----------------------------------------------------------------------------
		inline float Cos (float radians)
		{
			return cosf(Dia::Maths::Angle(radians).AsRadians());
		}

		//-----------------------------------------------------------------------------
		inline float Sin (float radians)
		{
			return sinf(Dia::Maths::Angle(radians).AsRadians());
		}

		//-----------------------------------------------------------------------------
		inline float Tan (float radians)
		{
			return tanf(Dia::Maths::Angle(radians).AsRadians());
		}

		//-----------------------------------------------------------------------------
		inline float ACos(float radians)
		{
			DIA_ASSERT(radians >= -1.0f && radians <= 1.0f, "Outside range");

			return acosf(Dia::Maths::Angle(radians).AsRadians());
		}

		//-----------------------------------------------------------------------------
		inline float ASin(float radians)
		{
			DIA_ASSERT(radians >= -1.0f && radians <= 1.0f, "Outside range");

			return asinf(Dia::Maths::Angle(radians).AsRadians());
		}

		//-----------------------------------------------------------------------------
		inline float ATan(float radians)
		{
			DIA_ASSERT(radians >= -Dia::Maths::PI_HALF && radians <= Dia::Maths::PI_HALF, "Outside range");

			return atanf(Dia::Maths::Angle(radians).AsRadians());
		}

		//-----------------------------------------------------------------------------
		inline float Sin(const Angle& angle)
		{
			return Sin(angle.AsRadians());
		}
		
		//-----------------------------------------------------------------------------
		inline float Cos(const Angle& angle)
		{
			return Cos(angle.AsRadians());
		}
		
		//-----------------------------------------------------------------------------
		inline float Tan(const Angle& angle)
		{
			return Tan(angle.AsRadians());
		}
		
		//-----------------------------------------------------------------------------
		inline float ATan(const Angle& angle)
		{
			return ATan(angle.AsRadians());
		}
		
		//-----------------------------------------------------------------------------
		inline float ACos(const Angle& angle)
		{
			return ACos(angle.AsRadians());
		}
		
		//-----------------------------------------------------------------------------
		inline float ASin(const Angle& angle)
		{
			return ASin(angle.AsRadians());
		}
	}
}