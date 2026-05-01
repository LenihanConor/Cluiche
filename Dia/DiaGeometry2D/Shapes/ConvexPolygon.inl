#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		inline
		int ConvexPolygon::GetVertexCount() const
		{
			return mVertexCount;
		}

		//-----------------------------------------------------------------------------
		inline
		const Dia::Maths::Vector2D& ConvexPolygon::GetVertex(int index) const
		{
			DIA_ASSERT(index >= 0 && index < mVertexCount, "Index out of bounds");
			return mVertices[index];
		}
	}
}
