#pragma once

namespace Dia
{
	namespace Maths
	{
		class Vector3D
		{
		public:
			static const Vector3D& XAxis();
            static const Vector3D& YAxis();
			static const Vector3D& ZAxis();
			static const Vector3D& Zero();
			static const Vector3D& Max();
			static const Vector3D& Min();

			Vector3D(); 
			Vector3D(float X, float Y, float Z);
			Vector3D(float number);
			Vector3D(const Vector3D& vVector);		
			
			Vector3D&		operator=			(const Vector3D& vVector);
			Vector3D&		operator+=			(const Vector3D& v);
			Vector3D&		operator-=			(const Vector3D& v);
			Vector3D&		operator*=			(const Vector3D& v);
			Vector3D&		operator/=			(const Vector3D& v);
			Vector3D&		operator*=			(float value);
			Vector3D&		operator/=			(float value);

			Vector3D		operator +			() const;
			Vector3D		operator -			() const;

			Vector3D		operator +			( const Vector3D& v) const;
			Vector3D		operator -			( const Vector3D& v) const;
			Vector3D		operator *			( const Vector3D& v) const;
			Vector3D		operator /			( const Vector3D& ) const;
			Vector3D		operator *			( const float value ) const;
			Vector3D		operator /			( const float value) const;
			
			float&			operator []			(int index);
			float			operator []			(int index) const;

			bool			operator==			(const Vector3D& v);	
			bool			operator!=			(const Vector3D& v);
			
			float			X					() const; 			
			float			Y					() const; 	
			float			Z					() const;
			void			X					( float newX );
			void			Y					( float newY );
			void			Z					( float newZ );

			void			Set					( float X, float Y, float Z );	
			void			Set					( const Vector3D& rhs );	

			Vector3D&		Clear				();
			Vector3D&		Invert				();
			Vector3D		AsInverse			() const;
			bool			IsNormal			() const;
			Vector3D		AsNormal			() const;
            Vector3D&		Normalize			();
			Vector3D		AsNormalSafe		() const;
            Vector3D&		NormalizeSafe		();
			Vector3D		Absolute			()const;
			Vector3D&		Absolutize			();
			float			Dot					( const Vector3D& ) const;
			float			SquareMagnitude		() const;
			float			Magnitude			() const;
			float			DistanceTo			( const Vector3D& ) const;
			float			SquareDistanceTo	( const Vector3D& ) const;
			float			ManhattanDistanceTo	( const Vector3D& ) const;
			Vector3D		ProjectOn			( const Vector3D& ) const;
			bool		    IsValid				() const;

			float x, y, z;
		};
	}
}
