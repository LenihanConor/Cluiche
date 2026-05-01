#include "DiaCore/Core/Assert.h"

namespace Dia
{
	namespace Geometry2D
	{
		//-----------------------------------------------------------------------------
		inline
		Line::Line()
		{
			mPt[0].Set(0.0f, 0.0f);
			mPt[1].Set(1.0f, 0.0f);
		}

		//-----------------------------------------------------------------------------
		inline
		Line::Line(const Line& rhs)
		{
			mPt[0] = rhs.mPt[0];
			mPt[1] = rhs.mPt[1];
		}

		//-----------------------------------------------------------------------------
		inline
		Line::Line(const Dia::Maths::Vector2D& pt1, const Dia::Maths::Vector2D& pt2)
		{
			mPt[0] = pt1;
			mPt[1] = pt2;
		}

		//-----------------------------------------------------------------------------
		inline
		Line& Line::operator =(const Line& rhs)
		{
			mPt[0] = rhs.mPt[0];
			mPt[1] = rhs.mPt[1];
			return *this;
		}

		//-----------------------------------------------------------------------------
		inline
		Dia::Maths::Vector2D& Line::operator[](int index)
		{
			DIA_ASSERT(index >= 0 && index <= kNumPts, "Index out of bounds");
			return mPt[index];
		}

		//-----------------------------------------------------------------------------
		inline
		const Dia::Maths::Vector2D& Line::operator[](int index) const
		{
			DIA_ASSERT(index >= 0 && index <= kNumPts, "Index out of bounds");
			return mPt[index];
		}

		//-----------------------------------------------------------------------------
		inline
		Dia::Maths::Vector2D& Line::operator[](unsigned int index)
		{
			DIA_ASSERT(index >= 0 && index <= (unsigned int)kNumPts, "Index out of bounds");
			return mPt[index];
		}

		//-----------------------------------------------------------------------------
		inline
		const Dia::Maths::Vector2D& Line::operator[](unsigned int index) const
		{
			DIA_ASSERT(index >= 0 && index <= (unsigned int)kNumPts, "Index out of bounds");
			return mPt[index];
		}

		//-----------------------------------------------------------------------------
		inline
		const Dia::Maths::Vector2D& Line::GetPt1() const
		{
			return mPt[0];
		}

		//-----------------------------------------------------------------------------
		inline
		const Dia::Maths::Vector2D& Line::GetPt2() const
		{
			return mPt[1];
		}
	}
}
