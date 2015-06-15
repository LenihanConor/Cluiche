#include "DiaCore/Type/TypeFacade.h"


namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			TypeFacade::TypeFacade()
				: mRegistry()
			{
				mTextSerializer.Initilize(&mRegistry);
			}

			TypeRegistry& TypeFacade::Registry()
			{
				return mRegistry;
			}

			const TypeRegistry&	TypeFacade::Registry()const
			{
				return mRegistry;
			}
		}
	}
}