#ifndef HALFFLOAT_H
#define HALFFLOAT_H

namespace Dia
{
	namespace Maths
	{
		class HalfFloat
		{
		public:
			HalfFloat();
			explicit HalfFloat( float val );
			
			static HalfFloat		Create( float val );

			operator				float				() const;
			HalfFloat				operator -			() const;
			HalfFloat				operator +			( HalfFloat rhs ) const;
			HalfFloat				operator -			( HalfFloat rhs ) const;
			HalfFloat				operator *			( HalfFloat rhs ) const;
			HalfFloat				operator /			( HalfFloat rhs ) const;
			HalfFloat&				operator =			( HalfFloat h );
			HalfFloat&				operator +=			( HalfFloat h );
			HalfFloat&				operator -=			( HalfFloat h );
			HalfFloat&				operator *=			( HalfFloat h );
			HalfFloat&				operator /=			( HalfFloat h );

			bool					operator ==			( HalfFloat h );
			bool					operator !=			( HalfFloat h );

			HalfFloat				operator +			( float rhs ) const;
			HalfFloat				operator -			( float rhs ) const;
			HalfFloat				operator *			( float rhs ) const;
			HalfFloat				operator /			( float rhs ) const;
			HalfFloat&				operator =			( float h );
			HalfFloat&				operator +=			( float h );
			HalfFloat&				operator -=			( float h );
			HalfFloat&				operator *=			( float h );
			HalfFloat&				operator /=			( float h );

			bool					operator ==			( float h );
			bool					operator !=			( float h );
			
			HalfFloat				Round					( unsigned int n ) const;

			bool					IsFinite				() const;
			bool					IsNormalized			() const;
			bool					IsDenormalized			() const;
			bool					IsZero					() const;
			bool					IsNaN					() const;
			bool					IsInfinity				() const;
			bool					IsNegative				() const;

			static HalfFloat        PositiveInfinity		();
			static HalfFloat        NegativeInfinity		();
			static HalfFloat		QNaN					();
			static HalfFloat        SNaN					();
			static HalfFloat        Min						();
			static HalfFloat        Max						();
			static HalfFloat        MinNormalized			();
			static HalfFloat        MinEpsilon				();
			static HalfFloat		Epsilon					();
			static int				NumberDigits			();
		
			unsigned short			Bits					() const;
			void					SetBits					( unsigned short bits );
			
			float					ToFloat					() const;
			
		private:

			union uif
			{
				unsigned int i;
				float f;
			};
			
			static short				Convert( int i );
			static float				Overflow ();

			unsigned short				mHalfFloat;

			static const unsigned short lookupTable[ 1 << 9 ];
		};
	}
};

#include "DiaMaths/Core/HalfFloat.inl"

#endif
