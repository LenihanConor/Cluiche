////////////////////////////////////////////////////////////////////////////////
// Filename: Vertex.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "DiaGraphics/Misc/RGBA.h"
#include <DiaMaths/Vector/Vector2D.h>

namespace Dia
{
	namespace Graphics
	{
		////////////////////////////////////////////////////////////
		/// \brief Define a point with color and texture coordinates
		///
		/// A vertex is an improved point. It has a position and other
		/// extra attributes that will be used for drawing: in SFML,
		/// vertices have a color and a pair of texture coordinates.
		///
		/// The vertex is the building block of drawing. Everything which
		/// is visible on screen is made of vertices. They are grouped
		/// as 2D primitives (triangles, quads, ...), and these primitives
		/// are grouped to create even more complex 2D entities such as
		/// sprites, texts, etc.
		///
		/// \note This structure layout MUST match sf::Vertex exactly for
		/// zero-copy rendering to work. Position, color, texCoords order
		/// is critical.
		////////////////////////////////////////////////////////////
		class Vertex
		{
		public:
			////////////////////////////////////////////////////////////
			/// \brief Default constructor
			////////////////////////////////////////////////////////////
			Vertex();

			////////////////////////////////////////////////////////////
			/// \brief Construct the vertex from its position
			///
			/// The vertex color is white and texture coordinates are (0, 0).
			///
			/// \param thePosition Vertex position
			////////////////////////////////////////////////////////////
			explicit Vertex(const Maths::Vector2D& thePosition);

			////////////////////////////////////////////////////////////
			/// \brief Construct the vertex from its position and color
			///
			/// The texture coordinates are (0, 0).
			///
			/// \param thePosition Vertex position
			/// \param theColor    Vertex color
			////////////////////////////////////////////////////////////
			Vertex(const Maths::Vector2D& thePosition, const RGBA& theColor);

			////////////////////////////////////////////////////////////
			/// \brief Construct the vertex from its position and texture coordinates
			///
			/// The vertex color is white.
			///
			/// \param thePosition  Vertex position
			/// \param theTexCoords Vertex texture coordinates
			////////////////////////////////////////////////////////////
			Vertex(const Maths::Vector2D& thePosition, const Maths::Vector2D& theTexCoords);

			////////////////////////////////////////////////////////////
			/// \brief Construct the vertex from its position, color and texture coordinates
			///
			/// \param thePosition  Vertex position
			/// \param theColor     Vertex color
			/// \param theTexCoords Vertex texture coordinates
			////////////////////////////////////////////////////////////
			Vertex(const Maths::Vector2D& thePosition, const RGBA& theColor, const Maths::Vector2D& theTexCoords);

			////////////////////////////////////////////////////////////
			// Member data
			////////////////////////////////////////////////////////////
			Maths::Vector2D position;  ///< 2D position of the vertex
			RGBA color;                ///< Color of the vertex
			Maths::Vector2D texCoords; ///< Coordinates of the texture's pixel to map to the vertex
		};
	}
}
