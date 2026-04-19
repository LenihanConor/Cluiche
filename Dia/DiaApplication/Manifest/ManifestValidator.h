#pragma once

#include "ApplicationManifest.h"
#include <DiaCore/Containers/Arrays/DynamicArrayC.h>
#include <DiaCore/Containers/HashTables/HashTable.h>
#include <DiaCore/CRC/CRCHashFunctor.h>

namespace Dia
{
	namespace Application
	{
		// Validation result codes
		enum class ManifestValidationResult
		{
			kSuccess,
			kSchemaVersionUnsupported,
			kMissingRequiredField,
			kInvalidJSON,
			kUnknownType,                // Type not registered
			kDuplicateInstanceId,
			kCircularDependency,
			kInvalidPhaseTransition,
			kModuleMissingFromPhase,     // Module references non-existent phase
			kImportNotFound,             // Imported manifest not found
			kImportCycle                 // Circular import
		};

		// Error context for clear error messages (AC17)
		struct ManifestValidationError
		{
			ManifestValidationResult code;
			const char* message;   // Detailed error message
			const char* context;   // JSON path (e.g., "processing_units[0].modules[2]")

			ManifestValidationError();
			ManifestValidationError(ManifestValidationResult code, const char* message, const char* context);

			const char* ToString() const;
			static const char* GetResultString(ManifestValidationResult result);
		};

		// Validation logic for manifests
		class ManifestValidator
		{
		public:
			ManifestValidator();

			// Validate entire manifest
			ManifestValidationResult Validate(const ApplicationManifest& manifest);

			// Specific validation checks
			bool ValidateSchema(const ApplicationManifest& manifest);
			bool ValidateTypes(const ApplicationManifest& manifest);
			bool ValidateDependencies(const ApplicationManifest::ProcessingUnitEntry& entry, const char* context);
			bool ValidatePhaseTransitions(const ApplicationManifest::ProcessingUnitEntry& entry, const char* context);
			bool ValidateModulePhaseReferences(const ApplicationManifest::ProcessingUnitEntry& entry, const char* context);
			bool ValidateDuplicateInstanceIds(const ApplicationManifest::ProcessingUnitEntry& entry, const char* context);
			bool ValidateOrphanedModules(const ApplicationManifest::ProcessingUnitEntry& entry, const char* context);

			// Error reporting (AC17: collect all errors)
			const Dia::Core::Containers::DynamicArrayC<ManifestValidationError, 32>& GetErrors() const;
			void ClearErrors();

		private:
			Dia::Core::Containers::DynamicArrayC<ManifestValidationError, 32> mErrors;

			void AddError(ManifestValidationResult code, const char* message, const char* context);

			// Circular dependency detection helper
			bool DetectCircularDependencies(
				const Dia::Core::Containers::DynamicArrayC<ApplicationManifest::ModuleEntry, 32>& modules,
				const char* context);

			bool HasCycleDFS(
				const Dia::Core::StringCRC& moduleId,
				const Dia::Core::Containers::HashTable<Dia::Core::StringCRC, Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>, Dia::Core::StringCRCHashFunctor>& adjList,
				Dia::Core::Containers::HashTable<Dia::Core::StringCRC, bool, Dia::Core::StringCRCHashFunctor>& visited,
				Dia::Core::Containers::HashTable<Dia::Core::StringCRC, bool, Dia::Core::StringCRCHashFunctor>& recStack,
				Dia::Core::Containers::DynamicArrayC<Dia::Core::StringCRC, 32>& cycle);
		};
	}
}
