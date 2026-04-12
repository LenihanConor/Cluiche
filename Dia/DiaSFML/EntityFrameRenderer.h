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
		class TextureManager;
		////////////////////////////////////////////////////////////
		/// \brief Renders entity/sprite frame data using SFML
		///
		/// Concrete visitor that processes EntityFrameData and
		/// renders sprites with automatic texture batching.
		////////////////////////////////////////////////////////////
		class EntityFrameRenderer : public Graphics::EntityFrameDataVisitor
		{
		public:
			////////////////////////////////////////////////////////////
			/// \brief Constructor
			///
			/// \param target SFML render target to draw to
			/// \param textureManager Texture manager for loading sprites
			////////////////////////////////////////////////////////////
			EntityFrameRenderer(sf::RenderTarget* target, TextureManager* textureManager);

			////////////////////////////////////////////////////////////
			/// \brief Visit and render entity frame data
			///
			/// \param data Frame data containing sprite draw commands
			////////////////////////////////////////////////////////////
			void Visit(const Graphics::EntityFrameData& data) const override;

		private:
			sf::RenderTarget* mTarget; ///< SFML render target
			TextureManager* mTextureManager; ///< Texture manager for sprite textures
		};
	}
}
