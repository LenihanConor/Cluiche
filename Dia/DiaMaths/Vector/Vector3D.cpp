
#include "Vector/Vector3D.h"

#include "float.h"

#include "Core/CoreMaths.h"
#include "Core/FloatMaths.h"

#include <DiaCore/Core/Assert.h>

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		const Vector3D& Vector3D::XAxis()
		{
			static const Vector3D xAxis( 1.0f, 0.0f, 0.0f );
			return xAxis;
		}
		 
		//-----------------------------------------------------------------------------
        const Vector3D& Vector3D::YAxis()
			{
			static const Vector3D yAxis( 0.0f, 1.0f, 0.0f );
			return yAxis;
		}

		//-----------------------------------------------------------------------------
        const Vector3D& Vector3D::ZAxis()
			{
			static const Vector3D yAxis( 0.0f, 0.0f, 1.0f );
			return yAxis;
		}

		//-----------------------------------------------------------------------------
		const Vector3D& Vector3D::Zero()
			{
			static const Vector3D zero( 0.0f, 0.0f, 0.0f );
			return zero;
		}

		//-----------------------------------------------------------------------------
		const Vector3D& Vector3D::Max()
			{
			static const Vector3D max( FLT_MAX, FLT_MAX, FLT_MAX );
			return max;
		}

		//-----------------------------------------------------------------------------
		const Vector3D& Vector3D::Min()
			{
			static const Vector3D min( FLT_MIN, FLT_MIN, FLT_MIN );
			return min;
		}

		//-----------------------------------------------------------------------------
		Vector3D::Vector3D() : x(0.0f), y(0.0f), z(0.0f){} 
		
		//-----------------------------------------------------------------------------
		Vector3D::Vector3D(float X, float Y, float Z) 
			 : x(X)
			 , y(Y)
			 , z(Z)
		{} 
		
		//-----------------------------------------------------------------------------
		Vector3D::Vector3D(float number) 
			: x(number)
			, y(number)
			, z(number)
		{}
		
		//-----------------------------------------------------------------------------
		Vector3D::Vector3D(const Vector3D& vVector) 
		{
			x = vVector.x;
			y = vVector.y;
			z = vVector.z;
		}		
		
		//-----------------------------------------------------------------------------
		Vector3D& Vector3D::operator=(const Vector3D& vVector) 
		{
			x = vVector.x;
			y = vVector.y;
			z = vVector.z;
			return *this;
		}
		
		//-----------------------------------------------------------------------------
		Vector3D& Vector3D::operator+= (const Vector3D& v)			
		{
			x += v.x;
			y += v.y;
			z += v.z;
			return *this;
		}
		
		//-----------------------------------------------------------------------------
		Vector3D& Vector3D::operator-= (const Vector3D& v)
		{
			x -= v.x;
			y -= v.y;
			z -= v.z;
			return *this;
		}

		//-----------------------------------------------------------------------------
		Vector3D& Vector3D::operator *= ( const Vector3D& rhs )
		{
			x *= rhs.x;
			y *= rhs.y;
			z *= rhs.z;
			return *this;
		}

		//-----------------------------------------------------------------------------
		Vector3D& Vector3D::operator /= ( const Vector3D& rhs )
		{
			x /= rhs.x;
			y /= rhs.y;
			z /= rhs.z;
			return *this;
		}
		
		//-----------------------------------------------------------------------------
		Vector3D& Vector3D::operator*= (float value)
		{
			x *= value;
			y *= value;
			z *= value;
			return *this;
		}
		
		//-----------------------------------------------------------------------------
		Vector3D& Vector3D::operator/= (float value)
		{
			x /= value;
			y /= value;
			z /= value;
			return *this;
		}

		//-----------------------------------------------------------------------------
		Vector3D Vector3D::operator + ( const Vector3D& rhs ) const
		{
			return Vector3D(*this) += rhs;
		}

		//-----------------------------------------------------------------------------
		Vector3D Vector3D::operator - ( const Vector3D& rhs ) const
		{
			return Vector3D(*this) -= rhs;
		}

		//-----------------------------------------------------------------------------
		Vector3D Vector3D::operator * ( const Vector3D& rhs ) const
		{
			return Vector3D(*this) *= rhs;
		}

		// -----------------------------------------------------------------------------
		Vector3D Vector3D::operator / ( const Vector3D& rhs ) const
		{
			return Vector3D(*this) *= rhs;
		}

		//-----------------------------------------------------------------------------
		Vector3D Vector3D::operator * ( const float rhs ) const
		{
			return Vector3D(*this) *= rhs;
		}

		//-----------------------------------------------------------------------------
		Vector3D Vector3D::operator / ( const float rhs ) const
		{
			return Vector3D(*this) /= rhs;
		}

		//-----------------------------------------------------------------------------
		float& Vector3D::operator [](int index)
		{
			DIA_ASSERT(index < 3, "Index can not exceed 3");
			return reinterpret_cast<float*>(this)[index];
		}

		//-----------------------------------------------------------------------------
		float Vector3D::operator [](int index) const
		{
			DIA_ASSERT(index < 3, "Index can not exceed 3");
			return reinterpret_cast<const float*>(this)[index];
		}

		//-----------------------------------------------------------------------------
		bool Vector3D::operator== (const Vector3D& v)						
		{
			return (Maths::Float::FEqual(v.x, x) && Maths::Float::FEqual(v.y, y) && Maths::Float::FEqual(v.z, z));
		}

		//-----------------------------------------------------------------------------
		bool Vector3D::operator!= (const Vector3D& v)						
		{
			return !(*this == v);
		}
		
		//-----------------------------------------------------------------------------
		float Vector3D::X() const 
		{ 
			return x;
		}		
		
		//-----------------------------------------------------------------------------
		float Vector3D::Y() const 
		{ 
			return y;
		}	
		
		//-----------------------------------------------------------------------------
		float Vector3D::Z() const
		{
			return z;
		}

		//-----------------------------------------------------------------------------
		void Vector3D::X( float newX ) 
		{ 
			x = newX;
		}

		//-----------------------------------------------------------------------------
		void Vector3D::Y( float newY ) 
		{ 
			y = newY;
		}

		//-----------------------------------------------------------------------------
		void Vector3D::Z( float newZ ) 
		{ 
			z = newZ;
		}

		//-----------------------------------------------------------------------------
		void Vector3D::Set( float X, float Y, float Z)
		{
			x = X;
			y = Y;
			z = Z;
		}
		
		//-----------------------------------------------------------------------------
		void Vector3D::Set( const Vector3D& rhs )
		{
			*this = rhs;
		}

		//-----------------------------------------------------------------------------
		Vector3D& Vector3D::Clear()
		{
			*this = Zero();
			return *this;
		}

		//-----------------------------------------------------------------------------
		Vector3D& Vector3D::Invert()
		{
			-(*this);
			return *this;
		}

		//-----------------------------------------------------------------------------
		Vector3D Vector3D::AsInverse() const
		{
			return Vector3D(-x, -y, -z);
		}

		//-----------------------------------------------------------------------------
		bool Vector3D::IsNormal() const
		{
			return (SquareMagnitude() == 1.0f);
		}
		
		//-----------------------------------------------------------------------------
		Vector3D Vector3D::AsNormal() const
		{
			float mag = 1.0f / Magnitude();
			return Vector3D( x * mag, y * mag, z * mag );
		}

		//-----------------------------------------------------------------------------
        Vector3D& Vector3D::Normalize()
		{
			float mag = 1.0f / Magnitude();
			x *= mag;
			y *= mag;
			z *= mag;
			return *this;
		}
		
		//-----------------------------------------------------------------------------
		Vector3D&  Vector3D::NormalizeSafe()
		{
			float t = Magnitude();
			if (t != 0.0f)
			{
				float mag = 1.0f / t;
				x *= mag;
				y *= mag;
				z *= mag;
			}
			else
			{
				Set(1.0f,0.0f, 0.0f);
			}
			return *this;
		}

		//-----------------------------------------------------------------------------
		Vector3D  Vector3D::AsNormalSafe() const
		{
			float t = Magnitude();
			Vector3D result;
			if (t != 0.0f)
			{
				float mag = 1.0f / t;
				result.Set(x * mag, y * mag, z * mag);
			}
			else
			{
				result.Set(1.0f,0.0f, 0.0f);
			}
			return result;
		}

		//-----------------------------------------------------------------------------
		Vector3D Vector3D::Absolute()const
		{
			return Vector3D( Maths::Float::FAbs(x), Maths::Float::FAbs(y), Maths::Float::FAbs(z) );
		}

		//-----------------------------------------------------------------------------
		Vector3D& Vector3D::Absolutize()
		{
			x = Maths::Float::FAbs(x);
			y = Maths::Float::FAbs(y);
			z = Maths::Float::FAbs(z);
			return *this;
		}

		//-----------------------------------------------------------------------------
		float Vector3D::Dot( const Vector3D& vVector) const
		{
			return (x*vVector.x + y*vVector.y + z*vVector.z);
		}

		//-----------------------------------------------------------------------------
		float Vector3D::SquareMagnitude() const
		{
			return Dot(*this);
		}
		
		//-----------------------------------------------------------------------------
		float Vector3D::Magnitude() const
		{
			return Maths::Float::FSquareRoot(Dot(*this));
		}

		//-----------------------------------------------------------------------------
		float Vector3D::DistanceTo( const Vector3D& vVector) const
		{
			return (vVector - *this).Magnitude();
		}

		//-----------------------------------------------------------------------------
		float Vector3D::SquareDistanceTo( const Vector3D& vVector) const
		{
			return (vVector - *this).SquareMagnitude();
		}

		//-----------------------------------------------------------------------------
		float Vector3D::ManhattanDistanceTo	( const Vector3D& vVector) const
		{
			return (Maths::Float::FAbs(x - vVector.x) + Maths::Float::FAbs(y - vVector.y) + Maths::Float::FAbs(z - vVector.z));
		}
		
		// -----------------------------------------------------------------------------
		Vector3D Vector3D::ProjectOn( const Vector3D& rhs ) const
		{
			return (*this) * (this->Dot(rhs) / rhs.Dot(rhs));
		}
		
		// -----------------------------------------------------------------------------	
		bool Vector3D::IsValid() const
		{
			return (Maths::Float::FIsValid(x) && Maths::Float::FIsValid(y) && Maths::Float::FIsValid(z));
		}
	}
}
