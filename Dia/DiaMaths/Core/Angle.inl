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
        // Default constructor. Initializes the Angle with zero radians.
        inline Angle::Angle()
            : mRadian(0.0f)
        {}

        // -----------------------------------------------------------------------------
        // Constructor that initializes the Angle with the given radian value.
        // It also normalizes the angle.
        // @param radian: The value of the angle in radians.
        inline Angle::Angle(const float radian)
            : mRadian(radian)
        {
            Normalize();
        }

        // -----------------------------------------------------------------------------
        // Copy constructor. Creates a new Angle instance from another Angle.
        // @param angle: The Angle instance to be copied.
        inline Angle::Angle(const Angle& angle)
            : mRadian(angle.mRadian)
        {}

        // -----------------------------------------------------------------------------
        // Assignment operator. Assigns the value of another Angle to this Angle.
        // @param angle: The Angle instance to be assigned.
        inline Angle& Angle::operator=(const Angle& angle)
        {
            mRadian = angle.mRadian;
            return *this;
        }

        // -----------------------------------------------------------------------------
        // Unary minus operator. Returns the negation of the Angle.
        inline Angle Angle::operator-() const
        {
            return Angle(-this->mRadian);
        }

        // -----------------------------------------------------------------------------
        // Addition operator. Adds another Angle to this Angle and returns the result.
        // @param angle: The Angle instance to be added.
        inline Angle Angle::operator+(const Angle& angle) const
        {
            return Angle(*this) += angle;
        }

        // -----------------------------------------------------------------------------
        // Subtraction operator. Subtracts another Angle from this Angle and returns the result.
        // @param angle: The Angle instance to be subtracted.
        inline Angle Angle::operator-(const Angle& angle) const
        {
            return Angle(*this) -= angle;
        }

        // -----------------------------------------------------------------------------
        // Multiplication operator. Multiplies the Angle by a scaler value and returns the result.
        // @param scaler: The value to scale the Angle by.
        inline Angle Angle::operator*(const float scaler) const
        {
            return Angle(*this) *= scaler;
        }

        // -----------------------------------------------------------------------------
        // Division operator. Divides the Angle by a scaler value and returns the result.
        // @param scaler: The value to divide the Angle by.
        inline Angle Angle::operator/(const float scaler) const
        {
            return Angle(*this) /= scaler;
        }


        // -----------------------------------------------------------------------------
        // Compound addition operator. Adds another Angle to this Angle and updates the value.
        // @param angle: The Angle instance to be added.
        inline Angle& Angle::operator+=(const Angle& angle)
        {
            mRadian += angle.mRadian;
            Normalize();
            return *this;
        }

        // -----------------------------------------------------------------------------
        // Compound subtraction operator. Subtracts another Angle from this Angle and updates the value.
        // @param angle: The Angle instance to be subtracted.
        inline Angle& Angle::operator-=(const Angle& angle)
        {
            mRadian -= angle.mRadian;
            Normalize();
            return *this;
        }

		// -----------------------------------------------------------------------------
        // Compound multiplication operator. Multiplies the Angle by a scaler value and updates the value.
        // @param scaler: The value to scale the Angle by.
        inline Angle& Angle::operator*=(const float scaler)
		{
			mRadian *= scaler;
			Normalize();
			return *this;
		}

		// -----------------------------------------------------------------------------
		// Divide and assign operator overload for Angle class
		// Divides the angle by the specified scaler and assigns the result back to the current angle.
		// Arguments:
		// - scaler: The value by which the angle should be divided
		inline Angle& Angle::operator /= (const float scaler)
		{
		    mRadian /= scaler;
		    Normalize();
		    return *this;
		}

		// -----------------------------------------------------------------------------
		// Equality comparison operator overload for Angle class
		// Compares the current angle with another angle for equality.
		// Arguments:
		// - angle: The angle to compare with
		inline bool Angle::operator ==(const Angle& angle) const
		{
		    return Dia::Maths::Float::FEqual(mRadian, angle.mRadian, 0.001f);
		}

		// -----------------------------------------------------------------------------
		// Inequality comparison operator overload for Angle class
		// Compares the current angle with another angle for inequality.
		// Arguments:
		// - angle: The angle to compare with
		inline bool Angle::operator !=(const Angle& angle) const
		{
		    return !Dia::Maths::Float::FEqual(mRadian, angle.mRadian, 0.001f);
		}

		// -----------------------------------------------------------------------------
		// Less than or equal to comparison operator overload for Angle class
		// Checks if the current angle is less than or equal to another angle.
		// Arguments:
		// - angle: The angle to compare with
		inline bool Angle::operator <=(const Angle& angle) const
		{
		    return Dia::Maths::Float::FLessEqual(mRadian, angle.mRadian);
		}

		// -----------------------------------------------------------------------------
		// Greater than or equal to comparison operator overload for Angle class
		// Checks if the current angle is greater than or equal to another angle.
		// Arguments:
		// - angle: The angle to compare with
		inline bool Angle::operator >=(const Angle& angle) const
		{
		    return Dia::Maths::Float::FGreaterEqual(mRadian, angle.mRadian);
		}

		// -----------------------------------------------------------------------------
		// Less than comparison operator overload for Angle class
		// Checks if the current angle is less than another angle.
		// Arguments:
		// - angle: The angle to compare with
		inline bool Angle::operator <(const Angle& angle) const
		{
		    return Dia::Maths::Float::FLess(mRadian, angle.mRadian);
		}

		// -----------------------------------------------------------------------------
		// Greater than comparison operator overload for Angle class
		// Checks if the current angle is greater than another angle.
		// Arguments:
		// - angle: The angle to compare with
		inline bool Angle::operator >(const Angle& angle) const
		{
		    return Dia::Maths::Float::FGreater(mRadian, angle.mRadian);
		}

		// -----------------------------------------------------------------------------
		// Returns the angle value in radians
		// Returns:
		// - The angle value in radians
		inline float Angle::AsRadians() const
		{
		    return mRadian;
		}

		// -----------------------------------------------------------------------------
		// Returns the angle value in degrees
		// Returns:
		// - The angle value in degrees
		inline float Angle::AsDegrees() const
		{
		    return Dia::Maths::RadiansToDeg(mRadian);
		}

		// -----------------------------------------------------------------------------
		// Returns the positive angle value in radians
		// If the angle is negative, it adds 180 degrees (Pi radians) to make it positive.
		// Returns:
		// - The positive angle value in radians
		inline float Angle::AsPositiveRadians() const
		{
		    return Dia::Maths::Float::FSelect(mRadian, mRadian, mRadian + Dia::Maths::PI);
		}

		// -----------------------------------------------------------------------------
		// Returns the positive angle value in degrees
		// If the angle is negative, it adds 180 degrees to make it positive.
		// Returns:
		// - The positive angle value in degrees
		inline float Angle::AsPositiveDegrees() const
		{
		    return Dia::Maths::RadiansToDeg(AsPositiveRadians());
		}

		// -----------------------------------------------------------------------------
		// Creates an Angle object from the specified angle value in degrees
		// Arguments:
		// - degrees: The angle value in degrees
		// Returns:
		// - An Angle object representing the specified angle value in radians
		inline Angle Angle::FromDegrees(const float degrees)
		{
		    return Angle(Dia::Maths::DegToRadians(degrees));
		}

		// -----------------------------------------------------------------------------
		// Creates an Angle object from the specified angle value in radians
		// Arguments:
		// - radians: The angle value in radians
		// Returns:
		// - An Angle object representing the specified angle value in radians
		inline Angle Angle::FromRadians(const float radians)
		{
		    return Angle(radians);
		}

		// -----------------------------------------------------------------------------
		// Sets the angle value based on the specified angle value in degrees
		// Arguments:
		// - degrees: The angle value in degrees
		inline void Angle::SetFromDegrees(const float degrees)
		{
		    mRadian = Dia::Maths::DegToRadians(degrees);
		}

		// -----------------------------------------------------------------------------
		// Normalizes the angle value to the range [-PI, PI]
		// If the angle is greater than PI, it subtracts 2*PI until it is within the range.
		// If the angle is smaller than -PI, it adds 2*PI until it is within the range.
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

			DIA_ASSERT(mRadian >= -Dia::Maths::PI && mRadian <= Dia::Maths::PI, "Radians outside range [-PI, PI]");
		}

		// -----------------------------------------------------------------------------
		// Multiply operator overload for Angle class
		// Multiplies the angle by the specified scaler
		// Arguments:
		// - scaler: The value by which the angle should be multiplied
		// - angle: The angle to be multiplied
		// Returns:
		// - The resulting angle after multiplication
		inline Angle operator * (const float scaler, const Angle& angle)
		{
		    return Angle(angle) *= scaler;
		}
}
}
