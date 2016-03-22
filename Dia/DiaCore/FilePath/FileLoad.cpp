#include "DiaCore/FilePath/FileLoad.h"

#include "DiaCore/Core/Assert.h"
#include <fstream>



namespace Dia
{	
	namespace Core
	{
		IFileLoad::ReturnCode FileLoad::LoadNow(const FilePath::ResoledFilePath& filePath, char* outBuffer, const int outBufferMaxSize)
		{
			IFileLoad::ReturnCode returnCode = IFileLoad::ReturnCode::kFailureGeneric;
			std::ifstream f(filePath.AsCStr());

			if (f.is_open())
			{
				f.read(outBuffer, outBufferMaxSize);
				if (f.eof())
				{
					returnCode = IFileLoad::ReturnCode::kSuccess;

				}
				else if (f.fail())
				{
					// some other error...
					DIA_ASSERT(0, "Some file load fail that i should be getting better assert data on");

					returnCode = IFileLoad::ReturnCode::kFailureGeneric;
				}
				else
				{
					DIA_ASSERT(0, "Loading File [%s], buffer too small", filePath.AsCStr());

					returnCode = IFileLoad::ReturnCode::kFailureBufferToSmall;
				}
			}
			else
			{
				DIA_ASSERT(0, "Coudl not open file [%s]", filePath.AsCStr());

				returnCode = IFileLoad::ReturnCode::kFailureCouldNotOpenFile;
			}

			f.close();

			return returnCode;
		}

		IFileLoad::ReturnCode FileLoad::LoadAsync()
		{
			DIA_ASSERT(0, "Function not valid yet!");

			return IFileLoad::ReturnCode::kFailureGeneric;
		}
	}
}