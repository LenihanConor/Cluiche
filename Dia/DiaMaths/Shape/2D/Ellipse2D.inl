namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		inline
		Ellipse2D::Ellipse2D()
			: mCenter(0.0f, 0.0f)
			, mXRadius(1.0f)
			, mYRadius(1.0f)
		{}

		//-----------------------------------------------------------------------------
		inline
		Ellipse2D::Ellipse2D(const Ellipse2D& rhs)
			: mCenter (rhs.mCenter)
			, mXRadius (rhs.mXRadius)
			, mYRadius (rhs.mYRadius)
		{}

		//-----------------------------------------------------------------------------
		inline
		Ellipse2D& Ellipse2D::operator =(const Ellipse2D& rhs)
		{
			mCenter = rhs.mCenter;
			mXRadius = rhs.mXRadius;
			mYRadius = rhs.mYRadius;

			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		float Ellipse2D::GetXRadius()const
		{
			return mXRadius;
		}

		//-----------------------------------------------------------------------------
		inline
		float Ellipse2D::GetYRadius()const
		{
			return mYRadius;
		}

		//-----------------------------------------------------------------------------
		inline
		const Vector2D&	Ellipse2D::GetCenter()const
		{
			return mCenter;
		}
	}
}