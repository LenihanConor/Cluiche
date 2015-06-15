
#include "Vector/Vector4D.h"

#include "float.h"

#include "Core/CoreMaths.h"
#include "Core/FloatMaths.h"

#include <DiaCore/Core/Assert.h>

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		const Vector4D& Vector4D::XAxis()
		{
			static const Vector4D xAxis( 1.0f, 0.0f, 0.0f, 0.0f );
			return xAxis;
		}
		 
		//-----------------------------------------------------------------------------
        const Vector4D& Vector4D::YAxis()
			{
			static const Vector4D yAxis( 0.0f, 1.0f, 0.0f, 0.0f );
			return yAxis;
		}

		//-----------------------------------------------------------------------------
        const Vector4D& Vector4D::ZAxis()
			{
			static const Vector4D yAxis( 0.0f, 0.0f, 1.0f, 0.0f );
			return yAxis;
		}

		//-----------------------------------------------------------------------------
		const Vector4D& Vector4D::Zero()
			{
			static const Vector4D zero( 0.0f, 0.0f, 0.0f, 0.0f );
			return zero;
		}

		//-----------------------------------------------------------------------------
		const Vector4D& Vector4D::Max()
			{
			static const Vector4D max( FLT_MAX, FLT_MAX, FLT_MAX, 0.0f );
			return max;
		}

		//-----------------------------------------------------------------------------
		const Vector4D& Vector4D::Min()
			{
			static const Vector4D min( FLT_MIN, FLT_MIN, FLT_MIN, 0.0f );
			return min;
		}

		//-----------------------------------------------------------------------------
		Vector4D::Vector4D() : x(0.0f), y(0.0f), z(0.0f), w(0.0f){} 
		
		//-----------------------------------------------------------------------------
		Vector4D::Vector4D(float X, float Y, float Z, float W) 
			 : x(X)
			 , y(Y)
			 , z(Z)
			 , w(W)
		{} 
		
		//-----------------------------------------------------------------------------
		Vector4D::Vector4D(float number) 
			: x(number)
			, y(number)
			, z(number)
			, w(0.0f)
		{}
		
		//-----------------------------------------------------------------------------
		Vector4D::Vector4D(const Vector4D& vVector) 
		{
			x = vVector.x;
			y = vVector.y;
			z = vVector.z;
			w = vVector.w;
		}		
		
		//-----------------------------------------------------------------------------
		Vector4D& Vector4D::operator=(const Vector4D& vVector) 
		{
			x = vVector.x;
			y = vVector.y;
			z = vVector.z;
			w = vVector.w;
			return *this;
		}
		
		//-----------------------------------------------------------------------------
		Vector4D& Vector4D::operator+= (const Vector4D& v)			
		{
			x += v.x;
			y += v.y;
			z += v.z;
			w = v.w;

			return *this;
		}
		
		//-----------------------------------------------------------------------------
		Vector4D& Vector4D::operator-= (const Vector4D& v)
		{
			x -= v.x;
			y -= v.y;
			z -= v.z;
			w = v.w;

			return *this;
		}

		//-----------------------------------------------------------------------------
		Vector4D& Vector4D::operator *= ( const Vector4D& rhs )
		{
			x *= rhs.x;
			y *= rhs.y;
			z *= rhs.z;
			w = rhs.w;

			return *this;
		}

		//-----------------------------------------------------------------------------
		Vector4D& Vector4D::operator /= ( const Vector4D& rhs )
		{
			x /= rhs.x;
			y /= rhs.y;
			z /= rhs.z;
			w = rhs.w;

			return *this;
		}
		
		//-----------------------------------------------------------------------------
		Vector4D& Vector4D::operator*= (float value)
		{
			x *= value;
			y *= value;
			z *= value;

			return *this;
		}
		
		//-----------------------------------------------------------------------------
		Vector4D& Vector4D::operator/= (float value)
		{
			x /= value;
			y /= value;
			z /= value;
			return *this;
		}

		//-----------------------------------------------------------------------------
		Vector4D Vector4D::operator + ( const Vector4D& rhs ) const
		{
			return Vector4D(*this) += rhs;
		}

		//-----------------------------------------------------------------------------
		Vector4D Vector4D::operator - ( const Vector4D& rhs ) const
		{
			return Vector4D(*this) -= rhs;
		}

		//-----------------------------------------------------------------------------
		Vector4D Vector4D::operator * ( const Vector4D& rhs ) const
		{
			return Vector4D(*this) *= rhs;
		}

		// -----------------------------------------------------------------------------
		Vector4D Vector4D::operator / ( const Vector4D& rhs ) const
		{
			return Vector4D(*this) *= rhs;
		}

		//-----------------------------------------------------------------------------
		Vector4D Vector4D::operator * ( const float rhs ) const
		{
			return Vector4D(*this) *= rhs;
		}

		//-----------------------------------------------------------------------------
		Vector4D Vector4D::operator / ( const float rhs ) const
		{
			return Vector4D(*this) /= rhs;
		}

		//-----------------------------------------------------------------------------
		float& Vector4D::operator [](int index)
		{
			DIA_ASSERT(index < 4, "Index can not exceed 4");
			return reinterpret_cast<float*>(this)[index];
		}

		//-----------------------------------------------------------------------------
		float Vector4D::operator [](int index) const
		{
			DIA_ASSERT(index < 4, "Index can not exceed 4");
			return reinterpret_cast<const float*>(this)[index];
		}

		//-----------------------------------------------------------------------------
		bool Vector4D::operator== (const Vector4D& v)						
		{
			return (Maths::Float::FEqual(v.x, x) && Maths::Float::FEqual(v.y, y) && Maths::Float::FEqual(v.z, z) && Maths::Float::FEqual(v.w, w));
		}

		//-----------------------------------------------------------------------------
		bool Vector4D::operator!= (const Vector4D& v)						
		{
			return !(*this == v);
		}
		
		//-----------------------------------------------------------------------------
		float Vector4D::X() const 
		{ 
			return x;
		}		
		
		//-----------------------------------------------------------------------------
		float Vector4D::Y() const 
		{ 
			return y;
		}	
		
		//-----------------------------------------------------------------------------
		float Vector4D::Z() const
		{
			return z;
		}

		//-----------------------------------------------------------------------------
		float Vector4D::W() const
		{
			return w;
		}

		//-----------------------------------------------------------------------------
		void Vector4D::X( float newX ) 
		{ 
			x = newX;
		}

		//-----------------------------------------------------------------------------
		void Vector4D::Y( float newY ) 
		{ 
			y = newY;
		}
	
		//-----------------------------------------------------------------------------
		void Vector4D::Z( float newZ ) 
		{ 
			z = newZ;
		}
		
		//-----------------------------------------------------------------------------
		void Vector4D::W( float newW )
		{
			w = newW;
		}

		//-----------------------------------------------------------------------------
		void Vector4D::Set( float X, float Y, float Z, float W)
		{
			x = X;
			y = Y;
			z = Z;
			w = W;
		}
		
		//-----------------------------------------------------------------------------
		void Vector4D::Set( const Vector4D& rhs )
		{
			*this = rhs;
		}

		//-----------------------------------------------------------------------------
		Vector4D& Vector4D::Clear()
		{
			*this = Zero();
			return *this;
		}

		//-----------------------------------------------------------------------------
		Vector4D& Vector4D::Invert()
		{
			-(*this);
			return *this;
		}

		//-----------------------------------------------------------------------------
		Vector4D Vector4D::AsInverse() const
		{
			return Vector4D(-x, -y, -z, w);
		}

		//-----------------------------------------------------------------------------
		bool Vector4D::IsNormal() const
		{
			return (SquareMagnitude() == 1.0f);
		}
		
		//-----------------------------------------------------------------------------
		Vector4D Vector4D::AsNormal() const
		{
			float mag = 1.0f / Magnitude();
			return Vector4D( x * mag, y * mag, z * mag, w );
		}

		//-----------------------------------------------------------------------------
        Vector4D& Vector4D::Normalize()
		{
			float mag = 1.0f / Magnitude();
			x *= mag;
			y *= mag;
			z *= mag;

			return *this;
		}

		//-----------------------------------------------------------------------------
		Vector4D&  Vector4D::NormalizeSafe()
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
				Set(1.0f,0.0f, 0.0f, 0.0f);
			}
			return *this;
		}

		//-----------------------------------------------------------------------------
		Vector4D  Vector4D::AsNormalSafe() const
		{
			float t = Magnitude();
			Vector4D result;
			if (t != 0.0f)
			{
				float mag = 1.0f / t;
				result.Set(x * mag, y * mag, z * mag, w);
			}
			else
			{
				result.Set(1.0f,0.0f, 0.0f, 0.0f);
			}
			return result;
		}

		//-----------------------------------------------------------------------------
		Vector4D Vector4D::Absolute()const
		{
			return Vector4D( Maths::Float::FAbs(x), Maths::Float::FAbs(y), Maths::Float::FAbs(z), w );
		}

		//-----------------------------------------------------------------------------
		Vector4D& Vector4D::Absolutize()
		{
			x = Maths::Float::FAbs(x);
			y = Maths::Float::FAbs(y);
			z = Maths::Float::FAbs(z);
			return *this;
		}

		//-----------------------------------------------------------------------------
		float Vector4D::Dot( const Vector4D& vVector) const
		{
			return (x*vVector.x + y*vVector.y + z*vVector.z);
		}

		//-----------------------------------------------------------------------------
		float Vector4D::SquareMagnitude() const
		{
			return Dot(*this);
		}
		
		//-----------------------------------------------------------------------------
		float Vector4D::Magnitude() const
		{
			return Maths::Float::FSquareRoot(Dot(*this));
		}

		//-----------------------------------------------------------------------------
		float Vector4D::DistanceTo( const Vector4D& vVector) const
		{
			return (vVector - *this).Magnitude();
		}

		//-----------------------------------------------------------------------------
		float Vector4D::SquareDistanceTo( const Vector4D& vVector) const
		{
			return (vVector - *this).SquareMagnitude();
		}

		//-----------------------------------------------------------------------------
		float Vector4D::ManhattanDistanceTo	( const Vector4D& vVector) const
		{
			return (Maths::Float::FAbs(x - vVector.x) + Maths::Float::FAbs(y - vVector.y) + Maths::Float::FAbs(z - vVector.z));
		}
		
		// -----------------------------------------------------------------------------
		Vector4D Vector4D::ProjectOn( const Vector4D& rhs ) const
		{
			return (*this) * (this->Dot(rhs) / rhs.Dot(rhs));
		}
		
		// -----------------------------------------------------------------------------	
		bool Vector4D::IsValid() const
		{
			return (Maths::Float::FIsValid(x) && Maths::Float::FIsValid(y) && Maths::Float::FIsValid(z) && Maths::Float::FIsValid(w));
		}
	}
}
