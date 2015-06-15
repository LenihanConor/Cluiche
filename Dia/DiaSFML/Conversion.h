////////////////////////////////////////////////////////////////////////////////
// Filename: Conversion.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <SFML\System\Vector2.hpp>

namespace sf
{
	class Color;
}

namespace Dia
{
	namespace Graphics
	{
		class RGBA;
	}

	// Forward Delarations
	namespace Maths
	{
		class Vector2D;
	}

	namespace SFML
	{
		// Colour Conversions
		void Convert(sf::Color& lhs, const Dia::Graphics::RGBA& rhs);
		//void Convert(Dia::Graphics::RGBA& lhs, const sf::Color& rhs); // TODO: This is not linking, i have no idea why?

		// Vector2D Conversions
		void Convert(sf::Vector2i& lhs, const Dia::Maths::Vector2D& rhs);
		void Convert(Dia::Maths::Vector2D& lhs, const sf::Vector2i& rhs);
		void Convert(sf::Vector2u& lhs, const Dia::Maths::Vector2D& rhs);
		void Convert(Dia::Maths::Vector2D& lhs, const sf::Vector2u& rhs);
		void Convert(sf::Vector2f& lhs, const Dia::Maths::Vector2D& rhs);
		void Convert(Dia::Maths::Vector2D& lhs, const sf::Vector2f& rhs);
	}
}