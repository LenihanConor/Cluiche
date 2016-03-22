#pragma once

#include "DiaCore/FilePath/FileLoad.h"

#include <DiaCore/Containers/Strings/StringReader.h>
#include <DiaCore/Type/TypeFacade.h>

namespace Dia
{
	namespace Core
	{
		class SerializedFileLoad: public FileLoad
		{
		public:	
			template<class T, const int bufferSize = 10000>
			IFileLoad::ReturnCode LoadNow(const FilePath::ResoledFilePath& filePath, T& outObject, const int outBufferMaxSize)
			{

				char getdata[bufferSize];
				IFileLoad::ReturnCode fileLoadReturnCode = FileLoad::LoadNow(filePath, getdata, bufferSize);
				if (fileLoadReturnCode != IFileLoad::ReturnCode::kSuccess)
				{
					return fileLoadReturnCode;
				}

				Dia::Core::Containers::StringReader bufferDeserial(&getdata[0]);

				//TODO - Deserialize error message
				//TODO Derialize interface to be passed in
				Dia::Core::Types::GetTypeFacade().JsonSerializer().Deserialize(outObject, bufferDeserial);

				return IFileLoad::ReturnCode::kSuccess;
			}
		}; 
	}
}