#pragma once

#include "DiaMaths/Vector/Vector2D.h"

namespace Dia
{
	namespace Maths
	{
		class Ellipse2D
		{
		public:
			Ellipse2D();

			Ellipse2D(const Ellipse2D& rhs);

			Ellipse2D& operator =(const Ellipse2D& rhs);
				
			float				GetXRadius()const;
			float				GetYRadius()const;
			const Vector2D&		GetCenter()const;

		private:

			Vector2D mCenter;

			float mXRadius;
			float mYRadius;
		};
	}
}

#include "DiaMaths/Shape/2D/Ellipse2D.inl"