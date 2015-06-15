
#include "Vector/VectorHalf2D.h"

#include "Core/CoreMaths.h"
#include "Core/Trigonometry.h"

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		VectorHalf2D&  VectorHalf2D::NormalizeSafe()
		{
			float t = Magnitude();
			if (t != 0.0f)
			{
				float mag = 1.0f / t;
				x *= mag;
				y *= mag;
			}
			else
			{
				Set(1.0f,0.0f);
			}
			return *this;
		}

		//-----------------------------------------------------------------------------
		VectorHalf2D  VectorHalf2D::AsNormalSafe() const
		{
			float t = Magnitude();
			VectorHalf2D result;
			if (t != 0.0f)
			{
				float mag = 1.0f / t;
				result.Set(x * mag, y * mag);
			}
			else
			{
				result.Set(1.0f,0.0f);
			}
			return result;
		}

		// -----------------------------------------------------------------------------
		VectorHalf2D VectorHalf2D::AsProjectOnTo( const VectorHalf2D& vec) const
		{
			VectorHalf2D a = Dot(vec);
			VectorHalf2D b = vec.Dot(vec);
			VectorHalf2D result = (a / b) * vec;
			return result;
		}

		// -----------------------------------------------------------------------------
		VectorHalf2D& VectorHalf2D::ProjectOnTo( const VectorHalf2D& vec) 
		{
			VectorHalf2D a = Dot(vec);
			VectorHalf2D b = vec.Dot(vec);
			Set((a / b) * vec);

			return *this;
		}
	}
}
