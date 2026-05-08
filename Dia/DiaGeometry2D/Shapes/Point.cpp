#include "DiaGeometry2D/Shapes/Point.h"

namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		Point::Point()
			: mPosition(0.0f, 0.0f)
		{}

		//-----------------------------------------------------------------------------
		Point::Point(const Dia::Maths::Vector2D& position)
			: mPosition(position)
		{}

		//-----------------------------------------------------------------------------
		Point::Point(float x, float y)
			: mPosition(x, y)
		{}

		//-----------------------------------------------------------------------------
		const Dia::Maths::Vector2D& Point::GetPosition() const
		{
			return mPosition;
		}

		//-----------------------------------------------------------------------------
		void Point::SetPosition(const Dia::Maths::Vector2D& position)
		{
			mPosition = position;
		}
	}
}
