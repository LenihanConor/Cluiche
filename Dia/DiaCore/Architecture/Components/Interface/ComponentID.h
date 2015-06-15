#ifndef DIA_COMPONENT_ID
#define DIA_COMPONENT_ID

namespace Dia
{
	namespace Core
	{
		/******************************************************************************/
		/*!
			\typedef  ComponentClassID

			\brief  Unique id for a componet class (not per component)
		*/
		/******************************************************************************/
		typedef unsigned int ComponentClassID;

		static const ComponentClassID INVALID_COMPONENT_ID = -1;
	}
}

#endif
