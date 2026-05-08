////////////////////////////////////////////////////////////////////////////////
// Filename: Conversion
////////////////////////////////////////////////////////////////////////////////
#include "DiaSFML/Conversion.h"

#include <SFML/Graphics.hpp>

#include <DiaGraphics\Misc\RGBA.h>
#include <DiaGraphics\Misc\Vertex.h>
#include <DiaGraphics\Misc\Transform.h>
#include <DiaCore/Core/Log.h>
#include <DiaGraphics\Misc\RenderStates.h>
#include <DiaGraphics\Misc\PrimitiveType.h>
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

		//-------------------------------------------------------------------
		void Convert(sf::Vertex& lhs, const Dia::Graphics::Vertex& rhs)
		{
			sf::Vector2f position;
			Convert(position, rhs.position);
			lhs.position = position;

			sf::Color color;
			Convert(color, rhs.color);
			lhs.color = color;

			sf::Vector2f texCoords;
			Convert(texCoords, rhs.texCoords);
			lhs.texCoords = texCoords;
		}

		//-------------------------------------------------------------------
		void Convert(Dia::Graphics::Vertex& lhs, const sf::Vertex& rhs)
		{
			Convert(lhs.position, rhs.position);
			// Can't convert color back due to linking issue mentioned above
			// lhs.color would need to be set manually
			Convert(lhs.texCoords, rhs.texCoords);
		}

		//-------------------------------------------------------------------
		void Convert(sf::Transform& lhs, const Dia::Graphics::Transform& rhs)
		{
			const float* matrix = rhs.GetMatrix();
			lhs = sf::Transform(matrix[0], matrix[4], matrix[12],
								matrix[1], matrix[5], matrix[13],
								matrix[3], matrix[7], matrix[15]);
		}

		//-------------------------------------------------------------------
		void Convert(Dia::Graphics::Transform& lhs, const sf::Transform& rhs)
		{
			const float* matrix = rhs.getMatrix();
			lhs = Dia::Graphics::Transform(matrix[0], matrix[4], matrix[12],
										   matrix[1], matrix[5], matrix[13],
										   matrix[3], matrix[7], matrix[15]);
		}

		//-------------------------------------------------------------------
		// Internal helper function - not exposed in header to avoid forward declaration issues
		static sf::BlendMode ConvertBlendMode(const Dia::Graphics::RenderStates::BlendMode& rhs)
		{
			switch (rhs)
			{
			case Dia::Graphics::RenderStates::BlendMode::Alpha:
				return sf::BlendAlpha;
			case Dia::Graphics::RenderStates::BlendMode::Add:
				return sf::BlendAdd;
			case Dia::Graphics::RenderStates::BlendMode::Multiply:
				return sf::BlendMultiply;
			case Dia::Graphics::RenderStates::BlendMode::None:
				return sf::BlendNone;
			default:
				Dia::Core::Log::OutputVaradicLine("[WARNING][Graphics] Unknown BlendMode value %d, defaulting to Alpha", static_cast<int>(rhs));
				return sf::BlendAlpha;
			}
		}

		//-------------------------------------------------------------------
		void Convert(sf::RenderStates& lhs, const Dia::Graphics::RenderStates& rhs)
		{
			// Convert blend mode using internal helper
			lhs.blendMode = ConvertBlendMode(rhs.blendMode);

			// Convert transform
			sf::Transform transform;
			Convert(transform, rhs.transform);
			lhs.transform = transform;

			// Texture and shader are handled via native handles
			// These would need to be set by the backend implementation
			lhs.texture = nullptr;
			lhs.shader = nullptr;
		}

		//-------------------------------------------------------------------
		// Internal helper function - not exposed in header to avoid forward declaration issues
		static void ConvertPrimitiveType(sf::PrimitiveType& lhs, const Dia::Graphics::PrimitiveType& rhs)
		{
			switch (rhs)
			{
			case Dia::Graphics::PrimitiveType::Points:
				lhs = sf::PrimitiveType::Points;
				break;
			case Dia::Graphics::PrimitiveType::Lines:
				lhs = sf::PrimitiveType::Lines;
				break;
			case Dia::Graphics::PrimitiveType::LineStrip:
				lhs = sf::PrimitiveType::LineStrip;
				break;
			case Dia::Graphics::PrimitiveType::Triangles:
				lhs = sf::PrimitiveType::Triangles;
				break;
			case Dia::Graphics::PrimitiveType::TriangleStrip:
				lhs = sf::PrimitiveType::TriangleStrip;
				break;
			case Dia::Graphics::PrimitiveType::TriangleFan:
				lhs = sf::PrimitiveType::TriangleFan;
				break;
			default:
				lhs = sf::PrimitiveType::Triangles;
				break;
			}
		}
	}
}