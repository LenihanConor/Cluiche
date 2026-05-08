namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		inline
		Arc::Arc()
			: mFocal(0.0f, 0.0f)
			, mAxis(1.0f, 0.0f)
			, mRadius(1.0f)
			, mAngle(0.0f)
		{}

		//-----------------------------------------------------------------------------
		inline
		Arc::Arc(const Arc& rhs)
			: mFocal(rhs.mFocal)
			, mAxis(rhs.mAxis)
			, mRadius(rhs.mRadius)
			, mAngle(rhs.mAngle)
		{}

		//-----------------------------------------------------------------------------
		inline
		Arc& Arc::operator =(const Arc& rhs)
		{
			mFocal  = rhs.mFocal;
			mAxis   = rhs.mAxis;
			mRadius = rhs.mRadius;
			mAngle  = rhs.mAngle;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		float Arc::GetRadius() const
		{
			return mRadius;
		}

		//-----------------------------------------------------------------------------
		inline
		float Arc::GetSquaredRadius() const
		{
			return mRadius * mRadius;
		}

		//-----------------------------------------------------------------------------
		inline
		const Dia::Maths::Angle& Arc::GetAngle() const
		{
			return mAngle;
		}

		//-----------------------------------------------------------------------------
		inline
		const Dia::Maths::Vector2D& Arc::GetFocal() const
		{
			return mFocal;
		}

		//-----------------------------------------------------------------------------
		inline
		const Dia::Maths::Vector2D& Arc::GetAxis() const
		{
			return mAxis;
		}
	}
}
