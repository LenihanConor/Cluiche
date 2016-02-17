#include "DiaCore/FilePath/PathStoreConfig.h"

namespace Dia
{	
	namespace Core
	{
		DIA_TYPE_DEFINITION(PathStoreConfigTuple)
			DIA_TYPE_ADD_VARIABLE("mAlias", mAlias)
			DIA_TYPE_ADD_VARIABLE("mPath", mPath)
		DIA_TYPE_DEFINITION_END()

		DIA_TYPE_DEFINITION(PathStoreConfig)
			DIA_TYPE_ADD_VARIABLE("mTupleArray", mTupleArray)
		DIA_TYPE_DEFINITION_END()
	}
}