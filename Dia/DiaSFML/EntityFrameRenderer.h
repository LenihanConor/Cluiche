////////////////////////////////////////////////////////////////////////////////
// Filename: EntityFrameRenderer.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "DiaGraphics/Frame/EntityFrameDataVisitor.h"

namespace sf
{
	class RenderTarget;
}

namespace Dia
{
	namespace SFML
	{
		class TextureHandler;

		class EntityFrameRenderer : public Graphics::EntityFrameDataVisitor
		{
		public:
			EntityFrameRenderer(sf::RenderTarget* target, TextureHandler* textureHandler);

			void Visit(const Graphics::EntityFrameData& data) const override;

		private:
			sf::RenderTarget* mTarget;
			TextureHandler* mTextureHandler;
		};
	}
}
