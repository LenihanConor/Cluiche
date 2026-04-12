////////////////////////////////////////////////////////////////////////////////
// Filename: EntityFrameRenderer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaSFML/EntityFrameRenderer.h"
#include "DiaSFML/TextureManager.h"
#include "DiaGraphics/Frame/EntityFrameData.h"
#include "DiaGraphics/Frame/SpriteDrawCommand.h"
#include "DiaSFML/Conversion.h"

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Transform.hpp>

namespace Dia
{
	namespace SFML
	{
		////////////////////////////////////////////////////////////
		EntityFrameRenderer::EntityFrameRenderer(sf::RenderTarget* target, TextureManager* textureManager)
			: mTarget(target)
			, mTextureManager(textureManager)
		{
		}

		////////////////////////////////////////////////////////////
		void EntityFrameRenderer::Visit(const Graphics::EntityFrameData& data) const
		{
			const Core::Containers::DynamicArrayC<Graphics::SpriteDrawCommand, 256>& sprites = data.GetSprites();

			// TODO: Implement texture batching for optimization (Phase 2 enhancement)
			// For now, render sprites individually

			for (unsigned int i = 0; i < sprites.Size(); ++i)
			{
				const Graphics::SpriteDrawCommand& cmd = sprites[i];

				// Get texture from manager
				const sf::Texture* texture = mTextureManager->GetTexture(cmd.textureId);
				if (!texture)
				{
					// Skip sprites with invalid/missing textures
					continue;
				}

				// Create SFML sprite with texture
				sf::Sprite sprite(*texture);

				// Set position
				sprite.setPosition(sf::Vector2f(cmd.position.X(), cmd.position.Y()));

				// Set scale
				sprite.setScale(sf::Vector2f(cmd.scale.X(), cmd.scale.Y()));

				// Set rotation (convert degrees to SFML angle)
				sprite.setRotation(sf::degrees(cmd.rotation));

				// Set origin (pivot point)
				sprite.setOrigin(sf::Vector2f(cmd.origin.X(), cmd.origin.Y()));

				// Set color tint
				sprite.setColor(sf::Color(cmd.tint.R(), cmd.tint.G(), cmd.tint.B(), cmd.tint.A()));

				// Set texture rect if specified (non-zero size means use sub-rect)
				const Maths::Vector2D& bottomLeft = cmd.textureRect.GetBottomLeft();
				const Maths::Vector2D& topRight = cmd.textureRect.GetTopRight();
				float width = topRight.X() - bottomLeft.X();
				float height = topRight.Y() - bottomLeft.Y();

				if (width > 0.0f && height > 0.0f)
				{
					sprite.setTextureRect(sf::IntRect(
						sf::Vector2i(static_cast<int>(bottomLeft.X()), static_cast<int>(bottomLeft.Y())),
						sf::Vector2i(static_cast<int>(width), static_cast<int>(height))
					));
				}

				// Draw sprite
				// Note: Layer and subOrder sorting should happen before this visitor is called
				mTarget->draw(sprite);
			}
		}
	}
}
