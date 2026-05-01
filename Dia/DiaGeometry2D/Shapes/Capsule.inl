namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		inline
		Capsule::Capsule()
			: mPt1(0.0f, 0.0f)
			, mPt2(0.0f, 0.0f)
			, mRadius(0.0f)
		{}

		//-----------------------------------------------------------------------------
		inline
		Capsule::Capsule(float radius, const Dia::Maths::Vector2D& axisPt1, const Dia::Maths::Vector2D& axisPt2)
			: mPt1(axisPt1)
			, mPt2(axisPt2)
			, mRadius(radius)
		{}

		//-----------------------------------------------------------------------------
		inline
		Capsule::Capsule(const Capsule& rhs)
			: mPt1(rhs.mPt1)
			, mPt2(rhs.mPt2)
			, mRadius(rhs.mRadius)
		{}

		//-----------------------------------------------------------------------------
		inline
		Capsule& Capsule::operator =(const Capsule& rhs)
		{
			mPt1 = rhs.mPt1;
			mPt2 = rhs.mPt2;
			mRadius = rhs.mRadius;
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		float Capsule::GetRadius() const
		{
			return mRadius;
		}

		//-----------------------------------------------------------------------------
		inline
		float Capsule::GetSquaredRadius() const
		{
			return mRadius * mRadius;
		}

		//-----------------------------------------------------------------------------
		inline
		float Capsule::GetLength() const
		{
			return ((mPt1 - mPt2).Magnitude() + mRadius + mRadius);
		}

		//-----------------------------------------------------------------------------
		inline
		float Capsule::GetSquaredLength() const
		{
			return ((mPt1 - mPt2).SquareMagnitude() + mRadius + mRadius);
		}

		//-----------------------------------------------------------------------------
		inline
		float Capsule::GetAxisLength() const
		{
			return (mPt1 - mPt2).Magnitude();
		}

		//-----------------------------------------------------------------------------
		inline
		float Capsule::GetAxisSquaredLength() const
		{
			return (mPt1 - mPt2).SquareMagnitude();
		}

		//-----------------------------------------------------------------------------
		inline
		const Dia::Maths::Vector2D& Capsule::GetPoint1() const
		{
			return mPt1;
		}

		//-----------------------------------------------------------------------------
		inline
		const Dia::Maths::Vector2D& Capsule::GetPoint2() const
		{
			return mPt2;
		}

		//-----------------------------------------------------------------------------
		inline
		Dia::Maths::Vector2D Capsule::GetCenter() const
		{
			return (mPt1 + mPt2) / 2.0f;
		}
	}
}
