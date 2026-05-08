namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		inline
		AARect::AARect()
			: mBottomLeft(0.0f, 0.0f)
			, mTopRight(0.0f, 0.0f)
		{}

		//-----------------------------------------------------------------------------
		inline
		AARect::AARect(const Dia::Maths::Vector2D& bottomLeft, const Dia::Maths::Vector2D& topRight)
			: mBottomLeft(bottomLeft)
			, mTopRight(topRight)
		{}

		//-----------------------------------------------------------------------------
		inline
		AARect::AARect(const AARect& rhs)
			: mBottomLeft(rhs.mBottomLeft)
			, mTopRight(rhs.mTopRight)
		{}

		//-----------------------------------------------------------------------------
		inline
		AARect& AARect::operator =(const AARect& rhs)
		{
			mBottomLeft = rhs.mBottomLeft;
			mTopRight = rhs.mTopRight;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		float AARect::CalculateRadius() const
		{
			return (((mTopRight - mBottomLeft).Magnitude()) / 2.0f);
		}

		//-----------------------------------------------------------------------------
		inline
		float AARect::CalculateSquaredRadius() const
		{
			return (((mTopRight - mBottomLeft).SquareMagnitude()) / 2.0f);
		}

		//-----------------------------------------------------------------------------
		inline
		const Dia::Maths::Vector2D& AARect::GetBottomLeft() const
		{
			return mBottomLeft;
		}

		//-----------------------------------------------------------------------------
		inline
		const Dia::Maths::Vector2D& AARect::GetTopRight() const
		{
			return mTopRight;
		}
	}
}
