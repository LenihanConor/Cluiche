#pragma once

#include "DiaCore/FilePath/FileLoad.h"
#include "DiaCore/Reflect/JsonArchive.h"
#include "DiaCore/Reflect/DiaCoreSerializers.h"
#include "DiaCore/Json/external/json/json.h"

#include <DiaCore/Containers/Strings/StringReader.h>

namespace Dia
{
	namespace Core
	{
		//---------------------------------------------------------------------------------------------------------------------------------
		// SerializedFileLoad
		//
		// Template class for loading and deserializing JSON files into typed objects.
		//
		// Extends FileLoad to add automatic JSON deserialization using DiaReflect JsonReadArchive.
		// Any type with a DIA_SERIALIZE free function can be loaded directly from JSON.
		//
		// USAGE:
		//   PathStoreConfig config;
		//   SerializedFileLoad loader;
		//   if (loader.LoadNow<PathStoreConfig, 4096>(filePath, config) == IFileLoad::ReturnCode::kSuccess) {
		//       // config is now populated from JSON
		//   }
		//
		// REQUIREMENTS:
		//   - Target type T must have a serialize() free function (via DIA_SERIALIZE)
		//   - File must contain valid JSON matching the type's structure
		//---------------------------------------------------------------------------------------------------------------------------------
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

				Json::Value root;
				Json::Reader reader;
				if (!reader.parse(getdata, root, false))
				{
					return IFileLoad::ReturnCode::kFailureGeneric;
				}

				Dia::Reflect::JsonReadArchive ar(root);
				serialize(ar, outObject, 0u);

				return IFileLoad::ReturnCode::kSuccess;
			}
		};
	}
}
