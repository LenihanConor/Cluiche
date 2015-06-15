namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		inline
		Triangle2D::Triangle2D()
		{}

		//-----------------------------------------------------------------------------
		inline
		Triangle2D::Triangle2D(const Vector2D& pt1, const Vector2D& pt2, const Vector2D& pt3)
		{
			mPts[0] = pt1;
			mPts[1] = pt2;
			mPts[2] = pt3;
		}

		//-----------------------------------------------------------------------------
		inline
		Triangle2D::Triangle2D(const Triangle2D& rhs)
		{
			for (unsigned int i = 0; i < kNumPts; i++)
			{
				mPts[i] = rhs.mPts[i];
			}
		}

		//-----------------------------------------------------------------------------
		inline
		Triangle2D& Triangle2D::operator =(const Triangle2D& rhs)
		{
			for (unsigned int i = 0; i < kNumPts; i++)
			{
				mPts[i] = rhs.mPts[i];
			}

			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		const Vector2D&	Triangle2D::GetPt(PtId index)const
		{
			DIA_ASSERT(index >= 0 && index < kNumPts, "Inavlid index");

			return mPts[index];
		}

		//-----------------------------------------------------------------------------
		inline
		const Vector2D&	Triangle2D::GetPt(int index)const
		{
			DIA_ASSERT(index >= 0 && index < kNumPts, "Inavlid index");

			return mPts[index];
		}
	}
}