#include <DiaCore/Core/Assert.h>

#include "DiaMaths/Core/CoreMaths.h"
#include "DiaMaths/Core/FloatMaths.h"

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		inline
		const Vector2D& Vector2D::XAxis()
		{
			static const Vector2D xAxis( 1.0f, 0.0f );
			return xAxis;
		}

		//-----------------------------------------------------------------------------
		inline
		const Vector2D& Vector2D::YAxis()
		{
			static const Vector2D yAxis( 0.0f, 1.0f );
			return yAxis;
		}

		//-----------------------------------------------------------------------------
		inline 
		const Vector2D& Vector2D::Zero()
		{
			static const Vector2D zero( 0.0f, 0.0f );
			return zero;
		}		

		//-----------------------------------------------------------------------------
		inline
		Vector2D::Vector2D() : x(0.0f), y(0.0f){} 

		//-----------------------------------------------------------------------------
		inline
		Vector2D::Vector2D(float X, float Y) 
			: x(X)
			, y(Y)
		{} 

		//-----------------------------------------------------------------------------
		inline
		Vector2D::Vector2D(float number) 
			: x(number)
			, y(number)
		{}

		//-----------------------------------------------------------------------------
		inline
		Vector2D::Vector2D(const Vector2D& vVector) 
		{
			x = vVector.x;
			y = vVector.y;
		}		

		//-----------------------------------------------------------------------------
		inline
		Vector2D& Vector2D::operator=(const Vector2D& vVector) 
		{
			x = vVector.x;
			y = vVector.y;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		Vector2D& Vector2D::operator+= (const Vector2D& v)			
		{
			x += v.x;
			y += v.y;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		Vector2D& Vector2D::operator-= (const Vector2D& v)
		{
			x -= v.x;
			y -= v.y;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		Vector2D& Vector2D::operator *= ( const Vector2D& rhs )
		{
			x *= rhs.x;
			y *= rhs.y;

			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		Vector2D& Vector2D::operator /= ( const Vector2D& rhs )
		{
			x /= rhs.x;
			y /= rhs.y;

			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		Vector2D& Vector2D::operator*= (float value)
		{
			x *= value;
			y *= value;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		Vector2D& Vector2D::operator/= (float value)
		{
			x /= value;
			y /= value;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		Vector2D Vector2D::operator - () const
		{
			return Vector2D(-this->x, -this->y);
		}

		//-----------------------------------------------------------------------------
		inline
		Vector2D Vector2D::operator + ( const Vector2D& rhs ) const
		{
			return Vector2D(*this) += rhs;
		}

		//-----------------------------------------------------------------------------
		inline
		Vector2D Vector2D::operator - ( const Vector2D& rhs ) const
		{
			return Vector2D(*this) -= rhs;
		}

		//-----------------------------------------------------------------------------
		inline
		Vector2D Vector2D::operator * ( const Vector2D& rhs ) const
		{
			return Vector2D(*this) *= rhs;
		}

		// -----------------------------------------------------------------------------
		inline
		Vector2D Vector2D::operator / ( const Vector2D& rhs ) const
		{
			return Vector2D(*this) /= rhs;
		}

		//-----------------------------------------------------------------------------
		inline
		Vector2D Vector2D::operator * ( const float rhs ) const
		{
			return Vector2D(*this) *= rhs;
		}

		//-----------------------------------------------------------------------------
		inline
		Vector2D Vector2D::operator / ( const float rhs ) const
		{
			return Vector2D(*this) /= rhs;
		}

		//-----------------------------------------------------------------------------
		inline
		bool Vector2D::operator== (const Vector2D& v)const						
		{
			return (Dia::Maths::Float::FEqual(v.x, x) && Dia::Maths::Float::FEqual(v.y, y));
		}

		//-----------------------------------------------------------------------------
		inline
		bool Vector2D::operator!= (const Vector2D& v)const					
		{
			return !(*this == v);
		}

		//-----------------------------------------------------------------------------
		inline
		float Vector2D::X() const 
		{ 
			return x;
		}		

		//-----------------------------------------------------------------------------
		inline
		float Vector2D::Y() const 
		{ 
			return y;
		}	

		//-----------------------------------------------------------------------------
		inline
		void Vector2D::X( float newX ) 
		{ 
			x = newX;
		}

		//-----------------------------------------------------------------------------
		inline
		void Vector2D::Y( float newY ) 
		{ 
			y = newY;
		}

		//-----------------------------------------------------------------------------
		inline
		float& Vector2D::operator [](int index)
		{
			DIA_ASSERT(index >= 0 && index <= 1, "Index can not exceed 2");
			return reinterpret_cast<float*>(this)[index];
		}

		//-----------------------------------------------------------------------------
		inline
		float Vector2D::operator [](int index) const
		{
			DIA_ASSERT(index >= 0 && index <= 1, "Index can not exceed 2");	
			return reinterpret_cast<const float*>(this)[index];
		}

		//-----------------------------------------------------------------------------
		inline
		void Vector2D::Set( float X, float Y )
		{
			x = X;
			y = Y;
		}

		//-----------------------------------------------------------------------------
		inline
		void Vector2D::Set( const Vector2D& rhs)
		{
			*this = rhs;
		}

		//-----------------------------------------------------------------------------
		inline
		Vector2D& Vector2D::Clear()
		{
			*this = Zero();
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		Vector2D& Vector2D::Invert()
		{
			x *= -1.0f;
			y *= -1.0f;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		Vector2D Vector2D::AsInverse() const
		{
			return Vector2D(-x, -y);
		}

		//-----------------------------------------------------------------------------
		inline
		Vector2D Vector2D::AsNormal() const
		{
			float mag = Magnitude();
			
			DIA_ASSERT(Dia::Maths::Float::FIsValid(mag) && mag > 0.0f, "Magnitude of vector equal zero");

			mag = 1.0f / mag;

			return Vector2D( x * mag, y * mag );
		}

		//-----------------------------------------------------------------------------
        inline
		Vector2D& Vector2D::Normalize()
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
		float Vector2D::Dot( const Vector2D& vVector) const
		{
			return (x*vVector.x + y*vVector.y);
		}

		//-----------------------------------------------------------------------------
		inline
		float Vector2D::SquareMagnitude() const
		{
			return Dot(*this);
		}

		// -----------------------------------------------------------------------------
		inline
		Vector2D Vector2D::AsSwitchedAxis()const
		{
			Vector2D temp(*this);
			temp.SwitchAxis();
			return temp;
		}
		
		// -----------------------------------------------------------------------------
		inline
		Vector2D& Vector2D::SwitchAxis() 
		{
			Dia::Maths::Swap(x, y);
			return *this;
		}

		// -----------------------------------------------------------------------------
		inline
		bool Vector2D::IsClockwiseTo(const Vector2D& rhs) const
		{
			return !(0.f <= (X()*(rhs.Y() - 0.f) + rhs.X()*(0.f - Y())));
		}
		
		// -----------------------------------------------------------------------------
		inline
		bool Vector2D::IsCounterClockwiseTo( const Vector2D& rhs ) const
		{
			return 0.f < (X()*(rhs.Y() - 0.f) + rhs.X()*(0.f - Y()));
		}
		
		// -----------------------------------------------------------------------------
		inline
		bool Vector2D::IsParallelTo( const Vector2D& vec, float accuracy) const
		{
			return ((Dia::Maths::Float::FEqual(vec.x, x, accuracy) && Dia::Maths::Float::FEqual(vec.y, y, accuracy)) || 
				   (Dia::Maths::Float::FEqual(vec.x, -x, accuracy) && Dia::Maths::Float::FEqual(vec.y, -y, accuracy)));
		}

		//-----------------------------------------------------------------------------
		inline
		const Vector2D& Vector2D::Max()
		{
			static const Vector2D maxVector( Dia::Maths::Float::Max(), Dia::Maths::Float::Max() );
			return maxVector;
		}

		//-----------------------------------------------------------------------------
		inline
		const Vector2D& Vector2D::Min()
		{
			static const Vector2D minVector( Dia::Maths::Float::Min(), Dia::Maths::Float::Min() );
			return minVector;
		}
		
		//-----------------------------------------------------------------------------
		inline
		bool Vector2D::IsNormal() const
		{
			return (Dia::Maths::Float::FEqual(SquareMagnitude(), 1.0f));
		}
		
		//-----------------------------------------------------------------------------
		inline
		Vector2D Vector2D::Absolute()const
		{
			return Vector2D( Maths::Float::FAbs(x), Maths::Float::FAbs(y) );
		}

		//-----------------------------------------------------------------------------
		inline
		Vector2D& Vector2D::Absolutize()
		{
			x = Maths::Float::FAbs(x);
			y = Maths::Float::FAbs(y);
			return *this;
		}
		
		//-----------------------------------------------------------------------------
		inline
		float Vector2D::Magnitude() const
		{
			return Maths::Float::FSquareRoot(Dot(*this));
		}

		//-----------------------------------------------------------------------------
		inline
		float Vector2D::DistanceTo( const Vector2D& vVector) const
		{
			return (vVector - *this).Magnitude();
		}

		//-----------------------------------------------------------------------------
		inline
		float Vector2D::SquareDistanceTo( const Vector2D& vVector) const
		{
			return (vVector - *this).SquareMagnitude();
		}

		//-----------------------------------------------------------------------------
		inline
		float Vector2D::ManhattanDistanceTo	( const Vector2D& vVector) const
		{
			return (Maths::Float::FAbs(x - vVector.x) + Maths::Float::FAbs(y - vVector.y));
		}
				
		// -----------------------------------------------------------------------------	
		inline
		bool Vector2D::IsValid() const
		{
			return (Maths::Float::FIsValid(x) && Maths::Float::FIsValid(y));
		}

		// -----------------------------------------------------------------------------	
		inline
		Vector2D Vector2D::AsRotated90DegreeCounterClockwise()const
		{
			 return Vector2D( -y, x );
		}

		// -----------------------------------------------------------------------------	
		inline
		Vector2D& Vector2D::Rotate90DegreeCounterClockwise()
		{
			float tempX = x; 
			x = -y;
			y = tempX;

			return *this;
		}

		// -----------------------------------------------------------------------------	
		inline
		Vector2D Vector2D::AsRotated90DegreeClockwise()const
		{
			 return Vector2D( y, -x );
		}

		// -----------------------------------------------------------------------------
		inline
		Vector2D& Vector2D::Rotate90DegreeClockwise()
		{
			float tempX = x;
			x = y;
			y = -tempX;

			return *this;
		}

		// -----------------------------------------------------------------------------
		// Interpolation Functions
		//
		// Convenience functions for animating and moving vectors.
		// These wrap the generic interpolation functions from Interpolation.h
		// with Vector2D-specific implementations.
		// -----------------------------------------------------------------------------

		// Linear interpolation between a and b by t (clamped to 0-1)
		// Parameters: a - start vector, b - end vector, t - interpolation factor
		// Returns: Interpolated vector between a (at t=0) and b (at t=1)
		// Use for: Smooth movement between two points over time
		inline
		Vector2D Vector2D::Lerp(const Vector2D& a, const Vector2D& b, float t)
		{
			t = Clamp(t, 0.0f, 1.0f);
			return Vector2D(
				a.x + (b.x - a.x) * t,
				a.y + (b.y - a.y) * t
			);
		}

		// -----------------------------------------------------------------------------
		// Linear interpolation between a and b by t (not clamped)
		// Allows extrapolation beyond the a-b range when t < 0 or t > 1
		// Use for: Predicting future positions, trajectories
		inline
		Vector2D Vector2D::LerpUnclamped(const Vector2D& a, const Vector2D& b, float t)
		{
			return Vector2D(
				a.x + (b.x - a.x) * t,
				a.y + (b.y - a.y) * t
			);
		}

		// -----------------------------------------------------------------------------
		// Smooth interpolation between a and b using smoothstep
		// Provides ease-in and ease-out (slow start, fast middle, slow end)
		// Use for: Smooth animations, camera movement, polished motion
		inline
		Vector2D Vector2D::SmoothLerp(const Vector2D& a, const Vector2D& b, float t)
		{
			t = Clamp(t, 0.0f, 1.0f);
			float smoothT = t * t * (3.0f - 2.0f * t); // Classic smoothstep formula
			return LerpUnclamped(a, b, smoothT);
		}

		// -----------------------------------------------------------------------------
		// Moves current toward target by maxDistance (does not overshoot)
		// Guarantees we stop exactly at target if within maxDistance
		// Parameters:
		//   current     - Current position
		//   target      - Target position to move toward
		//   maxDistance - Maximum distance to move this frame
		// Use for: Speed-limited following (camera, AI), frame-rate independent movement
		inline
		Vector2D Vector2D::MoveTowards(const Vector2D& current, const Vector2D& target, float maxDistance)
		{
			Vector2D diff = target - current;
			float sqrDist = diff.SquareMagnitude();

			// If already at target or within one step, return target exactly
			// This prevents oscillation and ensures we stop precisely
			if (sqrDist <= maxDistance * maxDistance || sqrDist < FLOAT_EPSILON)
			{
				return target;
			}

			// Move maxDistance toward target along direction vector
			float dist = Dia::Maths::SquareRoot(sqrDist);
			return current + diff * (maxDistance / dist);
		}
	}
}