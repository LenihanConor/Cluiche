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

			// Non-const: populates mRulesPath from the manifest's "rules_path" field.
			// Call GetRulesPath() after a successful load to retrieve it.
			LoadResult<AssetRegistry> LoadManifest(const char* path);

			// Save a registry to a manifest file path (null-terminated C string).
			// If a rules_path has been set (via load or SetRulesPath), it is preserved.
			// Returns true on success.
			bool SaveManifest(const AssetRegistry& registry, const char* path) const;

			// Rules path accessors — relative path stored in the manifest.
			const char* GetRulesPath() const;
			void SetRulesPath(const char* path);
			bool HasRulesPath() const;

		private:
			void ParseAssetsFromJson(
				const char* jsonText,
				const char* sourcePath,
				bool isSubManifest,
				LoadResult<AssetRegistry>& result) const;

			static const unsigned int kRulesPathLength = 256;
			char mRulesPath[kRulesPathLength];
		};

	} // namespace AssetCatalogue
} // namespace Dia
