#pragma once

#include "DiaMaths/Core/MathsDefines.h"
#include "DiaCore/Type/TypeDeclarationMacros.h"

namespace Dia
{
	namespace Maths
	{
		class Angle;
		class Matrix22;

		class Vector2D
		{
		public:
			DIA_TYPE_DECLARATION;

			static const Vector2D& XAxis();
            static const Vector2D& YAxis();
			static const Vector2D& Zero();
			static const Vector2D& Max();
			static const Vector2D& Min();

			Vector2D(); 
			Vector2D(float X, float Y);
			Vector2D(float number);
			Vector2D(const Vector2D& vVector);		
			
			Vector2D&		operator =			(const Vector2D& vVector);
			Vector2D&		operator +=			(const Vector2D& v);
			Vector2D&		operator -=			(const Vector2D& v);
			Vector2D&		operator *=			(const Vector2D& v);
			Vector2D&		operator /=			(const Vector2D& v);
			Vector2D&		operator *=			(float value);
			Vector2D&		operator /=			(float value);

			Vector2D		operator -			() const;

			Vector2D		operator +			( const Vector2D& v) const;
			Vector2D		operator -			( const Vector2D& v) const;
			Vector2D		operator *			( const Vector2D& v) const;
			Vector2D		operator /			( const Vector2D& v) const;
			Vector2D		operator *			( const float value ) const;
			Vector2D		operator /			( const float value) const;
			
			float&			operator []			(int index);
			float			operator []			(int index) const;

			bool			operator==			(const Vector2D& v)const;	
			bool			operator!=			(const Vector2D& v)const;
			
			float			X					() const ; 			
			float			Y					() const; 			
			void			X					( float newX );
			void			Y					( float newY );

			void			Set					( float X, float Y );	
			void			Set					( const Vector2D& );	

			Vector2D&		Clear				();
			
			bool		    IsValid				() const;

			Vector2D&		Invert				();
			Vector2D		AsInverse			() const;

			bool			IsNormal			() const;
			Vector2D		AsNormal			() const;
            Vector2D&		Normalize			();
			Vector2D		AsNormalSafe		() const;
            Vector2D&		NormalizeSafe		();

			Vector2D		Absolute			()const;
			Vector2D&		Absolutize			();

			float			Dot					( const Vector2D& ) const;
			
			float			SquareMagnitude		() const;
			float			Magnitude			() const;
			float			DistanceTo			( const Vector2D& ) const;
			float			SquareDistanceTo	( const Vector2D& ) const;
			float			ManhattanDistanceTo	( const Vector2D& ) const;
			
			Vector2D		AsSwitchedAxis		()const;
			Vector2D&		SwitchAxis			();

			Vector2D		AsMultipliedBy		( const Matrix22& m) const;
			Vector2D&		MultipliedBy		( const Matrix22& m);

			Vector2D		AsProjectOnTo		( const Vector2D& ) const;
			Vector2D&		ProjectOnTo			( const Vector2D& ) ;
		
			Vector2D		AsRotateClockwiseBy			( const Angle& angle) const;
			Vector2D&		RotateClockwiseBy			( const Angle& angle);
			Vector2D		AsRotateCounterClockwiseBy	( const Angle& angle) const;
			Vector2D&		RotateCounterClockwiseBy	( const Angle& angle);

			Vector2D		AsRotated90DegreeClockwise			()const;
			Vector2D&		Rotate90DegreeClockwise				();
			Vector2D		AsRotated90DegreeCounterClockwise	()const;
			Vector2D&		Rotate90DegreeCounterClockwise		();

			void			GetAngleBetween					(const Vector2D& vec, Angle& result)const;		// Shortest Angle Between
			void			GetClockwiseAngleBetween		(const Vector2D& vec, Angle& result)const;
			void			GetCounterClockwiseAngleBetween	(const Vector2D& vec, Angle& result)const;
			
			void			GetRotationBetween				(const Vector2D& vec, Matrix22& result)const;

			bool			IsClockwiseTo			( const Vector2D& ) const;
			bool			IsCounterClockwiseTo	( const Vector2D& ) const;
			bool			IsParallelTo			( const Vector2D&, float accuracy = Dia::Maths::FLOAT_EPSILON ) const;
			
			float x, y;
		};
	}
}

#include "DiaMaths/Vector/Vector2D.inl"
