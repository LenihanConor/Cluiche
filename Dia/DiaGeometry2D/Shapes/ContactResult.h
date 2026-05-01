#ifndef DIA_GEOMETRY2D_CONTACTRESULT_H
#define DIA_GEOMETRY2D_CONTACTRESULT_H

#include "DiaMaths/Vector/Vector2D.h"

namespace Dia
{
	namespace Geometry2D
	{
		struct ContactResult
		{
			Dia::Maths::Vector2D point;
			Dia::Maths::Vector2D normal;
			float depth;

			ContactResult();
			ContactResult(const Dia::Maths::Vector2D& point, const Dia::Maths::Vector2D& normal, float depth);
		};
	}
}

#endif // DIA_GEOMETRY2D_CONTACTRESULT_H
