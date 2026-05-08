#include "DiaGeometry2D/Shapes/ConvexPolygon.h"

namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		ConvexPolygon::ConvexPolygon()
			: mVertexCount(0)
		{}

		//-----------------------------------------------------------------------------
		ConvexPolygon::ConvexPolygon(const Dia::Maths::Vector2D* vertices, int count)
			: mVertexCount(0)
		{
			DIA_ASSERT(count >= 0 && count <= kMaxVertices, "Vertex count out of range");

			mVertexCount = count;
			for (int i = 0; i < count; i++)
			{
				mVertices[i] = vertices[i];
			}
		}

		//-----------------------------------------------------------------------------
		Dia::Maths::Vector2D ConvexPolygon::CalculateCenter() const
		{
			Dia::Maths::Vector2D sum(0.0f, 0.0f);

			if (mVertexCount == 0)
			{
				return sum;
			}

			for (int i = 0; i < mVertexCount; i++)
			{
				sum = sum + mVertices[i];
			}

			return sum / static_cast<float>(mVertexCount);
		}
	}
}
