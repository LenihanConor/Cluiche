#include <DiaCore/Core/Assert.h>

namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		inline
		OORect2D::OORect2D()
		{}
		
		//-----------------------------------------------------------------------------
		inline
		OORect2D::OORect2D(const Vector2D& pt1, const Vector2D& pt2, const Vector2D& pt3, const Vector2D& pt4)
		{
			mPts[0] = pt1;
			mPts[1] = pt2;
			mPts[2] = pt3;
			mPts[3] = pt4;
		}

		//-----------------------------------------------------------------------------
		inline
		OORect2D::OORect2D(const OORect2D& rhs)
		{
			for (unsigned int i = 0; i < kNumPts; i++)
			{
				mPts[i] = rhs.mPts[i];
			}
		}

		//-----------------------------------------------------------------------------
		inline
		OORect2D& OORect2D::operator =(const OORect2D& rhs)
		{
			for (unsigned int i = 0; i < kNumPts; i++)
			{
				mPts[i] = rhs.mPts[i];
			}

			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		const Vector2D&	OORect2D::GetPt(PtId index)const
		{
			DIA_ASSERT(index >= 0 && index < kNumPts, "Inavlid index");

			return mPts[index];
		}
		
		//-----------------------------------------------------------------------------
		inline
		const Vector2D&	OORect2D::GetPt(int index)const
		{
			DIA_ASSERT(index >= 0 && index < kNumPts, "Inavlid index");

			return mPts[index];
		}

		//-----------------------------------------------------------------------------
		inline
		float OORect2D::CalculateRadius()const
		{
			return ((mPts[0] - mPts[2]).Magnitude() / 2.0f);
		}

		//-----------------------------------------------------------------------------
		inline
		float OORect2D::CalculateSquareRadius()const
		{
			return (mPts[0] - mPts[2]).SquareMagnitude();
		}

		//-----------------------------------------------------------------------------
		inline
		Vector2D OORect2D::CalculateCenter()const
		{
			return ((mPts[0] + mPts[2]) / 2.0f);
		}
	}
}