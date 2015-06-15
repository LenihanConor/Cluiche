#pragma once

#include "DiaMaths/Core/MathsDefines.h"
#include "DiaMaths/Core/HalfFloat.h"

namespace Dia
{
	namespace Maths
	{
		class VectorHalf2D
		{
		public:
			static const VectorHalf2D& XAxis();
            static const VectorHalf2D& YAxis();
			static const VectorHalf2D& Zero();
			static const VectorHalf2D& Max();
			static const VectorHalf2D& Min();

			VectorHalf2D(); 
			VectorHalf2D(float X, float Y);
			VectorHalf2D(float number);
			VectorHalf2D(const VectorHalf2D& vVector);		
			
			VectorHalf2D&		operator =			(const VectorHalf2D& vVector);
			VectorHalf2D&		operator +=			(const VectorHalf2D& v);
			VectorHalf2D&		operator -=			(const VectorHalf2D& v);
			VectorHalf2D&		operator *=			(const VectorHalf2D& v);
			VectorHalf2D&		operator /=			(const VectorHalf2D& v);
			VectorHalf2D&		operator *=			(float value);
			VectorHalf2D&		operator /=			(float value);

			VectorHalf2D		operator -			() const;

			VectorHalf2D		operator +			( const VectorHalf2D& v) const;
			VectorHalf2D		operator -			( const VectorHalf2D& v) const;
			VectorHalf2D		operator *			( const VectorHalf2D& v) const;
			VectorHalf2D		operator /			( const VectorHalf2D& v) const;
			VectorHalf2D		operator *			( const float value ) const;
			VectorHalf2D		operator /			( const float value) const;
			
			HalfFloat&			operator []			(int index);
			HalfFloat			operator []			(int index) const;

			bool			operator==			(const VectorHalf2D& v)const;	
			bool			operator!=			(const VectorHalf2D& v)const;
			
			HalfFloat		X					() const; 			
			HalfFloat		Y					() const; 			
			void			X					( float newX );
			void			Y					( float newY );

			void			Set					( float X, float Y );	
			void			Set					( const VectorHalf2D& );	

			VectorHalf2D&		Clear				();
			
			bool				IsValid				() const;

			VectorHalf2D&		Invert				();
			VectorHalf2D		AsInverse			() const;

			bool				IsNormal			() const;
			VectorHalf2D		AsNormal			() const;
            VectorHalf2D&		Normalize			();
			VectorHalf2D		AsNormalSafe		() const;
            VectorHalf2D&		NormalizeSafe		();

			VectorHalf2D		Absolute			()const;
			VectorHalf2D&		Absolutize			();

			float				Dot					( const VectorHalf2D& ) const;
			
			float				SquareMagnitude		() const;
			float				Magnitude			() const;
			float				DistanceTo			( const VectorHalf2D& ) const;
			float				SquareDistanceTo	( const VectorHalf2D& ) const;
			float				ManhattanDistanceTo	( const VectorHalf2D& ) const;
			
			VectorHalf2D		AsSwitchedAxis		()const;
			VectorHalf2D&		SwitchAxis			();

			VectorHalf2D		AsProjectOnTo		( const VectorHalf2D& ) const;
			VectorHalf2D&		ProjectOnTo			( const VectorHalf2D& ) ;
		
			VectorHalf2D		AsRotated90DegreeClockwise			()const;
			VectorHalf2D&		Rotate90DegreeClockwise				();
			VectorHalf2D		AsRotated90DegreeCounterClockwise	()const;
			VectorHalf2D&		Rotate90DegreeCounterClockwise		();
			
			bool			IsClockwiseTo			( const VectorHalf2D& ) const;
			bool			IsCounterClockwiseTo	( const VectorHalf2D& ) const;
			bool			IsParallelTo			( const VectorHalf2D&, float accuracy = Dia::Maths::FLOAT_EPSILON ) const;
			
			HalfFloat x;
			HalfFloat y;
		};
	}
}

#include "DiaMaths/Vector/VectorHalf2D.inl"
