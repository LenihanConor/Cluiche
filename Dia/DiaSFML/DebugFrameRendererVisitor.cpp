////////////////////////////////////////////////////////////////////////////////
// Filename: DebugFrameRendererVisitor.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaSFML/DebugFrameRendererVisitor.h"

#include "DiaSFML/Conversion.h"

#include <DiaGraphics/Frame/DebugFrameData.h>
#include <DiaCore/Core/Assert.h>

#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>

#include <cmath>

#pragma warning( disable : 4800 )

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Dia
{
	namespace SFML
	{
		//------------------------------------------------------------------------------
		void DebugFrameRendererVisitor::Visit(const Dia::Graphics::DebugFrameData& object) const
		{
			mRenderTarget->pushGLStates();
			object.AcceptVisitor(*this);
			mRenderTarget->popGLStates();
		}

		//------------------------------------------------------------------------------
		void DebugFrameRendererVisitor::Visit(const Dia::Graphics::DebugPrimitive& p) const
		{
			switch (p.type)
			{
				case Dia::Graphics::DebugPrimitiveType::Circle2D:   DrawCircle2D(p.circle2D);     break;
				case Dia::Graphics::DebugPrimitiveType::Line2D:     DrawLine2D(p.line2D);         break;
				case Dia::Graphics::DebugPrimitiveType::Point2D:    DrawPoint2D(p.point2D);       break;
				case Dia::Graphics::DebugPrimitiveType::Rect2D:     DrawRect2D(p.rect2D);         break;
				case Dia::Graphics::DebugPrimitiveType::Arc2D:      DrawArc2D(p.arc2D);           break;
				case Dia::Graphics::DebugPrimitiveType::Ray2D:      DrawRay2D(p.ray2D);           break;
				case Dia::Graphics::DebugPrimitiveType::Triangle2D: DrawTriangle2D(p.triangle2D); break;
				default: DIA_ASSERT(0, "Unhandled DebugPrimitiveType in DebugFrameRendererVisitor"); break;
			}
		}

		//------------------------------------------------------------------------------
		void DebugFrameRendererVisitor::DrawCircle2D(const Dia::Graphics::DebugPrimitiveCircle2D& p) const
		{
			sf::CircleShape shape(p.radius);
			shape.setOutlineThickness(2.0f);
			shape.setPosition(sf::Vector2f(p.position.x - p.radius, p.position.y - p.radius));

			sf::Color outline;
			Convert(outline, p.outlineColour);
			shape.setOutlineColor(outline);

			sf::Color fill;
			Convert(fill, p.fillColour);
			shape.setFillColor(fill);

			mRenderTarget->draw(shape);
		}

		//------------------------------------------------------------------------------
		void DebugFrameRendererVisitor::DrawLine2D(const Dia::Graphics::DebugPrimitiveLine2D& p) const
		{
			sf::Color sfColour;
			Convert(sfColour, p.colour);

			sf::Vertex line[2];
			line[0].position = sf::Vector2f(p.start.x, p.start.y);
			line[0].color    = sfColour;
			line[1].position = sf::Vector2f(p.end.x, p.end.y);
			line[1].color    = sfColour;

			mRenderTarget->draw(line, 2, sf::PrimitiveType::Lines);
		}

		//------------------------------------------------------------------------------
		void DebugFrameRendererVisitor::DrawPoint2D(const Dia::Graphics::DebugPrimitivePoint2D& p) const
		{
			sf::CircleShape dot(2.0f);
			dot.setPosition(sf::Vector2f(p.position.x - 2.0f, p.position.y - 2.0f));

			sf::Color sfColour;
			Convert(sfColour, p.colour);
			dot.setFillColor(sfColour);
			dot.setOutlineThickness(0.0f);

			mRenderTarget->draw(dot);
		}

		//------------------------------------------------------------------------------
		void DebugFrameRendererVisitor::DrawRect2D(const Dia::Graphics::DebugPrimitiveRect2D& p) const
		{
			sf::Vector2f size(p.max.x - p.min.x, p.max.y - p.min.y);
			sf::RectangleShape shape(size);
			shape.setPosition(sf::Vector2f(p.min.x, p.min.y));
			shape.setOutlineThickness(2.0f);

			sf::Color outline;
			Convert(outline, p.outlineColour);
			shape.setOutlineColor(outline);

			sf::Color fill;
			Convert(fill, p.fillColour);
			shape.setFillColor(fill);

			mRenderTarget->draw(shape);
		}

		//------------------------------------------------------------------------------
		// static
		int DebugFrameRendererVisitor::ArcSegmentCount(float startDeg, float endDeg)
		{
			float span = endDeg - startDeg;
			if (span < 0.0f) span += 360.0f;
			if (span < 45.0f)  return 4;
			if (span < 90.0f)  return 8;
			if (span < 180.0f) return 12;
			return 16;
		}

		//------------------------------------------------------------------------------
		void DebugFrameRendererVisitor::DrawArc2D(const Dia::Graphics::DebugPrimitiveArc2D& p) const
		{
			int segments = ArcSegmentCount(p.startAngleDeg, p.endAngleDeg);

			sf::Color sfColour;
			Convert(sfColour, p.colour);

			float span = p.endAngleDeg - p.startAngleDeg;
			if (span < 0.0f) span += 360.0f;

			float stepDeg = span / static_cast<float>(segments);
			const float degToRad = static_cast<float>(M_PI) / 180.0f;

			for (int i = 0; i < segments; ++i)
			{
				float a0 = (p.startAngleDeg + stepDeg * static_cast<float>(i))     * degToRad;
				float a1 = (p.startAngleDeg + stepDeg * static_cast<float>(i + 1)) * degToRad;

				sf::Vertex line[2];
				line[0].position = sf::Vector2f(
					p.position.x + p.radius * std::cos(a0),
					p.position.y + p.radius * std::sin(a0));
				line[0].color = sfColour;

				line[1].position = sf::Vector2f(
					p.position.x + p.radius * std::cos(a1),
					p.position.y + p.radius * std::sin(a1));
				line[1].color = sfColour;

				mRenderTarget->draw(line, 2, sf::PrimitiveType::Lines);
			}
		}

		//------------------------------------------------------------------------------
		void DebugFrameRendererVisitor::DrawRay2D(const Dia::Graphics::DebugPrimitiveRay2D& p) const
		{
			sf::Color sfColour;
			Convert(sfColour, p.colour);

			sf::Vector2f origin(p.origin.x, p.origin.y);
			sf::Vector2f tip(
				p.origin.x + p.direction.x * p.length,
				p.origin.y + p.direction.y * p.length);

			sf::Vertex line[2];
			line[0].position = origin;
			line[0].color    = sfColour;
			line[1].position = tip;
			line[1].color    = sfColour;

			mRenderTarget->draw(line, 2, sf::PrimitiveType::Lines);
		}

		//------------------------------------------------------------------------------
		void DebugFrameRendererVisitor::DrawTriangle2D(const Dia::Graphics::DebugPrimitiveTriangle2D& p) const
		{
			sf::ConvexShape shape(3);
			shape.setPoint(0, sf::Vector2f(p.p1.x, p.p1.y));
			shape.setPoint(1, sf::Vector2f(p.p2.x, p.p2.y));
			shape.setPoint(2, sf::Vector2f(p.p3.x, p.p3.y));
			shape.setOutlineThickness(2.0f);

			sf::Color outline;
			Convert(outline, p.outlineColour);
			shape.setOutlineColor(outline);

			sf::Color fill;
			Convert(fill, p.fillColour);
			shape.setFillColor(fill);

			mRenderTarget->draw(shape);
		}

	} // namespace SFML
} // namespace Dia
