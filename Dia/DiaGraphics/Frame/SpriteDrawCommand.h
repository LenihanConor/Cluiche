////////////////////////////////////////////////////////////////////////////////
// Filename: SpriteDrawCommand.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGraphics/Assets/ITexture.h>
#include <DiaMaths/Vector/Vector2D.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGraphics/Misc/RGBA.h>

namespace Dia
{
	namespace Graphics
	{
		struct SpriteDrawCommand
		{
			SpriteDrawCommand();
			SpriteDrawCommand(ITexture* texture, const Maths::Vector2D& pos);

			ITexture*               texture;      ///< Renderer-owned; nullptr or non-ready sprites are skipped
			Maths::Vector2D         position;
			Maths::Vector2D         scale;
			float                   rotation;
			RGBA                    tint;
			Geometry2D::AARect      textureRect;
			Maths::Vector2D         origin;
			int                     layer;
			int                     subOrder;
		};
	}
}
