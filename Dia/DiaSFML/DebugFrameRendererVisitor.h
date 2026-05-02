////////////////////////////////////////////////////////////////////////////////
// Filename: DebugFrameRendererVisitor.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGraphics/Frame/DebugFrameDataVisitor.h>
#include <DiaGraphics/Frame/DebugPrimitive.h>

// Forward declare
namespace sf
{
	class RenderTarget;
}

namespace Dia
{
	namespace Graphics
	{
		class DebugFrameData;
	}

	namespace SFML
	{
		////////////////////////////////////////////////////////////////////////////////
		// DebugFrameRendererVisitor — renders debug primitives via SFML
		////////////////////////////////////////////////////////////////////////////////
		class DebugFrameRendererVisitor : public Dia::Graphics::DebugFrameDataVisitor
		{
		public:
			explicit DebugFrameRendererVisitor(sf::RenderTarget* renderTarget)
				: mRenderTarget(renderTarget) {}

			virtual void Visit(const Dia::Graphics::DebugFrameData& frameData) const override;
			virtual void Visit(const Dia::Graphics::DebugPrimitive& primitive) const override;

		private:
			void DrawCircle2D  (const Dia::Graphics::DebugPrimitiveCircle2D&   p) const;
			void DrawLine2D    (const Dia::Graphics::DebugPrimitiveLine2D&     p) const;
			void DrawPoint2D   (const Dia::Graphics::DebugPrimitivePoint2D&    p) const;
			void DrawRect2D    (const Dia::Graphics::DebugPrimitiveRect2D&     p) const;
			void DrawArc2D     (const Dia::Graphics::DebugPrimitiveArc2D&      p) const;
			void DrawRay2D     (const Dia::Graphics::DebugPrimitiveRay2D&      p) const;
			void DrawTriangle2D(const Dia::Graphics::DebugPrimitiveTriangle2D& p) const;

			static int ArcSegmentCount(float startDeg, float endDeg);

			sf::RenderTarget* mRenderTarget;
		};
	}
}
