////////////////////////////////////////////////////////////////////////////////
// Filename: SpriteDrawCommand.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGraphics/Frame/SpriteDrawCommand.h"

namespace Dia
{
	namespace Graphics
	{
		////////////////////////////////////////////////////////////
		SpriteDrawCommand::SpriteDrawCommand()
			: textureId(0)
			, position(0.0f, 0.0f)
			, scale(1.0f, 1.0f)
			, rotation(0.0f)
			, tint(RGBA::White)
			, textureRect(Maths::Vector2D(0.0f, 0.0f), Maths::Vector2D(0.0f, 0.0f)) // 0 size means use full texture
			, origin(0.0f, 0.0f)
			, layer(0)
			, subOrder(0)
		{
		}

		////////////////////////////////////////////////////////////
		SpriteDrawCommand::SpriteDrawCommand(unsigned int textureId, const Maths::Vector2D& pos)
			: textureId(textureId)
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
