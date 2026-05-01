////////////////////////////////////////////////////////////////////////////////
// Filename: Conversion.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <SFML\System\Vector2.hpp>

// Forward declare SFML types to minimize header dependencies
namespace sf
{
	class Color;
	struct Vertex;
	class Transform;
	struct RenderStates;
}

namespace Dia
{
	namespace Graphics
	{
		class RGBA;
		class Vertex;
		class Transform;
		class RenderStates;
		enum class PrimitiveType;
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

		// Graphics Type Conversions
		// Note: These functions are implemented in Conversion.cpp which includes full SFML headers
		void Convert(sf::Vertex& lhs, const Dia::Graphics::Vertex& rhs);
		void Convert(Dia::Graphics::Vertex& lhs, const sf::Vertex& rhs);
		void Convert(sf::Transform& lhs, const Dia::Graphics::Transform& rhs);
		void Convert(Dia::Graphics::Transform& lhs, const sf::Transform& rhs);
		void Convert(sf::RenderStates& lhs, const Dia::Graphics::RenderStates& rhs);

		// BlendMode and PrimitiveType conversion (SFML types not forward-declared to avoid conflicts)
		// These are implemented in Conversion.cpp where SFML headers are included
		// PrimitiveType conversion is available internally but not exported to avoid enum forward-declaration issues
	}
}