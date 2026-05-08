////////////////////////////////////////////////////////////////////////////////
// Filename: EntityFrameRenderer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaSFML/EntityFrameRenderer.h"
#include "DiaSFML/TextureHandler.h"
#include "DiaGraphics/Frame/EntityFrameData.h"
#include "DiaGraphics/Frame/SpriteDrawCommand.h"
#include "DiaSFML/Conversion.h"
#include <DiaCore/Core/Assert.h>

#include <SFML/Graphics/RenderTarget.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Transform.hpp>

namespace Dia
{
	namespace SFML
	{
		EntityFrameRenderer::EntityFrameRenderer(sf::RenderTarget* target, TextureHandler* textureHandler)
			: mTarget(target)
			, mTextureHandler(textureHandler)
		{
		}

		void EntityFrameRenderer::Visit(const Graphics::EntityFrameData& data) const
		{
			const Core::Containers::DynamicArrayC<Graphics::SpriteDrawCommand, 256>& sprites = data.GetSprites();

			for (unsigned int i = 0; i < sprites.Size(); ++i)
			{
				const Graphics::SpriteDrawCommand& cmd = sprites[i];

				const sf::Texture* texture = mTextureHandler->GetTexture(cmd.textureId);
				DIA_ASSERT(texture, "Draw command references texture ID %u which is not loaded — asset must be loaded through AssetRuntime before use", cmd.textureId);
				if (!texture)
					continue;

				sf::Sprite sprite(*texture);
				sprite.setPosition(sf::Vector2f(cmd.position.X(), cmd.position.Y()));
				sprite.setScale(sf::Vector2f(cmd.scale.X(), cmd.scale.Y()));
				sprite.setRotation(sf::degrees(cmd.rotation));
				sprite.setOrigin(sf::Vector2f(cmd.origin.X(), cmd.origin.Y()));
				sprite.setColor(sf::Color(cmd.tint.R(), cmd.tint.G(), cmd.tint.B(), cmd.tint.A()));

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

				mTarget->draw(sprite);
			}
		}
	}
}
