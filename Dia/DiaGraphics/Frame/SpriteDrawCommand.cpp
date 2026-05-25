////////////////////////////////////////////////////////////////////////////////
// Filename: SpriteDrawCommand.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGraphics/Frame/SpriteDrawCommand.h"

namespace Dia
{
	namespace Graphics
	{
		SpriteDrawCommand::SpriteDrawCommand()
			: texture(nullptr)
			, position(0.0f, 0.0f)
			, scale(1.0f, 1.0f)
			, rotation(0.0f)
			, tint(RGBA::White)
			, textureRect(Maths::Vector2D(0.0f, 0.0f), Maths::Vector2D(0.0f, 0.0f))
			, origin(0.0f, 0.0f)
			, layer(0)
			, subOrder(0)
		{
		}

		SpriteDrawCommand::SpriteDrawCommand(ITexture* tex, const Maths::Vector2D& pos)
			: texture(tex)
			, position(pos)
			, scale(1.0f, 1.0f)
			, rotation(0.0f)
			, tint(RGBA::White)
			, textureRect(Maths::Vector2D(0.0f, 0.0f), Maths::Vector2D(0.0f, 0.0f))
			, origin(0.0f, 0.0f)
			, layer(0)
			, subOrder(0)
		{
		}
	}
}
