////////////////////////////////////////////////////////////////////////////////
// Filename: Canvas
////////////////////////////////////////////////////////////////////////////////
#include "DiaSFML/DebugFrameRendererVisitor.h"

#include "DiaSFML/Conversion.h"

#include <DiaGraphics/Frame/FrameData.h>

#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#pragma warning( disable : 4800 )

namespace Dia
{
	namespace SFML
	{
		void DebugFrameRendererVisitor::Visit(const Dia::Graphics::DebugFrameData& object)const
		{
			mRenderTarget->pushGLStates();

			object.AcceptVisitor(*this);

			mRenderTarget->popGLStates();
		}

		void DebugFrameRendererVisitor::Visit(const Dia::Graphics::DebugFrameDataCircle2D& object)const
		{
			sf::CircleShape shape(object.GetRadius());

			shape.setOutlineThickness(2);

			// Set the position, we need to offset the radius so that the center is the position of the circle
			shape.setPosition(sf::Vector2f(object.GetPosition().x - object.GetRadius(), object.GetPosition().y - object.GetRadius()));
			
			// Set the colour
			sf::Color sfColour;
			Convert(sfColour, object.GetColour());
			shape.setFillColor(sf::Color(0,0,0,0));
			shape.setOutlineColor(sfColour);

			// Now draw the circle
			mRenderTarget->draw(shape);
		}

		void DebugFrameRendererVisitor::Visit(const Dia::Graphics::DebugFrameDataLine2D& object)const
		{
			sf::Color sfColour;
			Convert(sfColour, object.GetColour());
			
			sf::Vertex line[] =
			{
				sf::Vertex(sf::Vector2f(object.GetPosition1().x, object.GetPosition1().y), sfColour),
				sf::Vertex(sf::Vector2f(object.GetPosition2().x, object.GetPosition2().y), sfColour)
			};

			// Now draw the circle
			mRenderTarget->draw(line, 2, sf::Lines);
		}
	}
}