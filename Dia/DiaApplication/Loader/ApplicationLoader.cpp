#include "ApplicationLoader.h"

#include <DiaApplication/Manifest/ApplicationManifestLoader.h>
#include <DiaApplication/Manifest/ManifestComposer.h>
#include <DiaApplication/Manifest/DiaGameManifest.h>
#include <DiaApplication/Manifest/DiaGameManifestLoader.h>
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

		// -----------------------------------------------------------------------------
		// LoadApplicationTree
		// -----------------------------------------------------------------------------
		ProcessingUnit* ApplicationLoader::LoadApplicationTree(ApplicationTypeRegistry& registry,
															   const char* manifestPath,
															   ManifestValidationResult& outResult)
		{
			DIA_ASSERT(manifestPath != nullptr, "Manifest path cannot be null");

			ApplicationManifestLoader loader(registry);

			ApplicationManifest manifest;
			outResult = loader.LoadFromFile(manifestPath, manifest);

			if (outResult != ManifestValidationResult::kSuccess)
			{
				DIA_LOG_ERROR("Application", "LoadApplicationTree: Failed to load manifest: %s", manifestPath);
				return nullptr;
			}

			if (manifest.processingUnits.Size() == 0)
			{
				DIA_LOG_ERROR("Application", "LoadApplicationTree: No processing units in manifest: %s", manifestPath);
				outResult = ManifestValidationResult::kMissingRequiredField;
				return nullptr;
			}

			// Validate exactly one root PU
			int rootIndex = -1;
			int rootCount = 0;
			for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
			{
				if (manifest.processingUnits[i].root)
				{
					rootIndex = static_cast<int>(i);
					++rootCount;
				}
			}

			if (rootCount == 0)
			{
				DIA_LOG_ERROR("Application", "LoadApplicationTree: No root PU declared in manifest: %s", manifestPath);
				outResult = ManifestValidationResult::kMissingRequiredField;
				return nullptr;
			}

			if (rootCount > 1)
			{
				DIA_LOG_ERROR("Application", "LoadApplicationTree: Multiple root PUs declared in manifest: %s", manifestPath);
				outResult = ManifestValidationResult::kDuplicateInstanceId;
				return nullptr;
			}

			// Instantiate all PUs
			Dia::Core::Containers::DynamicArrayC<ProcessingUnit*, 4> instantiatedPUs;
			for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
			{
				ProcessingUnit* pu = loader.Instantiate(manifest.processingUnits[i]);
				if (!pu)
				{
					DIA_LOG_ERROR("Application", "LoadApplicationTree: Failed to instantiate PU index %u", i);
					// Clean up already-created PUs
					for (unsigned int j = 0; j < instantiatedPUs.Size(); ++j)
						delete instantiatedPUs[j];
					outResult = ManifestValidationResult::kUnknownType;
					return nullptr;
				}
				instantiatedPUs.Add(pu);
			}

			ProcessingUnit* rootPU = instantiatedPUs[static_cast<unsigned int>(rootIndex)];

			// Build parent-child relationships
			for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
			{
				if (static_cast<int>(i) == rootIndex)
					continue;

				const auto& entry = manifest.processingUnits[i];
				ProcessingUnit* childPU = instantiatedPUs[i];

				ProcessingUnit* parentPU = nullptr;
				if (entry.parentInstanceId != Dia::Core::StringCRC::kZero)
				{
					parentPU = rootPU->FindProcessingUnitInTree(entry.parentInstanceId);
				}

				if (parentPU == nullptr)
					parentPU = rootPU;

				Dia::Core::UniquePtr<ProcessingUnit> owned(childPU);
				if (!parentPU->AddChildProcessingUnit(std::move(owned)))
				{
					DIA_LOG_ERROR("Application", "LoadApplicationTree: Failed to add child PU '%s' to parent",
						entry.instanceId.AsChar());
					delete rootPU;
					outResult = ManifestValidationResult::kDuplicateInstanceId;
					return nullptr;
				}
			}

			rootPU->Initialize();

			DIA_LOG_INFO("Application", "Successfully loaded application tree from manifest: %s", manifestPath);
			return rootPU;
		}

		// -----------------------------------------------------------------------------
		// LoadFromGameFile
		// -----------------------------------------------------------------------------
		ProcessingUnit* ApplicationLoader::LoadFromGameFile(ApplicationTypeRegistry& registry,
														   const char* diagamePath,
														   ManifestValidationResult& outResult)
		{
			DIA_ASSERT(diagamePath != nullptr, "Game file path cannot be null");

			// Compose the full manifest including stages — this extends module phaseIds
			// so that modules are wired to stage phases at instantiation time.
			ManifestComposer composer;
			ApplicationManifest manifest;
			DiaGameManifest gameManifest;
			outResult = composer.ComposeFromGameFile(diagamePath, manifest, gameManifest);
			if (outResult != ManifestValidationResult::kSuccess)
			{
				DIA_LOG_ERROR("Application", "Failed to compose .diagame file: %s", diagamePath);
				return nullptr;
			}

			if (manifest.processingUnits.Size() == 0)
			{
				DIA_LOG_ERROR("Application", "Composed manifest has no processing units: %s", diagamePath);
				outResult = ManifestValidationResult::kMissingRequiredField;
				return nullptr;
			}

			// Find the root PU
			unsigned int rootIndex = 0;
			for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
			{
				if (manifest.processingUnits[i].root)
				{
					rootIndex = i;
					break;
				}
			}

			ApplicationManifestLoader loader(registry);
			ProcessingUnit* pu = loader.Instantiate(manifest.processingUnits[rootIndex]);
			if (!pu)
			{
				DIA_LOG_ERROR("Application", "Failed to instantiate ProcessingUnit from composed manifest: %s", diagamePath);
				outResult = ManifestValidationResult::kUnknownType;
				return nullptr;
			}

			pu->Initialize();

			DIA_LOG_INFO("Application", "Successfully loaded application from .diagame: %s", diagamePath);
			return pu;
		}

		// -----------------------------------------------------------------------------
		// LoadGameManifest
		// -----------------------------------------------------------------------------
		ManifestValidationResult ApplicationLoader::LoadGameManifest(const char* diagamePath,
																	 DiaGameManifest& outManifest)
		{
			DIA_ASSERT(diagamePath != nullptr, "Game file path cannot be null");
			return DiaGameManifestLoader::LoadGameFile(diagamePath, outManifest);
		}

		// -----------------------------------------------------------------------------
		// LoadStageManifest
		// -----------------------------------------------------------------------------
		ManifestValidationResult ApplicationLoader::LoadStageManifest(const char* diastagePath,
																	  DiaStageManifest& outStage)
		{
			DIA_ASSERT(diastagePath != nullptr, "Stage file path cannot be null");
			return DiaGameManifestLoader::LoadStageFile(diastagePath, outStage);
		}
	}
}
