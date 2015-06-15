namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		inline
		Line2D::Line2D()
		{
			mPt[0].Set(0.0f, 0.0f);
			mPt[1].Set(1.0f, 0.0f);
		}

		//-----------------------------------------------------------------------------
		inline
		Line2D::Line2D(const Line2D& rhs)
		{
			mPt[0] = rhs.mPt[0];
			mPt[1] = rhs.mPt[1];
		}

		//-----------------------------------------------------------------------------
		inline
		Line2D::Line2D(const Vector2D& pt1, const Vector2D& pt2)
		{
			mPt[0] = pt1;
			mPt[1] = pt2;
		}

		//-----------------------------------------------------------------------------
		inline
		Line2D& Line2D::operator =(const Line2D& rhs)
		{
			mPt[0] = rhs.mPt[0];
			mPt[1] = rhs.mPt[1];

			return *this;
		}
		
		//-----------------------------------------------------------------------------
		inline
		Vector2D& Line2D::operator[](int index)
		{
			DIA_ASSERT(index >=0 && index <= kNumPts, "Index out of bounds");

			return mPt[index];
		}

		//-----------------------------------------------------------------------------
		inline
		const Vector2D& Line2D::operator[](int index) const
		{
			DIA_ASSERT(index >=0 && index <= kNumPts, "Index out of bounds");

			return mPt[index];
		}

		//-----------------------------------------------------------------------------
		inline
		Vector2D& Line2D::operator[](unsigned int index)
		{
			DIA_ASSERT(index >=0 && index <= kNumPts, "Index out of bounds");

			return mPt[index];
		}

		//-----------------------------------------------------------------------------
		inline
		const Vector2D& Line2D::operator[](unsigned int index) const
		{
			DIA_ASSERT(index >=0 && index <= kNumPts, "Index out of bounds");

			return mPt[index];
		}

		//-----------------------------------------------------------------------------
		inline
		const Vector2D&	Line2D::GetPt1()const
		{
			return mPt[0];
		}
		
		//-----------------------------------------------------------------------------
		inline
		const Vector2D&	Line2D::GetPt2()const
		{
			return mPt[1];
		}
	}
}