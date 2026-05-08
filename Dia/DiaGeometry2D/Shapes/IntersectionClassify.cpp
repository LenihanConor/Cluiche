#include "DiaGeometry2D/Shapes/IntersectionClassify.h"

namespace Dia
{
	namespace Geometry2D
	{
		// Provide non-inline implementation for SetClassification
		// (needed when function is used in exception handling or address is taken)
		void IntersectionClassify::SetClassification(Classification classify)
		{
			mResult = classify;
		}
	}
}

// Other methods are implemented inline in IntersectionClassify.inl
