#pragma once

#include "DiaAssetCatalogue/LoadResult.h"

#include "DiaCore/Type/TypeRegistry.h"
#include "DiaCore/Type/TypeJsonSerializer.h"
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
		// DiaCore's TypeJsonSerializer, and validates required fields.
		// Returns LoadResult<T> with either the populated object or detailed LoadError entries.
		//---------------------------------------------------------------------------------------------------------
		class JsonDefinitionLoader
		{
		public:
			explicit JsonDefinitionLoader(const Dia::Core::Types::TypeRegistry& registry);

			template<typename T>
			LoadResult<T> Load(const Dia::Core::FilePath& path) const;

			template<typename T>
			LoadResult<T> LoadFromBuffer(const Dia::Core::Containers::StringReader& buffer) const;

		private:
			template<typename T>
			void ValidateRequiredFields(const T& value, const char* jsonText, LoadResult<T>& result) const;

			const Dia::Core::Types::TypeRegistry& mRegistry;
		};

	} // namespace AssetCatalogue
} // namespace Dia

#include "DiaAssetCatalogue/JsonDefinitionLoader.inl"
