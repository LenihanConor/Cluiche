namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		inline
		Capsule2D::Capsule2D()
			: mPt1(0.0f, 0.0f)
			, mPt2(0.0f, 0.0f)
			, mRadius(0.0f)
		{}
		
		//-----------------------------------------------------------------------------
		inline
		Capsule2D::Capsule2D(float radius, const Vector2D& axisPt1, const Vector2D& axisPt2)
			: mPt1(axisPt1)
			, mPt2(axisPt2)
			, mRadius(radius)
		{}
		//-----------------------------------------------------------------------------
		inline
		Capsule2D::Capsule2D(const Capsule2D& rhs)
			: mPt1 (rhs.mPt1)
			, mPt2 (rhs.mPt2)
			, mRadius (rhs.mRadius)
		{}

		//-----------------------------------------------------------------------------
		inline
		Capsule2D& Capsule2D::operator =(const Capsule2D& rhs)
		{
			mPt1 = rhs.mPt1;
			mPt2 = rhs.mPt2;
			mRadius = rhs.mRadius;

			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		float Capsule2D::GetRadius()const
		{	
			return mRadius;
		}
		
		//-----------------------------------------------------------------------------
		inline
		float Capsule2D::GetSquaredRadius()const
		{
			return mRadius * mRadius;
		}
			
		//-----------------------------------------------------------------------------
		inline
		float Capsule2D::GetLength()const
		{
			return ((mPt1 - mPt2).Magnitude() + mRadius + mRadius);
		}

		//-----------------------------------------------------------------------------
		inline
		float Capsule2D::GetSquaredLength()const
		{
			return ((mPt1 - mPt2).SquareMagnitude() + mRadius + mRadius);
		}

		//-----------------------------------------------------------------------------
		inline
		float Capsule2D::GetAxisLength()const
		{
			return (mPt1 - mPt2).Magnitude();
		}

		//-----------------------------------------------------------------------------
		inline
		float Capsule2D::GetAxisSquaredLength()const
		{
			return (mPt1 - mPt2).SquareMagnitude();
		}

		//-----------------------------------------------------------------------------
		inline
		const Vector2D& Capsule2D::GetPoint1()const
		{
			return mPt1;
		}

		//-----------------------------------------------------------------------------
		inline
		const Vector2D& Capsule2D::GetPoint2()const
		{
			return mPt2;
		}

		//-----------------------------------------------------------------------------
		inline
		Vector2D Capsule2D::GetCenter()const
		{
			return (mPt1 + mPt2) / 2.0f;
		}
	}
}