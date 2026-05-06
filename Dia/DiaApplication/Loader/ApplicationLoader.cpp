#include "ApplicationLoader.h"

#include <DiaApplication/Manifest/ApplicationManifestLoader.h>
#include <DiaApplication/TypeRegistry/ApplicationTypeRegistry.h>
#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaLogger/DiaLog.h>

namespace Dia
{
	namespace Application
	{
		// -----------------------------------------------------------------------------
		// LoadApplication (with validation result output)
		// -----------------------------------------------------------------------------
		ProcessingUnit* ApplicationLoader::LoadApplication(ApplicationTypeRegistry& registry,
														   const char* manifestPath,
														   ManifestValidationResult& outResult)
		{
			DIA_ASSERT(manifestPath != nullptr, "Manifest path cannot be null");

			ApplicationManifestLoader loader(registry);

			// Load and parse manifest
			ApplicationManifest manifest;
			outResult = loader.LoadFromFile(manifestPath, manifest);

			if (outResult != ManifestValidationResult::kSuccess)
			{
				const auto& errors = loader.GetErrors();
				DIA_LOG_ERROR("Application", "Failed to load application manifest: %s", manifestPath);
				for (unsigned int i = 0; i < errors.Size(); ++i)
				{
					const ManifestValidationError& error = errors[i];
					DIA_LOG_ERROR("Application", "  [%s] %s (context: %s)",
							ManifestValidationError::GetResultString(error.code),
							error.message.AsCStr(),
							error.context.IsEmpty() ? "N/A" : error.context.AsCStr());
				}
				return nullptr;
			}

			if (manifest.processingUnits.Size() == 0)
			{
				DIA_LOG_ERROR("Application", "Manifest contains no processing units: %s", manifestPath);
				outResult = ManifestValidationResult::kMissingRequiredField;
				return nullptr;
			}

			// Find the root PU (marked with "root": true), falling back to index 0
			unsigned int rootIndex = 0;
			for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
			{
				if (manifest.processingUnits[i].root)
				{
					rootIndex = i;
					break;
				}
			}

			ProcessingUnit* pu = loader.Instantiate(manifest.processingUnits[rootIndex]);
			if (!pu)
			{
				DIA_LOG_ERROR("Application", "Failed to instantiate ProcessingUnit from manifest: %s", manifestPath);
				outResult = ManifestValidationResult::kUnknownType;
				return nullptr;
			}

			pu->Initialize();

			DIA_LOG_INFO("Application", "Successfully loaded application from manifest: %s", manifestPath);
			return pu;
		}

		// -----------------------------------------------------------------------------
		// LoadApplication (convenience overload)
		// -----------------------------------------------------------------------------
		ProcessingUnit* ApplicationLoader::LoadApplication(ApplicationTypeRegistry& registry,
														   const char* manifestPath)
		{
			ManifestValidationResult result;
			return LoadApplication(registry, manifestPath, result);
		}

		// -----------------------------------------------------------------------------
		// LoadApplicationWithFallback (AC9)
		// -----------------------------------------------------------------------------
		ProcessingUnit* ApplicationLoader::LoadApplicationWithFallback(
			ApplicationTypeRegistry& registry,
			const char* manifestPath,
			ProcessingUnit* (*fallbackFactory)())
		{
			DIA_ASSERT(manifestPath != nullptr, "Manifest path cannot be null");
			DIA_ASSERT(fallbackFactory != nullptr, "Fallback factory cannot be null");

			ManifestValidationResult result;
			ProcessingUnit* pu = LoadApplication(registry, manifestPath, result);

			if (pu)
			{
				// Success - return manifest-loaded application
				return pu;
			}

			// Failed to load from manifest - use fallback
			DIA_LOG_WARNING("Application", "Falling back to code-defined application structure (manifest load failed)");
			pu = fallbackFactory();

			if (!pu)
			{
				DIA_LOG_ERROR("Application", "Fallback factory returned nullptr!");
			}
			else
			{
				DIA_LOG_INFO("Application", "Successfully created fallback application structure");
			}

			return pu;
		}
	}
}
