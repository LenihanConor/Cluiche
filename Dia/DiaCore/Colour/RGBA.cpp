////////////////////////////////////////////////////////////////////////////////
// Filename: RGBA.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaCore/Colour/RGBA.h"

namespace Dia
{
	namespace Core
	{
		const RGBA RGBA::Black = RGBA(0, 0, 0, 255);
		const RGBA RGBA::White = RGBA(255, 255, 255, 255);
		const RGBA RGBA::Red = RGBA(255, 0, 0, 255);
		const RGBA RGBA::Green = RGBA(0, 255, 0, 255);
		const RGBA RGBA::Blue = RGBA(0, 0, 255, 255);
		const RGBA RGBA::Yellow = RGBA(255, 255, 0, 255);
		const RGBA RGBA::Magenta = RGBA(255, 0, 255, 255);
		const RGBA RGBA::Cyan = RGBA(0, 255, 255, 255);
		const RGBA RGBA::Transparent = RGBA(255, 255, 255, 0);

		//------------------------------------------------
		RGBA::RGBA()
			: r(0)
			, g(0)
			, b(0)
			, a(255)
		{}

		//------------------------------------------------
		RGBA::RGBA(const RGBA& rhs)
			: r(rhs.r)
			, g(rhs.g)
			, b(rhs.b)
			, a(rhs.a)
		{}

		//------------------------------------------------
		RGBA::RGBA(unsigned char red, unsigned char green, unsigned char blue, unsigned char alpha)
		{
			SetR(red);
			SetG(green);
			SetB(blue);
			SetA(alpha);
		}

		//------------------------------------------------
		void RGBA::SetR(unsigned char red)
		{
			r = (red < kMin) ? kMin : (red > kMax ? kMax : red);
		}

		//------------------------------------------------
		void RGBA::SetG(unsigned char green)
		{
			g = (green < kMin) ? kMin : (green > kMax ? kMax : green);
		}

		//------------------------------------------------
		void RGBA::SetB(unsigned char blue)
		{
			b = (blue < kMin) ? kMin : (blue > kMax ? kMax : blue);
		}

		//------------------------------------------------
		void RGBA::SetA(unsigned char alpha)
		{
			a = (alpha < kMin) ? kMin : (alpha > kMax ? kMax : alpha);
		}

		//------------------------------------------------
		RGBA& RGBA::operator =(const RGBA& rhs)
		{
			SetR(rhs.r);
			SetG(rhs.g);
			SetB(rhs.b);
			SetA(rhs.a);

			return *this;
		}

		//------------------------------------------------
		bool RGBA::operator ==(const RGBA& rhs)const
		{
			return (r == rhs.r || g == rhs.g || b == rhs.b || a == rhs.a);
		}

		//------------------------------------------------
		bool RGBA::operator !=(const RGBA& rhs)const
		{
			return (r != rhs.r || g != rhs.g || b != rhs.b || a != rhs.a);
		}

		//------------------------------------------------
		RGBA RGBA::operator +(const RGBA& rhs)const
		{
			RGBA newRGBA;

			newRGBA.SetR(this->r + rhs.r);
			newRGBA.SetG(this->g + rhs.g);
			newRGBA.SetB(this->b + rhs.b);
			newRGBA.SetA(this->a + rhs.a);

			return newRGBA;
		}

		//------------------------------------------------
		RGBA RGBA::operator -(const RGBA& rhs)const
		{
			RGBA newRGBA;

			newRGBA.SetR(this->r - rhs.r);
			newRGBA.SetG(this->g - rhs.g);
			newRGBA.SetB(this->b - rhs.b);
			newRGBA.SetA(this->a - rhs.a);

			return newRGBA;
		}

		//------------------------------------------------
		RGBA RGBA::operator *(const RGBA& rhs)const
		{
			RGBA newRGBA;

			newRGBA.SetR(this->r * rhs.r);
			newRGBA.SetG(this->g * rhs.g);
			newRGBA.SetB(this->b * rhs.b);
			newRGBA.SetA(this->a * rhs.a);

			return newRGBA;
		}

		//------------------------------------------------
		RGBA& RGBA::operator +=(const RGBA& rhs)
		{
			SetR(r + rhs.r);
			SetG(g + rhs.g);
			SetB(b + rhs.b);
			SetA(a + rhs.a);

			return *this;
		}

		//------------------------------------------------
		RGBA& RGBA::operator -=(const RGBA& rhs)
		{
			SetR(r - rhs.r);
			SetG(g - rhs.g);
			SetB(b - rhs.b);
			SetA(a - rhs.a);

			return *this;
		}

		//------------------------------------------------
		RGBA& RGBA::operator *=(const RGBA& rhs)
		{
			SetR(r * rhs.r);
			SetG(g * rhs.g);
			SetB(b * rhs.b);
			SetA(a * rhs.a);

			return *this;
		}
	}
}
