#pragma once

#include "DiaCore/FilePath/IFileLoad.h"

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// FileLoad
		//
		// Basic synchronous file loading implementation.
		//
		// Provides simple file reading using standard C++ file streams (std::ifstream).
		// Loads entire file into memory in one operation.
		//
		// FEATURES:
		//   - Synchronous blocking I/O
		//   - Buffer size validation
		//   - Detailed error reporting
		//
		// USAGE:
		//   FileLoad loader;
		//   char buffer[4096];
		//   if (loader.LoadNow(resolvedPath, buffer, 4096) == IFileLoad::ReturnCode::kSuccess) {
		//       // File loaded successfully
		//   }
		//
		// NOTE: For asynchronous loading, see AsyncFileLoader.h
		//---------------------------------------------------------------------------------------------------------------------------------
		class FileLoad: public IFileLoad
		{
		public:
			// Load file synchronously into buffer
			virtual IFileLoad::ReturnCode LoadNow(const FilePath::ResoledFilePath& filePath, char* outBuffer, const int outBufferMaxSize)override;

			// Async loading not yet implemented (will assert)
			virtual IFileLoad::ReturnCode LoadAsync()override;
		};
	}
}