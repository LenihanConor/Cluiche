#pragma once

#include "ApplicationManifest.h"
#include "ManifestValidator.h"
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

namespace Dia
{
	namespace Application
	{
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

			// Error reporting (AC17: clear error messages)
			const Dia::Core::Containers::DynamicArrayC<ManifestValidationError, 32>& GetErrors() const;
			void ClearErrors();

		private:
			ApplicationTypeRegistry& mRegistry;
			Dia::Core::Containers::DynamicArrayC<ManifestValidationError, 32> mErrors;
			ManifestValidator mValidator;

				// Helper to add errors
			void AddError(ManifestValidationResult code, const char* message, const char* context);
		};
	}
}
