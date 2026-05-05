#pragma once

#include "DiaAssetCatalogue/LoadResult.h"
#include "DiaAssetCatalogue/AssetRegistry.h"

namespace Dia
{
	namespace AssetCatalogue
	{
		//---------------------------------------------------------------------------------------------------------
		// CatalogueManifestSerializer
		//
		// Reads and writes catalogue manifest JSON files.
		//
		// LoadManifest:
		//   - Reads the JSON file at the given path (raw C string)
		//   - Populates an AssetRegistry with the records
		//   - Resolves single-level "includes" (sub-manifests referenced by relative path)
		//   - Rejects duplicate IDs with a LoadError (continues loading remaining records)
		//   - If a sub-manifest itself has an "includes" field, returns a LoadError
		//
		// SaveManifest:
		//   - Serializes the current registry state to a flat JSON file (no includes output)
		//
		// Uses jsoncpp directly (not JsonDefinitionLoader / TypeJsonSerializer).
		//---------------------------------------------------------------------------------------------------------
		class CatalogueManifestSerializer
		{
		public:
			CatalogueManifestSerializer();

			// Load a manifest from a file path (null-terminated C string).
			// Returns a LoadResult<AssetRegistry> with success flag and any errors.
			LoadResult<AssetRegistry> LoadManifest(const char* path) const;

			// Save a registry to a manifest file path (null-terminated C string).
			// Returns true on success.
			bool SaveManifest(const AssetRegistry& registry, const char* path) const;

		private:
			// Parse a single JSON "assets" array into the registry.
			// parentPath is used for error messages only.
			// If isSubManifest is true, the presence of an "includes" field is an error.
			void ParseAssetsFromJson(
				const char* jsonText,
				const char* sourcePath,
				bool isSubManifest,
				LoadResult<AssetRegistry>& result) const;
		};

	} // namespace AssetCatalogue
} // namespace Dia
