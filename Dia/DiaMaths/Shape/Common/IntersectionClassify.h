#pragma once

namespace Dia
{
	namespace Maths
	{
		class IntersectionClassify
		{
		public:
			enum Classification
			{
				kNoIntersection = 0,	// Not intersecting

				kPenatrating,			// Specified form of intersecting, Objects are overlapping
				kAContainsB,			// Specified form of intersecting, B object is entirely inside A
				kBContainsA			// Specified form of intersecting, B object is entirely inside A
			};
			
			IntersectionClassify();
			IntersectionClassify(Classification classify);

			IntersectionClassify&	operator = (const IntersectionClassify& rhs);
			bool					operator == (const IntersectionClassify& rhs);
			bool					operator != (const IntersectionClassify& rhs);

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

#include "DiaMaths/Shape/Common/IntersectionClassify.inl"