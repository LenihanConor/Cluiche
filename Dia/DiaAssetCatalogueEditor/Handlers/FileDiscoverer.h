#pragma once

#include <DiaAssetCatalogue/AssetTypeRegistry.h>
#include <DiaAssetCatalogue/AssetRegistry.h>
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Strings/String256.h>

namespace Dia
{
	namespace AssetCatalogue
	{
		namespace Editor
		{
			struct DiscoveredFile
			{
				char mFullPath[512];
				char mSuggestedType[64];
				char mSuggestedId[256];
			};

			static const unsigned int kMaxDiscoveredFiles = 64;

			class FileDiscoverer
			{
			public:
				// Recursively scan rootPath, matching files against registered asset types.
				// Files already registered in registry are excluded.
				// Results capped at kMaxDiscoveredFiles.
				void Discover(
					const char* rootPath,
					const Dia::AssetCatalogue::AssetTypeRegistry& typeRegistry,
					const Dia::AssetCatalogue::AssetRegistry& registry,
					Dia::Core::Containers::DynamicArrayC<DiscoveredFile, kMaxDiscoveredFiles>& outFiles);

				// Derive a "type.name" style ID from a file path relative to rootPath.
				// Strip root, lowercase, replace separators/dots-in-name with '_'.
				// Returns false if a valid ID cannot be derived.
				static bool GenerateDefaultId(
					const char* rootPath,
					const char* fullPath,
					const char* typeId,
					char* outId,
					unsigned int outIdCapacity);

			private:
				void ScanDirectory(
					const char* dir,
					const char* rootPath,
					const Dia::AssetCatalogue::AssetTypeRegistry& typeRegistry,
					const Dia::AssetCatalogue::AssetRegistry& registry,
					Dia::Core::Containers::DynamicArrayC<DiscoveredFile, kMaxDiscoveredFiles>& outFiles);
			};

		} // namespace Editor
	} // namespace AssetCatalogue
} // namespace Dia
