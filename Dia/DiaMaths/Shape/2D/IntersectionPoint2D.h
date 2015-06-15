#pragma once

#include "DiaMaths/Shape/Common/IntersectionClassify.h"
#include "DiaMaths/Vector/Vector2D.h"

namespace Dia
{
	namespace Maths
	{
		//==============================================================================
		// CLASS IntersectionPoint2D
		//==============================================================================
		class IntersectionPoint2D
		{
		public:
			IntersectionPoint2D();
			IntersectionPoint2D(const Vector2D& point, const Vector2D& normal, const IntersectionClassify& intersectionResult);

			IntersectionPoint2D(const IntersectionPoint2D& rhs);

			IntersectionPoint2D&	operator =	(const IntersectionPoint2D& rhs);
			bool					operator == (const IntersectionPoint2D& rhs);
			bool					operator != (const IntersectionPoint2D& rhs);

			void Create(const IntersectionClassify& classificaton, const Vector2D& point, const Vector2D& normal);

			const Vector2D&				GetPoint()const;
			const Vector2D&				GetNormal()const;
			const IntersectionClassify&	GetResult()const;

		private:
			Vector2D mPoint;
			Vector2D mNormal;
			IntersectionClassify mClassificaton;
		};
	}
}