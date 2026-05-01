// ===================================================================
// FileLoad.cpp
// Basic synchronous file loading implementation
// ===================================================================

#include "DiaCore/FilePath/FileLoad.h"

#include "DiaCore/Core/Assert.h"
#include <fstream>



namespace Dia
{
	namespace Core
	{
		// Load file synchronously into the provided buffer
		// Uses std::ifstream for file reading
		// Returns specific error codes based on failure type
		IFileLoad::ReturnCode FileLoad::LoadNow(const FilePath::ResoledFilePath& filePath, char* outBuffer, const int outBufferMaxSize)
		{
			IFileLoad::ReturnCode returnCode = IFileLoad::ReturnCode::kFailureGeneric;
			std::ifstream f(filePath.AsCStr());

			if (f.is_open())
			{
				// Attempt to read the entire file
				f.read(outBuffer, outBufferMaxSize);

				if (f.eof())
				{
					// Successfully read entire file
					returnCode = IFileLoad::ReturnCode::kSuccess;
				}
				else if (f.fail())
				{
					// I/O error occurred during reading
					DIA_ASSERT(0, "Some file load fail that i should be getting better assert data on");

					returnCode = IFileLoad::ReturnCode::kFailureGeneric;
				}
				else
				{
					// File is larger than buffer - partial read occurred
					DIA_ASSERT(0, "Loading File [%s], buffer too small", filePath.AsCStr());

					returnCode = IFileLoad::ReturnCode::kFailureBufferToSmall;
				}
			}
			else
			{
				// Could not open file (file doesn't exist or no permissions)
				DIA_ASSERT(0, "Could not open file [%s]", filePath.AsCStr());

				returnCode = IFileLoad::ReturnCode::kFailureCouldNotOpenFile;
			}

			f.close();

			return returnCode;
		}

		// Async loading is not yet implemented
		// For async file loading, use AsyncFileLoader instead
		IFileLoad::ReturnCode FileLoad::LoadAsync()
		{
			DIA_ASSERT(0, "Function not valid yet!");

			return IFileLoad::ReturnCode::kFailureGeneric;
		}
	}
}