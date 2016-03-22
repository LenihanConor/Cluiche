#pragma once

#include "DiaCore/FilePath/IFileLoad.h"

namespace Dia
{
	namespace Core
	{
		class FileLoad: public IFileLoad
		{
		public:
			virtual IFileLoad::ReturnCode LoadNow(const FilePath::ResoledFilePath& filePath, char* outBuffer, const int outBufferMaxSize)override;
			virtual IFileLoad::ReturnCode LoadAsync()override;
		}; 
	}
}