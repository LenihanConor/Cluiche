#include <DiaValueSchema/CategoricalColours.h>

namespace Dia::ValueSchema
{
	const char* GetCategoricalColour(unsigned int index)
	{
		static const char* const kColours[kCategoricalColourCount] =
		{
			"#4E79A7", "#F28E2B", "#E15759", "#76B7B2", "#59A14F", "#EDC948",
			"#B07AA1", "#FF9DA7", "#9C755F", "#BAB0AC", "#1F77B4", "#8CD17D"
		};
		return kColours[index % kCategoricalColourCount];
	}
}
