#ifndef DIA_TYPE_FACADE_H
#define DIA_TYPE_FACADE_H

#include "DiaCore/Type/TypeRegistry.h"
#include "DiaCore/Type/TypeTextSerializer.h"
#include "DiaCore/Type/TypeJsonSerializer.h"

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
			
				TypeJsonSerializer&			JsonSerializer();
				const TypeJsonSerializer&	JsonSerializer()const;

			private:
				TypeRegistry			mRegistry;					// Map from Type to CRC id
				TypeTextSerializer		mTextSerializer;			// Converts too and from text buffer.
				TypeJsonSerializer		mJsonSerializer;			// Converts too and from json buffer.
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