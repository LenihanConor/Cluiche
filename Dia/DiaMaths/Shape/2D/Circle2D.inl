namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		inline
		Circle2D::Circle2D()
			: mCenter(0.0f, 0.0f)
			, mRadius(0.0f)
		{}

		//-----------------------------------------------------------------------------
		inline
		Circle2D::Circle2D(float radius, const Vector2D& center)
			: mCenter(center)
			, mRadius(radius) 
		{}

		//-----------------------------------------------------------------------------
		inline
		Circle2D::Circle2D(const Circle2D& rhs)
			: mCenter (rhs.mCenter)
			, mRadius (rhs.mRadius)
		{}

		//-----------------------------------------------------------------------------
		inline
		bool Circle2D::operator == (const Circle2D& rhs)
		{
			return (mCenter == rhs.mCenter && 
					mRadius == rhs.mRadius);
		}

		//-----------------------------------------------------------------------------
		inline
		bool Circle2D::operator != (const Circle2D& rhs)
		{
			return !(*this == rhs);
		}

		//-----------------------------------------------------------------------------
		inline
		float Circle2D::GetRadius()const
		{
			return mRadius;
		}

		//-----------------------------------------------------------------------------
		inline
		float Circle2D::GetSquaredRadius()const
		{
			return Dia::Maths::Square(mRadius);
		}

		//-----------------------------------------------------------------------------
		inline
		const Vector2D&	Circle2D::GetCenter()const
		{
			return mCenter;
		}
	}
}