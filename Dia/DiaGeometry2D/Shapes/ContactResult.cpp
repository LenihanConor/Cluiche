#include "DiaGeometry2D/Shapes/ContactResult.h"

namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		ContactResult::ContactResult()
			: point(0.0f, 0.0f)
			, normal(0.0f, 0.0f)
			, depth(0.0f)
		{}

		//-----------------------------------------------------------------------------
		ContactResult::ContactResult(const Dia::Maths::Vector2D& point, const Dia::Maths::Vector2D& normal, float depth)
			: point(point)
			, normal(normal)
			, depth(depth)
		{}
	}
}
