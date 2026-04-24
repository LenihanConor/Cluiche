namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		inline
		IntersectionClassify::IntersectionClassify()
			: mResult(kNoIntersection)
		{}

		//-----------------------------------------------------------------------------
		inline
		IntersectionClassify::IntersectionClassify(Classification classify)
			: mResult(classify)
		{}

		//-----------------------------------------------------------------------------
		inline
		IntersectionClassify& IntersectionClassify::operator = (const IntersectionClassify& rhs)
		{
			mResult = rhs.mResult;

			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		bool IntersectionClassify::operator == (const IntersectionClassify& rhs) const
		{
			return (mResult == rhs.mResult);
		}

		//-----------------------------------------------------------------------------
		inline
		bool IntersectionClassify::operator != (const IntersectionClassify& rhs) const
		{
			return !(*this == rhs);
		}

		//-----------------------------------------------------------------------------
		inline
		bool IntersectionClassify::IsIntersecting()const
		{
			return (mResult != kNoIntersection);
		}

		//-----------------------------------------------------------------------------
		inline
		bool IntersectionClassify::IsNotIntersecting()const
		{
			return (mResult == kNoIntersection);
		}

		//-----------------------------------------------------------------------------
		inline
		bool IntersectionClassify::IsContainment()const
		{
			return (mResult == kAContainsB || mResult == kBContainsA);
		}

		//-----------------------------------------------------------------------------
		inline
		bool IntersectionClassify::IsNotContainment()const
		{
			return !IsContainment();
		}

		//-----------------------------------------------------------------------------
		inline
		IntersectionClassify::Classification IntersectionClassify::GetClassification()const
		{
			return mResult;
		}

		// SetClassification is implemented in IntersectionClassify.cpp (non-inline)
		// to ensure it has a linkable symbol for exception handling contexts

		//-----------------------------------------------------------------------------
		inline
		IntersectionClassify& IntersectionClassify::ReInterpretAandBObject()
		{
			if (mResult == kAContainsB)
			{
				mResult = kBContainsA;
			}
			else if (mResult == kBContainsA)
			{
				mResult = kAContainsB;
			}
			return *this;
		}
	}
}
