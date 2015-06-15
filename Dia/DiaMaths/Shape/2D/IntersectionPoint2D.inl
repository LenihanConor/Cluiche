namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		inline
		IntersectionPoint2D::IntersectionPoint2D()
			: mPoint()
			, mNormal()
			, mClassificaton()
		{}
		
		//-----------------------------------------------------------------------------
		inline
		bool IntersectionPoint2D::operator == (const IntersectionPoint2D& rhs)
		{
			return (mPoint == rhs.mPoint &&
					mNormal == rhs.mNormal &&
					mClassificaton == rhs.mClassificaton)
		}

		//-----------------------------------------------------------------------------
		inline
		bool IntersectionPoint2D::operator != (const IntersectionPoint2D& rhs)
		{
			return !(*this == rhs);
		}

		//-----------------------------------------------------------------------------
		inline
		const Vector2D&	IntersectionPoint2D::GetPoint()const
		{
			return mPoint;
		}

		//-----------------------------------------------------------------------------
		inline
		const Vector2D& IntersectionPoint2D::GetNormal()const
		{
			return mNormal;
		}

		//-----------------------------------------------------------------------------
		inline
		IntersectionClassify IntersectionPoint2D::GetResult()const
		{
			return mClassificaton;
		}
	}
}