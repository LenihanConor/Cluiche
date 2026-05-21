#pragma once

#include "DiaAssetCatalogue/LoadResult.h"

#include "DiaCore/Reflect/JsonArchive.h"
#include "DiaCore/FilePath/FilePath.h"
#include "DiaCore/Containers/Strings/StringReader.h"

namespace Dia
{
	namespace AssetCatalogue
	{
		//---------------------------------------------------------------------------------------------------------
		// JsonDefinitionLoader
		//
		// Loads a JSON file from disk (or buffer), deserializes it into a typed C++ object using
		// DiaReflect's JsonReadArchive.
		// Returns LoadResult<T> with either the populated object or detailed LoadError entries.
		//
		// T must have a serialize() free function registered via DIA_SERIALIZE.
		// Required-field errors are reported by JsonReadArchive via DIA_FIELD_REQUIRED.
		//---------------------------------------------------------------------------------------------------------
		class JsonDefinitionLoader
		{
		public:
			JsonDefinitionLoader() = default;

			template<typename T>
			LoadResult<T> Load(const Dia::Core::FilePath& path) const;

			template<typename T>
			LoadResult<T> LoadFromBuffer(const Dia::Core::Containers::StringReader& buffer) const;
		};

	} // namespace AssetCatalogue
} // namespace Dia

#include "DiaAssetCatalogue/JsonDefinitionLoader.inl"
