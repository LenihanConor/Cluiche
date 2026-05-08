#pragma once

#include "ApplicationManifest.h"
#include "ManifestValidator.h"
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Containers/HashTables/HashTable.h>
#include <DiaCore/CRC/CRCHashFunctor.h>

namespace Json { class Value; }

namespace Dia
{
	namespace Application
	{
		// Composes multiple manifest files by resolving imports and merging content
		// Implements AC12: Import resolution with cycle detection
		class ManifestComposer
		{
		public:
			ManifestComposer();
			~ManifestComposer();

			// Compose multiple manifests into a single unified manifest
			// Resolves imports recursively (depth-first) and merges with defined rules
			ManifestValidationResult ComposeManifests(
				const Dia::Core::Containers::DynamicArrayC<const char*, 16>& filePaths,
				ApplicationManifest& outComposedManifest);

			// Compose a single manifest (with its imports resolved)
			ManifestValidationResult ComposeSingleManifest(
				const char* filePath,
				ApplicationManifest& outComposedManifest);

			// Compose from a set of typed imports relative to a base path
			ManifestValidationResult ComposeFromTypedImports(
				const char* basePath,
				const Dia::Core::Containers::DynamicArrayC<TypedImport, 16>& imports,
				ApplicationManifest& outComposedManifest);

			// Error reporting
			const Dia::Core::Containers::DynamicArrayC<ManifestValidationError, 32>& GetErrors() const;
			void ClearErrors();

		private:
			static const unsigned int kFileBufferSize = 32768;

			Dia::Core::Containers::DynamicArrayC<ManifestValidationError, 32> mErrors;
			Dia::Core::Containers::DynamicArrayC<const char*, 16> mImportStack; // For cycle detection
			Dia::Core::Containers::HashTable<Dia::Core::StringCRC, bool, Dia::Core::StringCRCHashFunctor> mProcessedFiles; // Track processed files
			char* mFileBuffer; // Reused across recursive loads to avoid per-level allocation

			// Import resolution
			bool DetectImportCycle(const char* filePath);
			ManifestValidationResult ResolveImportsRecursive(
				const char* filePath,
				ApplicationManifest& outManifest);

			// Stage import resolution — loads .diastage, resolves its .diaapp, merges phases into target PUs
			ManifestValidationResult ResolveStageImport(
				const char* diastagePath,
				ApplicationManifest& outManifest);

			// Stage merge — merges stage_phases/transitions/modules into existing PU entries
			ManifestValidationResult MergeStageManifest(
				const ApplicationManifest& stageManifest,
				ApplicationManifest& target,
				const char* sourceFilePath);

			// Merging logic
			ManifestValidationResult MergeManifests(const ApplicationManifest& source, ApplicationManifest& target, const char* sourceFilePath);

			// Deep merge JSON config
			void DeepMergeConfig(const Json::Value& source, Json::Value& target);

			// Error reporting helper
			void AddError(ManifestValidationResult code, const char* message, const char* context);

			// File loading helper
			ManifestValidationResult LoadManifestFromFile(const char* filePath, ApplicationManifest& outManifest);
		};
	}
}
