#include "DiaCore/Type/TypeMember.h"

#include "DiaCore/Type/BasicTypeDefines.h"

namespace Dia
{
	namespace Core
	{
		namespace Types
		{
			TypeMember::TypeMember(){}
			
			//----------------------------------------------------------
			TypeMember::TypeMember(const char* name)
				: mNameID(name)

			{}
		}
	}
}