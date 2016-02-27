#include "DiaCore/FilePath/PathStoreConfig.h"

namespace Dia
{	
	namespace Core
	{
		DIA_TYPE_DEFINITION(AliasPathConfigTuple)
			DIA_TYPE_ADD_VARIABLE("mAlias", mAlias)
			DIA_TYPE_ADD_VARIABLE("mPath", mPath)
		DIA_TYPE_DEFINITION_END()

		DIA_TYPE_DEFINITION(AliasAppendPathConfig)
			DIA_TYPE_ADD_VARIABLE("mAlias", mAlias)
			DIA_TYPE_ADD_VARIABLE("mBaseAlias", mBaseAlias)
			DIA_TYPE_ADD_VARIABLE("mPathAppend", mPathAppend)
		DIA_TYPE_DEFINITION_END()

		DIA_TYPE_DEFINITION(PathStoreConfig)
			DIA_TYPE_ADD_VARIABLE("mAliasPathTupleArray", mAliasPathTupleArray)
			DIA_TYPE_ADD_VARIABLE("mAliasAppendPathArray", mAliasAppendPathArray)
		DIA_TYPE_DEFINITION_END()
	}
}