#ifndef ANGLE_H
#define ANGLE_H

namespace Dia
{
	namespace Maths
	{
        ////////////////////////////////////// class Angle //////////////////////////////////////

        class Angle
        {
		public:
							Angle			();
							Angle			( const Angle& angle );
			explicit		Angle			( const float radian );

            Angle&			operator =		( const Angle& angle );
            Angle			operator -		() const;
            Angle			operator +		( const Angle& angle) const;
            Angle			operator -		( const Angle& angle) const;
			Angle			operator *		( const float scaler) const;
			Angle			operator /		( const float scaler) const;

            inline          Angle&			operator +=		( const Angle& angle);
            inline          Angle&			operator -=		( const Angle& angle);
            inline          Angle&			operator *=		( const float scaler);
            inline          Angle&			operator /=		( const float scaler);

			bool			operator ==		( const Angle& angle) const;
			bool			operator !=		( const Angle& angle) const;
			bool			operator <=		( const Angle& angle) const;
			bool			operator >=		( const Angle& angle) const;
			bool			operator <		( const Angle& angle) const;
			bool			operator >		( const Angle& angle) const;

            float			AsRadians() const;
            float			AsDegrees() const;
            float			AsPositiveRadians() const;
            float			AsPositiveDegrees() const;
            inline  void	Normalize();

			static Angle	FromDegrees(const float degrees);
			static Angle	FromRadians(const float radians);

        private:
			void			SetFromDegrees(const float degrees);

            float			mRadian;

		public:
			static const Angle	Deg0;
			static const Angle	DegHalf;
			static const Angle	Deg1;
			static const Angle	Deg5;
			static const Angle	Deg10;
			static const Angle	Deg15;
			static const Angle	Deg30;
			static const Angle	Deg45;
			static const Angle	Deg60;
			static const Angle	Deg90;
			static const Angle	Deg120;
			static const Angle	Deg135;
			static const Angle	Deg180;
			static const Angle	Deg270;
			static const Angle	Deg360;
        };
	} 
} 

#include "DiaMaths/Core/Angle.inl"

#endif // __CORE_ANGLE_H
