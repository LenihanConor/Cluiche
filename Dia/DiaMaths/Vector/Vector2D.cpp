
#include "Vector/Vector2D.h"

#include "Core/CoreMaths.h"
#include "Core/FloatMaths.h"
#include "Core/Trigonometry.h"
#include "Matrix/Matrix22.h"
#include "DiaCore/Type/TypeDefinitionMacros.h"

namespace Dia
{
	namespace Maths
	{
		DIA_TYPE_DEFINITION( Vector2D )
			DIA_TYPE_ADD_VARIABLE( "x", x )
			DIA_TYPE_ADD_VARIABLE( "y", y )
		DIA_TYPE_DEFINITION_END()
		
		//-----------------------------------------------------------------------------
		Vector2D&  Vector2D::NormalizeSafe()
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
		Vector2D  Vector2D::AsNormalSafe() const
		{
			float t = Magnitude();
			Vector2D result;
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
		Vector2D Vector2D::AsProjectOnTo( const Vector2D& vec) const
		{
			Vector2D a = Dot(vec);
			Vector2D b = vec.Dot(vec);
			Vector2D result = (a / b) * vec;
			return result;
		}

		// -----------------------------------------------------------------------------
		Vector2D& Vector2D::ProjectOnTo( const Vector2D& vec) 
		{
			Vector2D a = Dot(vec);
			Vector2D b = vec.Dot(vec);
			Set((a / b) * vec);

			return *this;
		}
	
		//----------------------------------------------------------------------------
		Vector2D Vector2D::AsRotateClockwiseBy(const Angle& angle) const
		{
			DIA_ASSERT(*this != Zero(), "Cant rotate a zero vector");

			Vector2D newVector;

			float cosOfAngle = Dia::Maths::Cos(-angle);
			float sinOfAngle = Dia::Maths::Sin(-angle);

			newVector.x = (x * cosOfAngle) - (y * sinOfAngle);
			newVector.y = (y * cosOfAngle) + (x * sinOfAngle);

			return newVector;
		}

		//-----------------------------------------------------------------------------
		Vector2D& Vector2D::RotateClockwiseBy(const Angle& angle)
		{
			DIA_ASSERT(*this != Zero(), "Cant rotate a zero vector");

			float cosOfAngle = Dia::Maths::Cos(-angle);
			float sinOfAngle = Dia::Maths::Sin(-angle);
			
			float tempX = x;
			float tempY = y;

			x = (tempX * cosOfAngle) - (tempY * sinOfAngle);
			y = (tempY * cosOfAngle) + (tempX * sinOfAngle);

			return *this;
		}
			
		//----------------------------------------------------------------------------
		Vector2D Vector2D::AsRotateCounterClockwiseBy(const Angle& angle) const
		{
			DIA_ASSERT(*this != Zero(), "Cant rotate a zero vector");

			Vector2D newVector;

			float cosOfAngle = Dia::Maths::Cos(angle);
			float sinOfAngle = Dia::Maths::Sin(angle);

			newVector.x = (x * cosOfAngle) - (y * sinOfAngle);
			newVector.y = (y * cosOfAngle) + (x * sinOfAngle);

			return newVector;
		}

		//-----------------------------------------------------------------------------
		Vector2D& Vector2D::RotateCounterClockwiseBy(const Angle& angle)
		{
			DIA_ASSERT(*this != Zero(), "Cant rotate a zero vector");

			float cosOfAngle = Dia::Maths::Cos(angle);
			float sinOfAngle = Dia::Maths::Sin(angle);
			
			float tempX = x;
			float tempY = y;

			x = (tempX * cosOfAngle) - (tempY * sinOfAngle);
			y = (tempY * cosOfAngle) + (tempX * sinOfAngle);

			return *this;
		}

		//-----------------------------------------------------------------------------
		Vector2D Vector2D::AsMultipliedBy( const Matrix22& matrix) const
		{
			Vector2D newVector;

			newVector.x = ((matrix[0] * this->x) + (matrix[2] * this->y));
			newVector.y = ((matrix[1] * this->x) + (matrix[3] * this->y));
			
			return newVector;
		}

		//-----------------------------------------------------------------------------
		Vector2D& Vector2D::MultipliedBy(const Matrix22& matrix)
		{
			Vector2D newVector;

			newVector.x = ((matrix[0] * this->x) + (matrix[2] * this->y));
			newVector.y = ((matrix[1] * this->x) + (matrix[3] * this->y));
			
			this->x = newVector.x;
			this->y = newVector.y;

			return *this;
		}

		//-----------------------------------------------------------------------------
		void Vector2D::GetAngleBetween(const Vector2D& vec, Angle& result)const
		{
			DIA_ASSERT(*this != Zero(), "Cant rotate a zero vector");
			DIA_ASSERT(vec != Zero(), "Cant rotate a zero vector");
			
			DIA_ASSERT(this->IsNormal(), "Must be normal");
			DIA_ASSERT(vec.IsNormal(), "Must be normal");

			float fDot = Dot(vec);
			
			if (IsParallelTo(vec))
			{
				fDot = 1.0f * Dia::Maths::Sign(fDot);
			}

			result = Dia::Maths::Angle(Dia::Maths::ACos(Dia::Maths::Angle(fDot)));
			
			DIA_ASSERT( result.AsRadians() >= 0.0f && Dia::Maths::Float::FLessEqual( result.AsRadians(), Dia::Maths::PI ), " Incorrect angle"  );
		}
		
		//-----------------------------------------------------------------------------
		void Vector2D::GetClockwiseAngleBetween	(const Vector2D& vec, Angle& result)const
		{
			DIA_ASSERT(*this != Zero(), "Cant rotate a zero vector");
			DIA_ASSERT(vec != Zero(), "Cant rotate a zero vector");
			
			DIA_ASSERT(this->IsNormal(), "Must be normal");
			DIA_ASSERT(vec.IsNormal(), "Must be normal");

			float fDot = Dot(vec);

			bool isParrallel = IsParallelTo(vec);

			if (isParrallel)
			{
				fDot = 1.0f * Dia::Maths::Sign(fDot);
			}

			result = Dia::Maths::Angle(Dia::Maths::ACos(Dia::Maths::Angle(fDot)));

			if (!isParrallel && IsCounterClockwiseTo(vec))
			{
				result = Dia::Maths::Angle::Deg360 - result;
			}
		}

		//-----------------------------------------------------------------------------
		void Vector2D::GetCounterClockwiseAngleBetween	(const Vector2D& vec, Angle& result)const
		{
			DIA_ASSERT(*this != Zero(), "Cant rotate a zero vector");
			DIA_ASSERT(vec != Zero(), "Cant rotate a zero vector");
			
			DIA_ASSERT(this->IsNormal(), "Must be normal");
			DIA_ASSERT(vec.IsNormal(), "Must be normal");

			float fDot = Dot(vec);

			bool isParrallel = IsParallelTo(vec);

			if (isParrallel)
			{
				fDot = 1.0f * Dia::Maths::Sign(fDot);
			}

			result = Dia::Maths::Angle(Dia::Maths::ACos(Dia::Maths::Angle(fDot)));

			if (!isParrallel && IsClockwiseTo(vec))
			{
				result = Dia::Maths::Angle::Deg360 - result;
			}
		}

		void Vector2D::GetRotationBetween(const Vector2D& vec, Matrix22& result)const
		{
			Angle angle;
			GetClockwiseAngleBetween(vec, angle);
			
			result = Matrix22::FromAngleCounterClockwise(angle);
		}
	}
}
