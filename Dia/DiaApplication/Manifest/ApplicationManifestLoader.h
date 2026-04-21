#pragma once

#include "ApplicationManifest.h"
#include "ManifestValidator.h"
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
	namespace Application
	{
		class ProcessingUnit;
		class ApplicationTypeRegistry;

		// ApplicationManifestLoader
		// Loads and parses .diaapp manifest files, validates them, and instantiates ProcessingUnits
		class ApplicationManifestLoader
		{
		public:
			explicit ApplicationManifestLoader(ApplicationTypeRegistry& registry);
			~ApplicationManifestLoader();

			// Load manifest from file path
			// @param filePath - Path to .diaapp JSON file
			// @param outManifest - Output manifest structure
			// @return Validation result (kSuccess if loaded successfully)
			ManifestValidationResult LoadFromFile(const char* filePath, ApplicationManifest& outManifest);

			// Load manifest from JSON string
			// @param jsonString - JSON string containing manifest
			// @param outManifest - Output manifest structure
			// @return Validation result (kSuccess if loaded successfully)
			ManifestValidationResult LoadFromString(const char* jsonString, ApplicationManifest& outManifest);

			// Validate a manifest
			// @param manifest - Manifest to validate
			// @return Validation result (kSuccess if valid)
			ManifestValidationResult Validate(const ApplicationManifest& manifest);

			// Instantiate a ProcessingUnit from a manifest entry
			// @param entry - ProcessingUnit entry from manifest
			// @return Created ProcessingUnit (caller owns it), or nullptr on failure
			ProcessingUnit* Instantiate(const ApplicationManifest::ProcessingUnitEntry& entry);

			// Compose multiple manifests (resolve imports)
			// @param filePaths - Array of manifest file paths to compose
			// @param outComposedManifest - Output composed manifest
			// @return Validation result (kSuccess if composed successfully)
			// NOTE: Currently delegates to ManifestComposer (to be implemented separately)
			ManifestValidationResult ComposeManifests(const Dia::Core::Containers::DynamicArrayC<const char*, 16>& filePaths,
				ApplicationManifest& outComposedManifest);

			// Hot reload support (AC8: runtime manifest reloading)
			// @param filePath - Path to new .diaapp manifest file
			// @param existingPU - ProcessingUnit to reload
			// @return Validation result (kSuccess if reloaded successfully)
			// NOTE: Reloads manifest and applies changes to existing ProcessingUnit
			//       - Validates new manifest before applying (rollback on failure)
			//       - Stops removed modules/phases
			//       - Adds new modules/phases
			//       - Updates configuration for existing modules
			//       - Uses HotReloadManager for module state preservation
			ManifestValidationResult ReloadManifest(const char* filePath, ProcessingUnit* existingPU);

			// Error reporting (AC17: clear error messages)
			const Dia::Core::Containers::DynamicArrayC<ManifestValidationError, 32>& GetErrors() const;
			void ClearErrors();

		private:
			ApplicationTypeRegistry& mRegistry;
			Dia::Core::Containers::DynamicArrayC<ManifestValidationError, 32> mErrors;
			ManifestValidator mValidator;

			// Parse JSON into manifest structure
			bool ParseJSON(const Json::Value& root, ApplicationManifest& outManifest);

			// Helper methods for parsing each section
			bool ParseProcessingUnit(const Json::Value& puJson, ApplicationManifest::ProcessingUnitEntry& outEntry);
			bool ParsePhase(const Json::Value& phaseJson, ApplicationManifest::PhaseEntry& outEntry);
			bool ParseModule(const Json::Value& moduleJson, ApplicationManifest::ModuleEntry& outEntry);
			bool ParsePhaseTransition(const Json::Value& transitionJson, ApplicationManifest::PhaseTransition& outTransition);

			// Helper to add errors
			void AddError(ManifestValidationResult code, const char* message, const char* context);
		};
	}
}
