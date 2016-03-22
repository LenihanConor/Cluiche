#pragma once

#include "DiaCore/FilePath/FilePath.h"

#include <DiaCore/Core/EnumClass.h>

namespace Dia
{
	namespace Core
	{
		class IFileLoad
		{
		public:
			CLASSEDENUM(ReturnCode, \
				CE_ITEMSTART(kSuccess)\
				CE_ITEM(kFailureGeneric)\
				CE_ITEM(kFailureBufferToSmall)\
				CE_ITEM(kFailureCouldNotOpenFile)\
				, kSuccess \
				);

			virtual ReturnCode LoadNow(const FilePath::ResoledFilePath& filePath, char* buffer, const int bufferMaxSize) = 0;
			virtual ReturnCode LoadAsync() = 0;
		}; 
	}
}