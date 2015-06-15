// -----------------------------------------------------------------------------
#include "DiaMaths/Core/CoreMaths.h"
#include "DiaMaths/Core/FloatMaths.h"
#include "DiaMaths/Core/Trigonometry.h"

#include <DiaCore/Core/Assert.h>

namespace Dia
{
	namespace Maths
	{

		// -----------------------------------------------------------------------------
		inline Angle::Angle()
			: mRadian( 0.0f ) 
		{}

		// -----------------------------------------------------------------------------
		inline Angle::Angle( const float radian )
			: mRadian( radian )
		{
			Normalize();
		}

		// -----------------------------------------------------------------------------
		inline Angle::Angle( const Angle& angle )
			: mRadian(angle.mRadian)
		{}

		// -----------------------------------------------------------------------------
		inline Angle& Angle::operator = ( const Angle& angle)
		{ 
			mRadian = angle.mRadian;
			return *this;
		}

		// -----------------------------------------------------------------------------
		inline Angle Angle::operator -() const
		{
			return Angle(-this->mRadian);
		}

		// -----------------------------------------------------------------------------
		inline Angle Angle::operator + ( const Angle& angle) const
		{
			return Angle(*this) += angle;
		}

		// -----------------------------------------------------------------------------
		inline Angle Angle::operator - ( const Angle& angle) const
		{
			return Angle(*this) -= angle;
		}

		// -----------------------------------------------------------------------------
		inline Angle Angle::operator * ( const float scaler ) const
		{
			return Angle(*this) *= scaler;
		}

		// -----------------------------------------------------------------------------
		inline Angle Angle::operator / ( const float scaler ) const
		{
			return Angle(*this) /= scaler;
		}


		// -----------------------------------------------------------------------------
		inline Angle& Angle::operator += ( const Angle& angle)
		{
			mRadian += angle.mRadian;
			Normalize();
			return *this;
		}

		// -----------------------------------------------------------------------------
		inline Angle& Angle::operator -= ( const Angle& angle)
		{
			mRadian -= angle.mRadian;
			Normalize();
			return *this;
		}

		// -----------------------------------------------------------------------------
		inline Angle& Angle::operator *= ( const float scaler)
		{
			mRadian *= scaler;
			Normalize();
			return *this;
		}

		// -----------------------------------------------------------------------------
		inline Angle& Angle::operator /= ( const float scaler)
		{
			mRadian /= scaler;
			Normalize();
			return *this;
		}

		// -----------------------------------------------------------------------------
		inline bool Angle::operator ==	( const Angle& angle) const
		{
			return Dia::Maths::Float::FEqual(mRadian, angle.mRadian, 0.001f);
		}

		// -----------------------------------------------------------------------------
		inline bool Angle::operator !=	( const Angle& angle) const
		{
			return !Dia::Maths::Float::FEqual(mRadian, angle.mRadian, 0.001f);
		}

		// -----------------------------------------------------------------------------
		inline bool Angle::operator <=	( const Angle& angle) const
		{
			return Dia::Maths::Float::FLessEqual(mRadian, angle.mRadian);
		}

		// -----------------------------------------------------------------------------
		inline bool Angle::operator >=	( const Angle& angle) const
		{
			return Dia::Maths::Float::FGreaterEqual(mRadian, angle.mRadian);
		}

		// -----------------------------------------------------------------------------
		inline bool Angle::operator < ( const Angle& angle) const
		{
			return Dia::Maths::Float::FLess(mRadian, angle.mRadian);
		}

		// -----------------------------------------------------------------------------
		inline bool Angle::operator > ( const Angle& angle) const
		{
			return Dia::Maths::Float::FGreater(mRadian, angle.mRadian);
		}

		// -----------------------------------------------------------------------------
		inline float Angle::AsRadians() const
		{
			return mRadian;
		}

		// -----------------------------------------------------------------------------
		inline float Angle::AsDegrees() const
		{
			return Dia::Maths::RadiansToDeg(mRadian);
		}

		// -----------------------------------------------------------------------------
		inline float Angle::AsPositiveRadians() const
		{
			return Dia::Maths::Float::FSelect(mRadian, mRadian, mRadian + Dia::Maths::PI);
		}

		// -----------------------------------------------------------------------------
		inline float Angle::AsPositiveDegrees() const
		{
			return Dia::Maths::RadiansToDeg(AsPositiveRadians());
		}

		// -----------------------------------------------------------------------------
		inline Angle Angle::FromDegrees(const float degrees)
		{
			return Angle(Dia::Maths::DegToRadians(degrees));
		}

		// -----------------------------------------------------------------------------
		inline Angle Angle::FromRadians(const float radians)
		{
			return Angle(radians);
		}

		// -----------------------------------------------------------------------------
		inline void Angle::SetFromDegrees(const float degrees)
		{
			mRadian = Dia::Maths::DegToRadians(degrees);
		}

		// -----------------------------------------------------------------------------
		inline void Angle::Normalize()
		{
			while( mRadian > Dia::Maths::PI )
			{
				mRadian -= Dia::Maths::PI_2;
			}
			while( mRadian < -Dia::Maths::PI )
			{
				mRadian += Dia::Maths::PI_2;
			}
		
			DIA_ASSERT(mRadian >= -Dia::Maths::PI && mRadian <= Dia::Maths::PI, "Radians ouside range [-PI, PI]");
		}

		// -----------------------------------------------------------------------------
		inline Angle operator * ( const float scaler, const Angle& angle)
		{
			return Angle(angle) *= scaler;
		}
	} 
} 

