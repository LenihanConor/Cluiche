////////////////////////////////////////////////////////////////////////////////
// Filename: SpriteDrawCommand.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaMaths/Vector/Vector2D.h>
#include <DiaGeometry2D/Shapes/AARect.h>
#include <DiaGraphics/Misc/RGBA.h>

namespace Dia
{
	namespace Graphics
	{
		////////////////////////////////////////////////////////////
		/// \brief Command to draw a sprite
		///
		/// Contains all information needed to render a textured quad
		/// (sprite) at a specific position with transformations.
		///
		/// Sprites are automatically batched by texture ID for efficient
		/// rendering (multiple sprites with same texture = 1 draw call).
		////////////////////////////////////////////////////////////
		struct SpriteDrawCommand
		{
			////////////////////////////////////////////////////////////
			/// \brief Default constructor
			////////////////////////////////////////////////////////////
			SpriteDrawCommand();

			////////////////////////////////////////////////////////////
			/// \brief Construct with position and texture
			///
			/// \param textureId  ID of the texture to use
			/// \param pos        Position in world space
			////////////////////////////////////////////////////////////
			SpriteDrawCommand(unsigned int textureId, const Maths::Vector2D& pos);

			////////////////////////////////////////////////////////////
			// Member data
			////////////////////////////////////////////////////////////
			unsigned int textureId;      ///< Texture handle ID (from asset manager)
			Maths::Vector2D position;    ///< Position in world space
			Maths::Vector2D scale;       ///< Scale factors (default 1,1)
			float rotation;              ///< Rotation in degrees (default 0)
			RGBA tint;                   ///< Color tint/modulation (default white)
			Geometry2D::AARect textureRect; ///< Sub-rectangle for sprite atlases (default full texture)
			Maths::Vector2D origin;      ///< Pivot point for rotation/scale (default 0,0 = top-left)
			int layer;                   ///< Z-order layer (0=back, higher=front, default 0)
			int subOrder;                ///< Fine-grained ordering within layer (default 0)
		};
	}
}
