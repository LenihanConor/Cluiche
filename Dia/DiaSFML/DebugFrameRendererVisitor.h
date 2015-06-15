////////////////////////////////////////////////////////////////////////////////
// Filename: DebugRenderer.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGraphics/Frame/DebugFrameDataVisitor.h>

// Forward Declare
namespace sf
{
	class RenderWindow;
}

namespace Dia
{
	namespace Graphics
	{
		class DebugFrameData;
		class DebugFrameDataCircle2D;
		class DebugFrameDataLine2D;
	};

	namespace SFML
	{
		////////////////////////////////////////////////////////////////////////////////
		// Class name: DebugFrameRendererVisitor - How to render debug objects through SFML from the frame data
		////////////////////////////////////////////////////////////////////////////////
		class DebugFrameRendererVisitor : public Dia::Graphics::DebugFrameDataVisitor
		{
		public:
			DebugFrameRendererVisitor(sf::RenderWindow* renderWindow) : mRenderWindow(renderWindow){}

			virtual void Visit(const Dia::Graphics::DebugFrameData& object)const override;

		private:
			virtual void Visit(const Dia::Graphics::DebugFrameDataCircle2D& object)const override;
			virtual void Visit(const Dia::Graphics::DebugFrameDataLine2D& object)const override;

			sf::RenderWindow* mRenderWindow;
		};
	}
}