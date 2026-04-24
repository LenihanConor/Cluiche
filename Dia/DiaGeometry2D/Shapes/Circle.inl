namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		inline
		Circle::Circle()
			: mCenter(0.0f, 0.0f)
			, mRadius(0.0f)
		{}

		//-----------------------------------------------------------------------------
		inline
		Circle::Circle(float radius, const Dia::Maths::Vector2D& center)
			: mCenter(center)
			, mRadius(radius)
		{}

		//-----------------------------------------------------------------------------
		inline
		Circle::Circle(const Circle& rhs)
			: mCenter(rhs.mCenter)
			, mRadius(rhs.mRadius)
		{}

		//-----------------------------------------------------------------------------
		inline
		bool Circle::operator == (const Circle& rhs)
		{
			return (mCenter == rhs.mCenter &&
					mRadius == rhs.mRadius);
		}

		//-----------------------------------------------------------------------------
		inline
		bool Circle::operator != (const Circle& rhs)
		{
			return !(*this == rhs);
		}

		//-----------------------------------------------------------------------------
		inline
		float Circle::GetRadius()const
		{
			return mRadius;
		}

		//-----------------------------------------------------------------------------
		inline
		float Circle::GetSquaredRadius()const
		{
			return mRadius * mRadius;
		}

		//-----------------------------------------------------------------------------
		inline
		const Dia::Maths::Vector2D& Circle::GetCenter()const
		{
			return mCenter;
		}
	}
}
