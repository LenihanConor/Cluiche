#pragma once

#include "DiaCore/Type/TypeDeclarationMacros.h"

namespace Dia
{
	namespace Maths
	{
		class Vector2D;
		class Angle;

		class Matrix22
		{
		public:	
			DIA_TYPE_DECLARATION;

			Matrix22(); 
			Matrix22(float e00, float e01, float e10, float e11);
			Matrix22(const Vector2D& xAxis, const Vector2D& yAxis);
			Matrix22(const Matrix22& matrix);
			
			static Matrix22	FromAngleClockwise			( const Angle& angle );
			static Matrix22	FromAngleCounterClockwise	( const Angle& angle );
			static Matrix22	FromScale					( const float scale );
			static Matrix22 FromShearX					( const float shear );
			static Matrix22 FromShearY					( const float shear );
			static Matrix22 FromReflectionAxis			( const Vector2D& axis );
			static Matrix22 FromProjectionAxis			( const Vector2D& axis );

			void			Set					( float e00, float e01, float e10, float e11 );	
			void			Set					( const Vector2D& xAxis, const Vector2D& yAxis );
			void			Set					( const Matrix22& rhs );	

			Matrix22&		operator =			(const Matrix22& rhs);

			Matrix22&		operator +=			(const Matrix22& rhs);
			Matrix22&		operator -=			(const Matrix22& rhs);
			Matrix22&		operator *=			(const Matrix22& rhs);
			Matrix22&		operator *=			(float scalar);
			Matrix22&		operator /=			(float scalar);
			
			Matrix22		operator -			() const;

			Matrix22		operator +			( const Matrix22& rhs) const;
			Matrix22		operator -			( const Matrix22& rhs) const;
			Matrix22		operator *			( const Matrix22& rhs) const;
			Matrix22		operator *			( const float scalar ) const;
			Matrix22		operator /			( const float scalar) const;
			
			bool			operator==			(const Matrix22& m)const;	
			bool			operator!=			(const Matrix22& m)const;
			
			Vector2D		operator *			( const Vector2D& rhs) const;

			float&			operator []			(int index);
			float			operator []			(int index) const;

			float			Element				( int index ) const ;
			float			Element				( int row, int column ) const ;
			void			SetElement			( int index, float value );
			void			SetElement			( int row, int column, float value );

			void			XAxis				(Vector2D& xAxis)const;
			void			YAxis				(Vector2D& yAxis)const;

			Matrix22&		Clear				();
			
			bool		    IsValid				() const;
			bool			IsSymmetric			() const; 
			bool			IsSkewSymmetric		() const;
			bool			IsIdentity			() const;
			bool			IsDiagonalMatrix	() const;
			bool			IsOrthogonal		() const;	
			bool			IsScaled			() const;
			bool			IsUniformScale		() const;
			bool			IsLeftHanded		() const;
			bool			IsRightHanded		() const;
			bool			IsAxisNormal		() const;

			float			Trace						() const;
			float			Determinant					() const;
			void			Eigenvalues					(float& eigenValue1, float& eigenValue2)const;
			void			GetScale					(float& scaleX, float& scaleY)const;
			void			GetRotationClockwise		(Angle& result)const;
			void			GetRotationCounterClockwise	(Angle& result)const;

			Matrix22&		LookAtRotation				(const Vector2D& lookFrom, const Vector2D& lookAt);

			Matrix22&		LeftHanded			();
			Matrix22		AsLeftHanded		() const;

			Matrix22&		RightHanded			();
			Matrix22		AsRightHanded		() const;
		
			Matrix22&		NormalAxis			();
			Matrix22		AsNormalAxis		() const;

			Matrix22&		Negative			();
			Matrix22		AsNegative			() const;

			Matrix22&		Transpose			();
			Matrix22		AsTranspose			() const;

			Matrix22&		Invert				();
			Matrix22&		InvertOrthogonal	();
			Matrix22		AsInverse			() const;
			Matrix22		AsInverseOrthogonal	() const;
			
			Matrix22&		UniformScale		( float scale );
			Matrix22		AsUniformScale		( float scale ) const;

			Matrix22&		Scale				( float scaleX, float scaleY );
			Matrix22		AsScale				( float scaleX, float scaleY )const;
			
			Matrix22&		RotateClockwise				( const Angle& angle );	
			Matrix22&		RotateCounterClockwise		( const Angle& angle );
			Matrix22		AsRotateClockwise			( const Angle& angle )const;
			Matrix22		AsRotateCounterClockwise	( const Angle& angle )const;

			Matrix22&		ReflectArbitraryAxis		( const Vector2D& axis );
			Matrix22		AsReflectArbitraryAxis		( const Vector2D& axis )const;
		
			Matrix22&		ProjectArbitraryAxis		( const Vector2D& axis );
			Matrix22		AsProjectArbitraryAxis		( const Vector2D& axis )const;
		
			Matrix22&		ShearXAxis					( float shear );
			Matrix22&		ShearYAxis					( float shear );
			Matrix22		AsShearXAxis				( float shear )const;
			Matrix22		AsShearYAxis				( float shear )const;
			
			Matrix22&		Orthogonalize				();
			Matrix22		AsOrthogonal				()const;

		private:
			int LinearIndex(int row, int coloumn)const;

			static const int kNumRows = 2;
			static const int kNumColoumns = 2;
			static const int kNumElements = kNumRows * kNumColoumns;
			
			float mElement[kNumElements]; 

		public:
			static const Matrix22 Zero;
			static const Matrix22 Identity;
			
			static const Matrix22 XAxisReflection;
			static const Matrix22 YAxisReflection;
			
			static const Matrix22 XAxisProjection;
			static const Matrix22 YAxisProjection;
		};
	}
}