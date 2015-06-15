#include "DiaMaths/Shape/Common/IntersectionClassify.h"

namespace Dia
{
	namespace Maths
	{
		void IntersectionClassify::SetClassification(Classification classify)
		{
			mResult = classify;
		}

		IntersectionClassify& IntersectionClassify::ReInterpretAandBObject()
		{
			switch (mResult)
			{
			case kAContainsB: mResult = kBContainsA; break;
			case kBContainsA: mResult = kAContainsB; break;
			default: break;
			}

			return *this;
		}
	}
}