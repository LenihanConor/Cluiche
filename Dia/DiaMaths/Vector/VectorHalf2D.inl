#include <DiaCore/Core/Assert.h>

#include "DiaMaths/Core/CoreMaths.h"
#include "DiaMaths/Core/FloatMaths.h"

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		inline
		const VectorHalf2D& VectorHalf2D::XAxis()
		{
			static const VectorHalf2D xAxis( 1.0f, 0.0f );
			return xAxis;
		}

		//-----------------------------------------------------------------------------
		inline
		const VectorHalf2D& VectorHalf2D::YAxis()
		{
			static const VectorHalf2D yAxis( 0.0f, 1.0f );
			return yAxis;
		}

		//-----------------------------------------------------------------------------
		inline 
		const VectorHalf2D& VectorHalf2D::Zero()
		{
			static const VectorHalf2D zero( 0.0f, 0.0f );
			return zero;
		}		

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D::VectorHalf2D() : x(0.0f), y(0.0f){} 

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D::VectorHalf2D(float X, float Y) 
			: x(X)
			, y(Y)
		{} 

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D::VectorHalf2D(float number) 
			: x(number)
			, y(number)
		{}

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D::VectorHalf2D(const VectorHalf2D& vVector) 
		{
			x = vVector.x;
			y = vVector.y;
		}		

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D& VectorHalf2D::operator=(const VectorHalf2D& vVector) 
		{
			x = vVector.x;
			y = vVector.y;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D& VectorHalf2D::operator+= (const VectorHalf2D& v)			
		{
			x += v.x;
			y += v.y;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D& VectorHalf2D::operator-= (const VectorHalf2D& v)
		{
			x -= v.x;
			y -= v.y;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D& VectorHalf2D::operator *= ( const VectorHalf2D& rhs )
		{
			x *= rhs.x;
			y *= rhs.y;

			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D& VectorHalf2D::operator /= ( const VectorHalf2D& rhs )
		{
			x /= rhs.x;
			y /= rhs.y;

			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D& VectorHalf2D::operator*= (float value)
		{
			x *= value;
			y *= value;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D& VectorHalf2D::operator/= (float value)
		{
			x /= value;
			y /= value;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D VectorHalf2D::operator - () const
		{
			return VectorHalf2D(-this->x, -this->y);
		}

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D VectorHalf2D::operator + ( const VectorHalf2D& rhs ) const
		{
			return VectorHalf2D(*this) += rhs;
		}

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D VectorHalf2D::operator - ( const VectorHalf2D& rhs ) const
		{
			return VectorHalf2D(*this) -= rhs;
		}

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D VectorHalf2D::operator * ( const VectorHalf2D& rhs ) const
		{
			return VectorHalf2D(*this) *= rhs;
		}

		// -----------------------------------------------------------------------------
		inline
		VectorHalf2D VectorHalf2D::operator / ( const VectorHalf2D& rhs ) const
		{
			return VectorHalf2D(*this) /= rhs;
		}

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D VectorHalf2D::operator * ( const float rhs ) const
		{
			return VectorHalf2D(*this) *= rhs;
		}

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D VectorHalf2D::operator / ( const float rhs ) const
		{
			return VectorHalf2D(*this) /= rhs;
		}

		//-----------------------------------------------------------------------------
		inline
		bool VectorHalf2D::operator== (const VectorHalf2D& v)const						
		{
			return (Dia::Maths::Float::FEqual(v.x, x) && Dia::Maths::Float::FEqual(v.y, y));
		}

		//-----------------------------------------------------------------------------
		inline
		bool VectorHalf2D::operator!= (const VectorHalf2D& v)const					
		{
			return !(*this == v);
		}

		//-----------------------------------------------------------------------------
		inline
		HalfFloat VectorHalf2D::X() const 
		{ 
			return x;
		}		

		//-----------------------------------------------------------------------------
		inline
		HalfFloat VectorHalf2D::Y() const 
		{ 
			return y;
		}	

		//-----------------------------------------------------------------------------
		inline
		void VectorHalf2D::X( float newX ) 
		{ 
			x = newX;
		}

		//-----------------------------------------------------------------------------
		inline
		void VectorHalf2D::Y( float newY ) 
		{ 
			y = newY;
		}

		//-----------------------------------------------------------------------------
		inline
		HalfFloat& VectorHalf2D::operator [](int index)
		{
			DIA_ASSERT(index >= 0 && index <= 1, "Index can not exceed 2");
			return reinterpret_cast<HalfFloat*>(this)[index];
		}

		//-----------------------------------------------------------------------------
		inline
		HalfFloat VectorHalf2D::operator [](int index) const
		{
			DIA_ASSERT(index >= 0 && index <= 1, "Index can not exceed 2");	
			return reinterpret_cast<const HalfFloat*>(this)[index];
		}

		//-----------------------------------------------------------------------------
		inline
		void VectorHalf2D::Set( float X, float Y )
		{
			x = X;
			y = Y;
		}

		//-----------------------------------------------------------------------------
		inline
		void VectorHalf2D::Set( const VectorHalf2D& rhs)
		{
			*this = rhs;
		}

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D& VectorHalf2D::Clear()
		{
			*this = Zero();
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D& VectorHalf2D::Invert()
		{
			x *= -1.0f;
			y *= -1.0f;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D VectorHalf2D::AsInverse() const
		{
			return VectorHalf2D(-x, -y);
		}

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D VectorHalf2D::AsNormal() const
		{
			float mag = Magnitude();
			
			DIA_ASSERT(Dia::Maths::Float::FIsValid(mag) && mag > 0.0f, "Magnitude of vector equal zero");

			mag = 1.0f / mag;

			return VectorHalf2D( x * mag, y * mag );
		}

		//-----------------------------------------------------------------------------
        inline
		VectorHalf2D& VectorHalf2D::Normalize()
		{
			float mag = Magnitude();
			
			DIA_ASSERT(Dia::Maths::Float::FIsValid(mag) && mag > 0.0f, "Magnitude of vector equal zero");

			mag = 1.0f / mag;
			
			x *= mag;
			y *= mag;

			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		float VectorHalf2D::Dot( const VectorHalf2D& vVector) const
		{
			float a = x.ToFloat();
			float b = vVector.x.ToFloat();

			float c = y.ToFloat();
			float d = vVector.y.ToFloat();

			return (a*b + c*d);
		}

		//-----------------------------------------------------------------------------
		inline
		float VectorHalf2D::SquareMagnitude() const
		{
			return Dot(*this);
		}

		// -----------------------------------------------------------------------------
		inline
		VectorHalf2D VectorHalf2D::AsSwitchedAxis()const
		{
			VectorHalf2D temp(*this);
			temp.SwitchAxis();
			return temp;
		}
		
		// -----------------------------------------------------------------------------
		inline
		VectorHalf2D& VectorHalf2D::SwitchAxis() 
		{
			Dia::Maths::Swap(x, y);
			return *this;
		}

		// -----------------------------------------------------------------------------
		inline
		bool VectorHalf2D::IsClockwiseTo(const VectorHalf2D& rhs) const
		{
			return !(0.f <= (X()*(rhs.Y() - 0.f) + rhs.X()*(0.f - Y())));
		}
		
		// -----------------------------------------------------------------------------
		inline
		bool VectorHalf2D::IsCounterClockwiseTo( const VectorHalf2D& rhs ) const
		{
			return 0.f < (X()*(rhs.Y() - 0.f) + rhs.X()*(0.f - Y()));
		}
		
		// -----------------------------------------------------------------------------
		inline
		bool VectorHalf2D::IsParallelTo( const VectorHalf2D& vec, float accuracy) const
		{
			return ((Dia::Maths::Float::FEqual(vec.x, x, accuracy) && Dia::Maths::Float::FEqual(vec.y, y, accuracy)) || 
				   (Dia::Maths::Float::FEqual(vec.x, -x, accuracy) && Dia::Maths::Float::FEqual(vec.y, -y, accuracy)));
		}

		//-----------------------------------------------------------------------------
		inline
		const VectorHalf2D& VectorHalf2D::Max()
		{
			static const VectorHalf2D max( Dia::Maths::HalfFloat::Max(), Dia::Maths::HalfFloat::Max() );
			return max;
		}

		//-----------------------------------------------------------------------------
		inline
		const VectorHalf2D& VectorHalf2D::Min()
		{
			static const VectorHalf2D min( Dia::Maths::HalfFloat::Min(), Dia::Maths::HalfFloat::Min() );
			return min;
		}
		
		//-----------------------------------------------------------------------------
		inline
		bool VectorHalf2D::IsNormal() const
		{
			return (Dia::Maths::Float::FEqual(SquareMagnitude(), 1.0f, 0.00035f));
		}
		
		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D VectorHalf2D::Absolute()const
		{
			return VectorHalf2D( Maths::Float::FAbs(x), Maths::Float::FAbs(y) );
		}

		//-----------------------------------------------------------------------------
		inline
		VectorHalf2D& VectorHalf2D::Absolutize()
		{
			x = Maths::Float::FAbs(x);
			y = Maths::Float::FAbs(y);
			return *this;
		}
		
		//-----------------------------------------------------------------------------
		inline
		float VectorHalf2D::Magnitude() const
		{
			return Maths::Float::FSquareRoot(Dot(*this));
		}

		//-----------------------------------------------------------------------------
		inline
		float VectorHalf2D::DistanceTo( const VectorHalf2D& vVector) const
		{
			return (vVector - *this).Magnitude();
		}

		//-----------------------------------------------------------------------------
		inline
		float VectorHalf2D::SquareDistanceTo( const VectorHalf2D& vVector) const
		{
			return (vVector - *this).SquareMagnitude();
		}

		//-----------------------------------------------------------------------------
		inline
		float VectorHalf2D::ManhattanDistanceTo	( const VectorHalf2D& vVector) const
		{
			return (Maths::Float::FAbs(x - vVector.x) + Maths::Float::FAbs(y - vVector.y));
		}
				
		// -----------------------------------------------------------------------------	
		inline
		bool VectorHalf2D::IsValid() const
		{
			return (Maths::Float::FIsValid(x) && Maths::Float::FIsValid(y));
		}

		// -----------------------------------------------------------------------------	
		inline
		VectorHalf2D VectorHalf2D::AsRotated90DegreeCounterClockwise()const
		{
			 return VectorHalf2D( -y, x );
		}

		// -----------------------------------------------------------------------------	
		inline
		VectorHalf2D& VectorHalf2D::Rotate90DegreeCounterClockwise()
		{
			float tempX = x; 
			x = -y;
			y = tempX;

			return *this;
		}

		// -----------------------------------------------------------------------------	
		inline
		VectorHalf2D VectorHalf2D::AsRotated90DegreeClockwise()const
		{
			 return VectorHalf2D( y, -x );
		}

		// -----------------------------------------------------------------------------	
		inline
		VectorHalf2D& VectorHalf2D::Rotate90DegreeClockwise()
		{
			float tempX = x; 
			x = y;
			y = -tempX;

			return *this;
		}
	}
}