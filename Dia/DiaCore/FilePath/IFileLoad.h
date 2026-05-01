#pragma once

#include "DiaCore/FilePath/FilePath.h"

#include <DiaCore/Core/EnumClass.h>

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// IFileLoad
		//
		// Interface for file loading operations.
		//
		// Defines a common contract for synchronous and asynchronous file loading.
		// Implementations can provide different loading strategies (buffered, streaming, cached, etc.)
		//
		// USAGE:
		//   IFileLoad* loader = new FileLoad();
		//   char buffer[1024];
		//   IFileLoad::ReturnCode result = loader->LoadNow(filePath, buffer, 1024);
		//   if (result == IFileLoad::ReturnCode::kSuccess) {
		//       // Process loaded data
		//   }
		//---------------------------------------------------------------------------------------------------------------------------------
		class IFileLoad
		{
		public:
			// Return codes for file loading operations
			CLASSEDENUM(ReturnCode, \
				CE_ITEMSTART(kSuccess)\
				CE_ITEM(kFailureGeneric)\
				CE_ITEM(kFailureBufferToSmall)\
				CE_ITEM(kFailureCouldNotOpenFile)\
				, kSuccess \
				);

			// Load file synchronously into the provided buffer
			// @param filePath - Resolved file path to load
			// @param buffer - Destination buffer for file contents
			// @param bufferMaxSize - Maximum size of the buffer
			// @return ReturnCode indicating success or failure reason
			virtual ReturnCode LoadNow(const FilePath::ResoledFilePath& filePath, char* buffer, const int bufferMaxSize) = 0;

			// Load file asynchronously (implementation-specific behavior)
			// @return ReturnCode indicating whether async operation started successfully
			virtual ReturnCode LoadAsync() = 0;
		};
	}
}