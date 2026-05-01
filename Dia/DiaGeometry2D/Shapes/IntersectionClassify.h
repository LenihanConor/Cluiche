#ifndef DIA_GEOMETRY2D_INTERSECTIONCLASSIFY_H
#define DIA_GEOMETRY2D_INTERSECTIONCLASSIFY_H

namespace Dia
{
	namespace Geometry2D
	{
		class IntersectionClassify
		{
		public:
			enum Classification
			{
				kNoIntersection = 0,	// Not intersecting

				kPenatrating,			// Specified form of intersecting, Objects are overlapping
				kAContainsB,			// Specified form of intersecting, B object is entirely inside A
				kBContainsA				// Specified form of intersecting, B object is entirely inside A
			};

			IntersectionClassify();
			IntersectionClassify(Classification classify);

			IntersectionClassify&	operator = (const IntersectionClassify& rhs);
			bool					operator == (const IntersectionClassify& rhs) const;
			bool					operator != (const IntersectionClassify& rhs) const;

			bool			IsIntersecting()const;
			bool			IsNotIntersecting()const;

			bool			IsContainment()const;
			bool			IsNotContainment()const;

			void			SetClassification(Classification classify);
			Classification	GetClassification()const;

			IntersectionClassify&	ReInterpretAandBObject();
		private:
			Classification mResult;
		};
	}
}

#include "DiaGeometry2D/Shapes/IntersectionClassify.inl"

#endif // DIA_GEOMETRY2D_INTERSECTIONCLASSIFY_H
