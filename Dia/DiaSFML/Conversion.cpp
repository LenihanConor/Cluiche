////////////////////////////////////////////////////////////////////////////////
// Filename: Conversion
////////////////////////////////////////////////////////////////////////////////
#include "DiaSFML/Conversion.h"

#include <SFML/Graphics.hpp>

#include <DiaGraphics\Misc\RGBA.h>
#include <DiaMaths\Vector\Vector2D.h>

namespace Dia
{
	namespace SFML
	{
		//-------------------------------------------------------------------
		void Convert(sf::Color& lhs, const Dia::Graphics::RGBA& rhs)
		{
			lhs.r = rhs.R();
			lhs.g = rhs.G();
			lhs.b = rhs.B();
			lhs.a = rhs.A();
		}

		//-------------------------------------------------------------------
		/*void Convert(Dia::Graphics::RGBA& lhs, const sf::Color& rhs)
		{
			lhs.SetR(rhs.r);
			lhs.SetG(rhs.g);
			lhs.SetB(rhs.b);
			lhs.SetA(rhs.a);
		}*/

		//-------------------------------------------------------------------
		void Convert(sf::Vector2i& lhs, const Dia::Maths::Vector2D& rhs)
		{
			lhs.x = static_cast<int>(rhs.x);
			lhs.y = static_cast<int>(rhs.y);
		}

		//-------------------------------------------------------------------
		void Convert(Dia::Maths::Vector2D& lhs, const sf::Vector2i& rhs)
		{
			lhs.x = static_cast<float>(rhs.x);
			lhs.y = static_cast<float>(rhs.y);
		}

		//-------------------------------------------------------------------
		void Convert(sf::Vector2u& lhs, const Dia::Maths::Vector2D& rhs)
		{
			lhs.x = static_cast<unsigned int>(rhs.x);
			lhs.y = static_cast<unsigned int>(rhs.y);
		}

		//-------------------------------------------------------------------
		void Convert(Dia::Maths::Vector2D& lhs, const sf::Vector2u& rhs)
		{
			lhs.x = static_cast<float>(rhs.x);
			lhs.y = static_cast<float>(rhs.y);
		}

		//-------------------------------------------------------------------
		void Convert(sf::Vector2f& lhs, const Dia::Maths::Vector2D& rhs)
		{
			lhs.x = rhs.x;
			lhs.y = rhs.y;
		}

		//-------------------------------------------------------------------
		void Convert(Dia::Maths::Vector2D& lhs, const sf::Vector2f& rhs)
		{
			lhs.x = rhs.x;
			lhs.y = rhs.y;
		}
	}
}