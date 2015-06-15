namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		inline
		AARect2D::AARect2D()
			: mBottonLeft(0.0f, 0.0f)
			, mTopRight(0.0f, 0.0f)
		{}

		//-----------------------------------------------------------------------------
		inline
		AARect2D::AARect2D(const Vector2D& bottonLeft, const Vector2D& topRight)
			: mBottonLeft(bottonLeft)
			, mTopRight(topRight)
		{}
		
		//-----------------------------------------------------------------------------
		inline
		AARect2D::AARect2D(const AARect2D& rhs)
			: mBottonLeft (rhs.mBottonLeft)
			, mTopRight (rhs.mTopRight)
		{}

		//-----------------------------------------------------------------------------
		inline
		AARect2D& AARect2D::operator =(const AARect2D& rhs)
		{
			mBottonLeft = rhs.mBottonLeft;
			mTopRight = rhs.mTopRight;

			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		float AARect2D::CalculateRadius()const
		{
			return (((mTopRight - mBottonLeft).Magnitude()) / 2.0f);
		}

		//-----------------------------------------------------------------------------
		inline
		float AARect2D::CalculateSquaredRadius()const
		{
			return (((mTopRight - mBottonLeft).SquareMagnitude()) / 2.0f);
		}

		//-----------------------------------------------------------------------------
		inline
		const Vector2D& AARect2D::GetBottomLeft()const
		{
			return mBottonLeft;
		}

		//-----------------------------------------------------------------------------
		inline
		const Vector2D& AARect2D::GetTopRight()const
		{
			return mTopRight;
		}
	}
}