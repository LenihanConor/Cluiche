////////////////////////////////////////////////////////////////////////////////
// Filename: Vertex.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaGraphics/Misc/Vertex.h"

namespace Dia
{
	namespace Graphics
	{
		////////////////////////////////////////////////////////////
		Vertex::Vertex()
			: position(0.0f, 0.0f)
			, color(255, 255, 255)
			, texCoords(0.0f, 0.0f)
		{
		}

		////////////////////////////////////////////////////////////
		Vertex::Vertex(const Maths::Vector2D& thePosition)
			: position(thePosition)
			, color(255, 255, 255)
			, texCoords(0.0f, 0.0f)
		{
		}

		////////////////////////////////////////////////////////////
		Vertex::Vertex(const Maths::Vector2D& thePosition, const RGBA& theColor)
			: position(thePosition)
			, color(theColor)
			, texCoords(0.0f, 0.0f)
		{
		}

		////////////////////////////////////////////////////////////
		Vertex::Vertex(const Maths::Vector2D& thePosition, const Maths::Vector2D& theTexCoords)
			: position(thePosition)
			, color(255, 255, 255)
			, texCoords(theTexCoords)
		{
		}

		////////////////////////////////////////////////////////////
		Vertex::Vertex(const Maths::Vector2D& thePosition, const RGBA& theColor, const Maths::Vector2D& theTexCoords)
			: position(thePosition)
			, color(theColor)
			, texCoords(theTexCoords)
		{
		}
	}
}
