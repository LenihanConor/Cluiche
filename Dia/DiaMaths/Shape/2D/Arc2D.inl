namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		inline
		Arc2D::Arc2D()
			: mFocal(0.0f, 0.0f)
			, mAxis(1.0f, 0.0f)
			, mRadius(1.0f)
			, mAngle(0.0f)
		{}
		
		//-----------------------------------------------------------------------------
		inline
		Arc2D::Arc2D(const Arc2D& rhs)
			: mFocal (rhs.mFocal)
			, mAxis (rhs.mAxis)
			, mRadius (rhs.mRadius)
			, mAngle (rhs.mAngle)
		{}

		//-----------------------------------------------------------------------------
		inline
		Arc2D& Arc2D::operator =(const Arc2D& rhs)
		{
			mFocal = rhs.mFocal;
			mAxis = rhs.mAxis;
			mRadius = rhs.mRadius;
			mAngle = rhs.mAngle;

			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		float Arc2D::GetRadius() const
		{	
			return mRadius;
		}
			
		//-----------------------------------------------------------------------------
		inline
		float Arc2D::GetSquaredRadius() const
		{
			return Square(mRadius);	
		}	

		//-----------------------------------------------------------------------------
		inline
		const Angle& Arc2D::GetAngle() const
		{
			return mAngle;		
		}
		
		//-----------------------------------------------------------------------------
		inline
		const Vector2D& Arc2D::GetFocal() const
		{
			return mFocal;
		}
		
		//-----------------------------------------------------------------------------
		inline
		const Vector2D& Arc2D::GetAxis() const
		{
			return mAxis;
		}
	}
}