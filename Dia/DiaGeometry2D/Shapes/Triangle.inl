#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		inline
		Triangle::Triangle()
		{}

		//-----------------------------------------------------------------------------
		inline
		Triangle::Triangle(const Dia::Maths::Vector2D& pt1, const Dia::Maths::Vector2D& pt2, const Dia::Maths::Vector2D& pt3)
		{
			mPts[0] = pt1;
			mPts[1] = pt2;
			mPts[2] = pt3;
		}

		//-----------------------------------------------------------------------------
		inline
		Triangle::Triangle(const Triangle& rhs)
		{
			for (unsigned int i = 0; i < kNumPts; i++)
			{
				mPts[i] = rhs.mPts[i];
			}
		}

		//-----------------------------------------------------------------------------
		inline
		Triangle& Triangle::operator =(const Triangle& rhs)
		{
			for (unsigned int i = 0; i < kNumPts; i++)
			{
				mPts[i] = rhs.mPts[i];
			}

			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		const Dia::Maths::Vector2D& Triangle::GetPt(PtId index)const
		{
			DIA_ASSERT(index >= 0 && index < kNumPts, "Inavlid index");

			return mPts[index];
		}

		//-----------------------------------------------------------------------------
		inline
		const Dia::Maths::Vector2D& Triangle::GetPt(int index)const
		{
			DIA_ASSERT(index >= 0 && index < kNumPts, "Inavlid index");

			return mPts[index];
		}
	}
}
