#include "DiaMaths/Core/CoreMaths.h"

namespace Dia
{
	namespace Maths
	{
		// -----------------------------------------------------------------------------
		inline 
		HalfFloat::HalfFloat()
			: mHalfFloat(0) 
		{}

		// -----------------------------------------------------------------------------
		inline 
		HalfFloat::HalfFloat( float f )
		{
			if (f == 0)
			{
				mHalfFloat = 0;
			}
			else
			{
				uif x;

				x.f = f;

				register int e = (x.i >> 23) & 0x000001ff;

				e = lookupTable[e];

				if (e)
				{
					mHalfFloat = e + (((x.i & 0x007fffff) + 0x00001000) >> 13);
				}
				else
				{
					mHalfFloat = Convert (x.i);
				}
			}
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat HalfFloat::Create( float f )
		{
			HalfFloat result;
			if (f == 0)
			{
				result.mHalfFloat = 0;
			}
			else
			{
				uif x;

				x.f = f;

				register int e = (x.i >> 23) & 0x000001ff;

				e = lookupTable[e];

				if (e)
				{
					result.mHalfFloat = e + (((x.i & 0x007fffff) + 0x00001000) >> 13);
				}
				else
				{
					result.mHalfFloat = Convert (x.i);
				}
			}
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat::operator float () const
		{
			return ToFloat();
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat HalfFloat::operator - () const
		{
			HalfFloat h;
			h.mHalfFloat = mHalfFloat ^ 0x8000;
			return h;
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat HalfFloat::operator + ( HalfFloat rhs ) const
		{
			return HalfFloat( float( *this ) + float( rhs ) );
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat HalfFloat::operator - ( HalfFloat rhs ) const
		{
			return HalfFloat( float( *this ) - float( rhs ) );
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat HalfFloat::operator * ( HalfFloat rhs ) const
		{
			return HalfFloat( float( *this ) * float( rhs ) );
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat HalfFloat::operator / ( HalfFloat rhs ) const
		{
			return HalfFloat( float( *this ) / float( rhs ) );
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat& HalfFloat::operator = ( HalfFloat h )
		{
			mHalfFloat = h.mHalfFloat;
			return *this;
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat& HalfFloat::operator += ( HalfFloat h )
		{
			*this = HalfFloat( float( *this ) + float( h ) );
			return *this;
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat& HalfFloat::operator -= ( HalfFloat h )
		{
			*this = HalfFloat( float( *this ) - float( h ) );
			return *this;
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat& HalfFloat::operator *= ( HalfFloat h )
		{
			*this = HalfFloat( float( *this ) * float( h ) );
			return *this;
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat& HalfFloat::operator /= ( HalfFloat h )
		{
			*this = HalfFloat( float( *this ) / float( h ) );
			return *this;
		}

		// -----------------------------------------------------------------------------
		inline
		bool HalfFloat::operator == ( HalfFloat h )
		{
			return (mHalfFloat == h.mHalfFloat);
		}

		// -----------------------------------------------------------------------------
		inline
		bool HalfFloat::operator != ( HalfFloat h )
		{
			return !(mHalfFloat == h.mHalfFloat);
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat HalfFloat::operator + ( float rhs ) const
		{
			return HalfFloat( float( *this ) + rhs );
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat HalfFloat::operator - ( float rhs ) const
		{
			return HalfFloat( float( *this ) - rhs );
		}

		// -----------------------------------------------------------------------------
		inline
			HalfFloat HalfFloat::operator * ( float rhs ) const
		{
			return HalfFloat( float( *this ) * rhs );
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat HalfFloat::operator / ( float rhs ) const
		{
			return HalfFloat( float( *this ) / rhs );
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat& HalfFloat::operator = ( float h )
		{
			*this = HalfFloat(h);
			return *this;
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat& HalfFloat::operator += ( float h )
		{
			*this = HalfFloat( float( *this ) +  h );
			return *this;
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat& HalfFloat::operator -= ( float h )
		{
			*this = HalfFloat( float( *this ) -  h );
			return *this;
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat& HalfFloat::operator *= ( float h )
		{
			*this = HalfFloat( float( *this ) *  h );
			return *this;
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat& HalfFloat::operator /= ( float h )
		{
			*this = HalfFloat( float( *this ) / h );
			return *this;
		}

		// -----------------------------------------------------------------------------
		inline
		bool HalfFloat::operator == ( float h )
		{
			HalfFloat a(h);
			return (mHalfFloat == a.mHalfFloat);
		}

		// -----------------------------------------------------------------------------
		inline
		bool HalfFloat::operator != ( float h )
		{
			HalfFloat a(h);
			return !(mHalfFloat == a.mHalfFloat);
		}

		// -----------------------------------------------------------------------------
		inline
		bool HalfFloat::IsFinite() const
		{
			unsigned short e = (mHalfFloat >> 10) & 0x001f;
			return e < 31;
		}

		// -----------------------------------------------------------------------------
		inline
		bool HalfFloat::IsNormalized() const
		{
			unsigned short e = (mHalfFloat >> 10) & 0x001f;
			return e > 0 && e < 31;
		}

		// -----------------------------------------------------------------------------
		inline
		bool HalfFloat::IsDenormalized() const
		{
			unsigned short e = (mHalfFloat >> 10) & 0x001f;
			unsigned short m =  mHalfFloat & 0x3ff;
			return e == 0 && m != 0;
		}

		// -----------------------------------------------------------------------------
		inline
		bool HalfFloat::IsZero() const
		{
			return (mHalfFloat & 0x7fff) == 0;
		}

		// -----------------------------------------------------------------------------
		inline
		bool HalfFloat::IsNaN() const
		{
			unsigned short e = (mHalfFloat >> 10) & 0x001f;
			unsigned short m =  mHalfFloat & 0x3ff;
			return e == 31 && m != 0;
		}

		// -----------------------------------------------------------------------------
		inline
		bool HalfFloat::IsInfinity() const
		{
			unsigned short e = (mHalfFloat >> 10) & 0x001f;
			unsigned short m =  mHalfFloat & 0x3ff;
			return e == 31 && m == 0;
		}

		// -----------------------------------------------------------------------------
		inline
		bool HalfFloat::IsNegative() const
		{
			return (mHalfFloat & 0x8000) != 0;
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat HalfFloat::PositiveInfinity()
		{
			HalfFloat h;
			h.mHalfFloat = 0x7c00;
			return h;
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat HalfFloat::NegativeInfinity ()
		{
			HalfFloat h;
			h.mHalfFloat = 0xfc00;
			return h;
		}
		
		// -----------------------------------------------------------------------------
		inline
		HalfFloat HalfFloat::QNaN()
		{
			HalfFloat h;
			h.mHalfFloat = 0x7fff;
			return h;
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat HalfFloat::SNaN()
		{
			HalfFloat h;
			h.mHalfFloat = 0x7dff;
			return h;
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat HalfFloat::Min()
		{
			static HalfFloat min(5.96046448e-08f);
			return min;
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat HalfFloat::Max()
		{
			static HalfFloat max(65504.0f);
			return max;
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat HalfFloat::MinNormalized()
		{
			static HalfFloat minNormalized(6.10351562e-05f);
			return minNormalized;
		}

		// -----------------------------------------------------------------------------
		inline
		HalfFloat HalfFloat::MinEpsilon()
		{
			static HalfFloat epsilon(6.10351562e-05f);
			return epsilon;
		}

		// -----------------------------------------------------------------------------
		inline
		int HalfFloat::NumberDigits()
		{
			return 11;
		}
		
		// -----------------------------------------------------------------------------
		inline
		unsigned short HalfFloat::Bits() const
		{
			return mHalfFloat;
		}

		// -----------------------------------------------------------------------------
		inline
		void HalfFloat::SetBits( unsigned short bits )
		{
			mHalfFloat = bits;
		}
	}
}