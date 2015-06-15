namespace Dia
{
	namespace Maths
	{
		//-----------------------------------------------------------------------------
		inline
		IntersectionClassify::IntersectionClassify()
			: mResult(kNoIntersection)
		{}

		//-----------------------------------------------------------------------------
		inline
		IntersectionClassify::IntersectionClassify(Classification classify)
			: mResult (classify)
		{}

		//-----------------------------------------------------------------------------
		inline
		IntersectionClassify&	IntersectionClassify::operator = (const IntersectionClassify& rhs)
		{
			mResult = rhs.mResult;

			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		bool IntersectionClassify::operator == (const IntersectionClassify& rhs)
		{
			return (mResult == rhs.mResult);
		}

		//-----------------------------------------------------------------------------
		inline
		bool IntersectionClassify::operator != (const IntersectionClassify& rhs)
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
	}
}