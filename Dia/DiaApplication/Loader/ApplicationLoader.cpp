#include "ApplicationLoader.h"

#include <DiaApplication/Manifest/ApplicationManifestLoader.h>
#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaCore/Core/Log.h>

namespace Dia
{
	namespace Application
	{
		// -----------------------------------------------------------------------------
		// LoadApplication (with validation result output)
		// -----------------------------------------------------------------------------
		ProcessingUnit* ApplicationLoader::LoadApplication(const char* manifestPath,
														   ManifestValidationResult& outResult)
		{
			DIA_ASSERT(manifestPath != nullptr, "Manifest path cannot be null");

			// Create manifest loader
			ApplicationManifestLoader loader;

			// Load and parse manifest
			ApplicationManifest manifest;
			outResult = loader.LoadFromFile(manifestPath, manifest);

			if (outResult != ManifestValidationResult::kSuccess)
			{
				// Log all errors
				const auto& errors = loader.GetErrors();
				DIA_LOG("Failed to load application manifest: %s", manifestPath);
				for (unsigned int i = 0; i < errors.Size(); ++i)
				{
					const ManifestValidationError& error = errors.At(i);
					DIA_LOG("  [%s] %s (context: %s)",
							ManifestValidationError::GetResultString(error.code),
							error.message,
							error.context ? error.context : "N/A");
				}
				return nullptr;
			}

			// Validate manifest
			outResult = loader.Validate(manifest);
			if (outResult != ManifestValidationResult::kSuccess)
			{
				// Log validation errors
				const auto& errors = loader.GetErrors();
				DIA_LOG("Manifest validation failed: %s", manifestPath);
				for (unsigned int i = 0; i < errors.Size(); ++i)
				{
					const ManifestValidationError& error = errors.At(i);
					DIA_LOG("  [%s] %s (context: %s)",
							ManifestValidationError::GetResultString(error.code),
							error.message,
							error.context ? error.context : "N/A");
				}
				return nullptr;
			}

			// Instantiate first ProcessingUnit
			// NOTE: Currently only supporting single ProcessingUnit per manifest
			// Multi-PU support can be added later if needed
			if (manifest.processingUnits.Size() == 0)
			{
				DIA_LOG("Manifest contains no processing units: %s", manifestPath);
				outResult = ManifestValidationResult::kMissingRequiredField;
				return nullptr;
			}

			ProcessingUnit* pu = loader.Instantiate(manifest.processingUnits.At(0));
			if (!pu)
			{
				DIA_LOG("Failed to instantiate ProcessingUnit from manifest: %s", manifestPath);
				outResult = ManifestValidationResult::kUnknownType;
				return nullptr;
			}

			DIA_LOG("Successfully loaded application from manifest: %s", manifestPath);
			return pu;
		}

		// -----------------------------------------------------------------------------
		// LoadApplication (convenience overload)
		// -----------------------------------------------------------------------------
		ProcessingUnit* ApplicationLoader::LoadApplication(const char* manifestPath)
		{
			ManifestValidationResult result;
			return LoadApplication(manifestPath, result);
		}

		// -----------------------------------------------------------------------------
		// LoadApplicationWithFallback (AC9)
		// -----------------------------------------------------------------------------
		ProcessingUnit* ApplicationLoader::LoadApplicationWithFallback(
			const char* manifestPath,
			ProcessingUnit* (*fallbackFactory)())
		{
			DIA_ASSERT(manifestPath != nullptr, "Manifest path cannot be null");
			DIA_ASSERT(fallbackFactory != nullptr, "Fallback factory cannot be null");

			// Try to load from manifest
			ManifestValidationResult result;
			ProcessingUnit* pu = LoadApplication(manifestPath, result);

			if (pu)
			{
				// Success - return manifest-loaded application
				return pu;
			}

			// Failed to load from manifest - use fallback
			DIA_LOG("Falling back to code-defined application structure (manifest load failed)");
			pu = fallbackFactory();

			if (!pu)
			{
				DIA_LOG("WARNING: Fallback factory returned nullptr!");
			}
			else
			{
				DIA_LOG("Successfully created fallback application structure");
			}

			return pu;
		}
	}
}
