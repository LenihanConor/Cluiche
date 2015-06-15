#ifndef DIA_TYPE_FACADE_H
#define DIA_TYPE_FACADE_H

#include "DiaCore/Type/TypeRegistry.h"
#include "DiaCore/Type/TypeTextSerializer.h"

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			//------------------------------------------------------------------------------------
			//	Interface
			//------------------------------------------------------------------------------------
			class TypeFacade
			{
			public:
				TypeFacade();
				
				TypeRegistry&				Registry();
				const TypeRegistry&			Registry()const;

				TypeTextSerializer&			TextSerializer();
				const TypeTextSerializer&	TextSerializer()const;
			
			private:
				TypeRegistry			mRegistry;					// Map from Type to CRC id
				TypeTextSerializer		mTextSerializer;			// Converts too and from text buffer.
			};

			static TypeFacade& GetTypeFacade()
			{
				static TypeFacade sTypeFacade;
				return sTypeFacade;
			}			
		}
	}
}

#include "DiaCore/Type/TypeFacade.inl"

#endif