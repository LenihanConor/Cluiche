#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		inline
		OORect::OORect()
		{}

		//-----------------------------------------------------------------------------
		inline
		OORect::OORect(const Dia::Maths::Vector2D& pt1, const Dia::Maths::Vector2D& pt2, const Dia::Maths::Vector2D& pt3, const Dia::Maths::Vector2D& pt4)
		{
			mPts[0] = pt1;
			mPts[1] = pt2;
			mPts[2] = pt3;
			mPts[3] = pt4;
		}

		//-----------------------------------------------------------------------------
		inline
		OORect::OORect(const OORect& rhs)
		{
			for (unsigned int i = 0; i < kNumPts; i++)
			{
				mPts[i] = rhs.mPts[i];
			}
		}

		//-----------------------------------------------------------------------------
		inline
		OORect& OORect::operator =(const OORect& rhs)
		{
			for (unsigned int i = 0; i < kNumPts; i++)
			{
				mPts[i] = rhs.mPts[i];
			}

			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		const Dia::Maths::Vector2D& OORect::GetPt(PtId index)const
		{
			DIA_ASSERT(index >= 0 && index < kNumPts, "Inavlid index");

			return mPts[index];
		}

		//-----------------------------------------------------------------------------
		inline
		const Dia::Maths::Vector2D& OORect::GetPt(int index)const
		{
			DIA_ASSERT(index >= 0 && index < kNumPts, "Inavlid index");

			return mPts[index];
		}

		//-----------------------------------------------------------------------------
		inline
		float OORect::CalculateRadius()const
		{
			return ((mPts[0] - mPts[2]).Magnitude() / 2.0f);
		}

		//-----------------------------------------------------------------------------
		inline
		float OORect::CalculateSquareRadius()const
		{
			return (mPts[0] - mPts[2]).SquareMagnitude();
		}

		//-----------------------------------------------------------------------------
		inline
		Dia::Maths::Vector2D OORect::CalculateCenter()const
		{
			return ((mPts[0] + mPts[2]) / 2.0f);
		}
	}
}
