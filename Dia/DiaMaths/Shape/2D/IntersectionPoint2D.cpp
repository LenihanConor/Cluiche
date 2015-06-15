#include "DiaMaths/Shape/2D/IntersectionPoint2D.h"

namespace Dia
{
	namespace Maths
	{	
		IntersectionPoint2D::IntersectionPoint2D(const Vector2D& point, const Vector2D& normal, const IntersectionClassify& classificaton)
			: mPoint (point)
			, mNormal (mNormal)
			, mClassificaton (classificaton)
		{}
		
		IntersectionPoint2D& IntersectionPoint2D::operator =(const IntersectionPoint2D& rhs)
		{
			mPoint = rhs.mPoint;
			mNormal = rhs.mNormal;
			mClassificaton = rhs.mClassificaton;

			return *this;
		}

		void IntersectionPoint2D::Create(const IntersectionClassify& classificaton, const Vector2D& point, const Vector2D& normal)
		{
			mPoint = point;
			mNormal = normal;
			mClassificaton = classificaton;
		}
	}
}