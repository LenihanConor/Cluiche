#pragma once

#include "DiaCore/FilePath/FileLoad.h"

#include <DiaCore/Containers/Strings/StringReader.h>
#include <DiaCore/Type/TypeFacade.h>

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// SerializedFileLoad
		//
		// Template class for loading and deserializing JSON files into typed objects.
		//
		// Extends FileLoad to add automatic JSON deserialization using the Dia type system.
		// Any class with a DIA_TYPE_DEFINITION can be loaded directly from JSON.
		//
		// USAGE:
		//   PathStoreConfig config;
		//   SerializedFileLoad loader;
		//   if (loader.LoadNow<PathStoreConfig, 4096>(filePath, config) == IFileLoad::ReturnCode::kSuccess) {
		//       // config is now populated from JSON
		//   }
		//
		// REQUIREMENTS:
		//   - Target type T must have DIA_TYPE_DEFINITION macro defined
		//   - File must contain valid JSON matching the type's structure
		//
		// NOTE: Uses stack allocation for the buffer (default 10KB)
		//---------------------------------------------------------------------------------------------------------------------------------
		class SerializedFileLoad: public FileLoad
		{
		public:
			// Load and deserialize a JSON file into a typed object
			// @tparam T - Type to deserialize into (must have DIA_TYPE_DEFINITION)
			// @tparam bufferSize - Stack buffer size for file contents (default 10KB)
			// @param filePath - Resolved file path to load
			// @param outObject - Output object to populate
			// @param outBufferMaxSize - Maximum buffer size (unused, kept for interface consistency)
			// @return ReturnCode indicating success or failure
			template<class T, const int bufferSize = 10000>
			IFileLoad::ReturnCode LoadNow(const FilePath::ResoledFilePath& filePath, T& outObject, const int outBufferMaxSize)
			{
				// Load file contents into stack buffer
				char getdata[bufferSize];
				IFileLoad::ReturnCode fileLoadReturnCode = FileLoad::LoadNow(filePath, getdata, bufferSize);
				if (fileLoadReturnCode != IFileLoad::ReturnCode::kSuccess)
				{
					return fileLoadReturnCode;
				}

				// Deserialize JSON into the output object
				Dia::Core::Containers::StringReader bufferDeserial(&getdata[0]);

				//TODO - Deserialize interface to be passed in
				Dia::Core::Types::GetTypeFacade().JsonSerializer().Deserialize(outObject, bufferDeserial, filePath.AsCStr());

				return IFileLoad::ReturnCode::kSuccess;
			}
		};
	}
}