#ifndef DIA_GEOMETRY2D_POINT_H
#define DIA_GEOMETRY2D_POINT_H

#include "DiaMaths/Vector/Vector2D.h"

namespace Dia
{
	namespace Geometry2D
	{
		// Point - a 2D position. Thin wrapper used for type-safe shape dispatch.
		class Point
		{
		public:
			Point();
			explicit Point(const Dia::Maths::Vector2D& position);
			Point(float x, float y);

			const Dia::Maths::Vector2D& GetPosition() const;
			void SetPosition(const Dia::Maths::Vector2D& position);

		private:
			Dia::Maths::Vector2D mPosition;
		};
	}
}

#endif // DIA_GEOMETRY2D_POINT_H
