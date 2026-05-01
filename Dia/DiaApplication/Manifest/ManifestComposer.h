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

			// Compose multiple manifests into a single unified manifest
			// Resolves imports recursively (depth-first) and merges with defined rules
			// @param filePaths - Root manifest file paths to compose
			// @param outComposedManifest - Output composed manifest
			// @return Validation result (kSuccess or error code)
			ManifestValidationResult ComposeManifests(
				const Dia::Core::Containers::DynamicArrayC<const char*, 16>& filePaths,
				ApplicationManifest& outComposedManifest);

			// Compose a single manifest (with its imports resolved)
			// @param filePath - Root manifest file path
			// @param outComposedManifest - Output composed manifest
			// @return Validation result (kSuccess or error code)
			ManifestValidationResult ComposeSingleManifest(
				const char* filePath,
				ApplicationManifest& outComposedManifest);

			// Error reporting
			const Dia::Core::Containers::DynamicArrayC<ManifestValidationError, 32>& GetErrors() const;
			void ClearErrors();

		private:
			Dia::Core::Containers::DynamicArrayC<ManifestValidationError, 32> mErrors;
			Dia::Core::Containers::DynamicArrayC<const char*, 16> mImportStack; // For cycle detection
			Dia::Core::Containers::HashTable<Dia::Core::StringCRC, bool, Dia::Core::StringCRCHashFunctor> mProcessedFiles; // Track processed files

			// Import resolution
			bool DetectImportCycle(const char* filePath);
			ManifestValidationResult ResolveImportsRecursive(
				const char* filePath,
				ApplicationManifest& outManifest);

			// Merging logic (AC12 merge rules)
			void MergeManifests(const ApplicationManifest& source, ApplicationManifest& target);
			void MergeProcessingUnit(
				const ApplicationManifest::ProcessingUnitEntry& source,
				ApplicationManifest::ProcessingUnitEntry& target);
			void MergePhases(
				const Dia::Core::Containers::DynamicArrayC<ApplicationManifest::PhaseEntry, 16>& sourcePhases,
				Dia::Core::Containers::DynamicArrayC<ApplicationManifest::PhaseEntry, 16>& targetPhases);
			void MergeModules(
				const Dia::Core::Containers::DynamicArrayC<ApplicationManifest::ModuleEntry, 32>& sourceModules,
				Dia::Core::Containers::DynamicArrayC<ApplicationManifest::ModuleEntry, 32>& targetModules);
			void MergeTransitions(
				const Dia::Core::Containers::DynamicArrayC<ApplicationManifest::PhaseTransition, 32>& sourceTransitions,
				Dia::Core::Containers::DynamicArrayC<ApplicationManifest::PhaseTransition, 32>& targetTransitions);

			// Deep merge JSON config (AC12 rule 3: deep merge module configs)
			void DeepMergeConfig(const Json::Value& source, Json::Value& target);

			// Error reporting helper
			void AddError(ManifestValidationResult code, const char* message, const char* context);

			// File loading helper (stub for now - will integrate with loader)
			ManifestValidationResult LoadManifestFromFile(const char* filePath, ApplicationManifest& outManifest);
		};
	}
}
