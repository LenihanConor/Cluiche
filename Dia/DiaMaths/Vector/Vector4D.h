#pragma once

namespace Dia
{
	namespace Maths
	{
		class Vector4D
		{
		public:
			static const Vector4D& XAxis();
            static const Vector4D& YAxis();
			static const Vector4D& ZAxis();
			static const Vector4D& Zero();
			static const Vector4D& Max();
			static const Vector4D& Min();

			Vector4D(); 
			Vector4D(float X, float Y, float Z, float W);
			Vector4D(float number);
			Vector4D(const Vector4D& vVector);		
			
			Vector4D&		operator=			(const Vector4D& vVector);
			Vector4D&		operator+=			(const Vector4D& v);
			Vector4D&		operator-=			(const Vector4D& v);
			Vector4D&		operator*=			(const Vector4D& v);
			Vector4D&		operator/=			(const Vector4D& v);
			Vector4D&		operator*=			(float value);
			Vector4D&		operator/=			(float value);

			Vector4D		operator +			() const;
			Vector4D		operator -			() const;

			Vector4D		operator +			( const Vector4D& v) const;
			Vector4D		operator -			( const Vector4D& v) const;
			Vector4D		operator *			( const Vector4D& v) const;
			Vector4D		operator /			( const Vector4D& ) const;
			Vector4D		operator *			( const float value ) const;
			Vector4D		operator /			( const float value) const;
			
			float&			operator []			(int index);
			float			operator []			(int index) const;

			bool			operator==			(const Vector4D& v);	
			bool			operator!=			(const Vector4D& v);
			
			float			X					() const; 			
			float			Y					() const; 	
			float			Z					() const;
			float			W					() const;
			void			X					( float newX );
			void			Y					( float newY );
			void			Z					( float newZ );
			void			W					( float newW );

			void			Set					( float X, float Y, float Z, float W );	
			void			Set					( const Vector4D& rhs );	

			Vector4D&		Clear				();
			Vector4D&		Invert				();
			Vector4D		AsInverse			() const;
			bool			IsNormal			() const;
			Vector4D		AsNormal			() const;
            Vector4D&		Normalize			();
			Vector4D		AsNormalSafe		() const;
            Vector4D&		NormalizeSafe		();
			Vector4D		Absolute			()const;
			Vector4D&		Absolutize			();
			float			Dot					( const Vector4D& ) const;
			float			SquareMagnitude		() const;
			float			Magnitude			() const;
			float			DistanceTo			( const Vector4D& ) const;
			float			SquareDistanceTo	( const Vector4D& ) const;
			float			ManhattanDistanceTo	( const Vector4D& ) const;
			Vector4D		ProjectOn			( const Vector4D& ) const;
			bool		    IsValid				() const;

			float x, y, z, w;
		};
	}
}
