#ifndef DIA_GEOMETRY2D_CONVEXPOLYGON_H
#define DIA_GEOMETRY2D_CONVEXPOLYGON_H

#include "DiaMaths/Vector/Vector2D.h"

namespace Dia
{
	namespace Geometry2D
	{
		// ConvexPolygon - up to 16 vertices, winding order CCW.
		class ConvexPolygon
		{
		public:
			static const int kMaxVertices = 16;

			ConvexPolygon();
			explicit ConvexPolygon(const Dia::Maths::Vector2D* vertices, int count);

			int GetVertexCount() const;
			const Dia::Maths::Vector2D& GetVertex(int index) const;
			Dia::Maths::Vector2D CalculateCenter() const;

		private:
			Dia::Maths::Vector2D mVertices[kMaxVertices];
			int mVertexCount;
		};
	}
}

#include "DiaGeometry2D/Shapes/ConvexPolygon.inl"

#endif // DIA_GEOMETRY2D_CONVEXPOLYGON_H
