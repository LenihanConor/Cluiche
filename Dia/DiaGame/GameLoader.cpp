#include "GameLoader.h"
#include "GameFileComposer.h"
#include "DiaGameManifestLoader.h"

#include <DiaApplicationFlow/Manifest/ApplicationManifest.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifestLoader.h>
#include <DiaApplicationFlow/TypeRegistry/ApplicationTypeRegistry.h>
#include <DiaApplicationFlow/ApplicationProcessingUnit.h>
#include <DiaLogger/DiaLog.h>
#include <DiaCore/Core/Assert.h>

namespace Dia
{
	namespace Game
	{
		Dia::Application::ProcessingUnit* GameLoader::LoadFromGameFile(
			Dia::Application::ApplicationTypeRegistry& registry,
			const char* diagamePath,
			Dia::Application::ManifestValidationResult& outResult)
		{
			DIA_ASSERT(diagamePath != nullptr, "Game file path cannot be null");

			GameFileComposer composer;
			Dia::Application::ApplicationManifest manifest;
			DiaGameManifest gameManifest;
			outResult = composer.ComposeFromGameFile(diagamePath, manifest, gameManifest);
			if (outResult != Dia::Application::ManifestValidationResult::kSuccess)
			{
				DIA_LOG_ERROR("Game", "Failed to compose .diagame file: %s", diagamePath);
				return nullptr;
			}

			if (manifest.processingUnits.Size() == 0)
			{
				DIA_LOG_ERROR("Game", "Composed manifest has no processing units: %s", diagamePath);
				outResult = Dia::Application::ManifestValidationResult::kMissingRequiredField;
				return nullptr;
			}

			unsigned int rootIndex = 0;
			for (unsigned int i = 0; i < manifest.processingUnits.Size(); ++i)
			{
				if (manifest.processingUnits[i].root)
				{
					rootIndex = i;
					break;
				}
			}

			Dia::Application::ApplicationManifestLoader loader(registry);
			Dia::Application::ProcessingUnit* pu = loader.Instantiate(manifest.processingUnits[rootIndex]);
			if (!pu)
			{
				DIA_LOG_ERROR("Game", "Failed to instantiate ProcessingUnit from composed manifest: %s", diagamePath);
				outResult = Dia::Application::ManifestValidationResult::kUnknownType;
				return nullptr;
			}

			pu->Initialize();

			DIA_LOG_INFO("Game", "Successfully loaded application from .diagame: %s", diagamePath);
			return pu;
		}

		Dia::Application::ManifestValidationResult GameLoader::LoadGameManifest(
			const char* diagamePath,
			DiaGameManifest& outManifest)
		{
			DIA_ASSERT(diagamePath != nullptr, "Game file path cannot be null");
			return DiaGameManifestLoader::LoadGameFile(diagamePath, outManifest);
		}

		Dia::Application::ManifestValidationResult GameLoader::LoadStageManifest(
			const char* diastagePath,
			DiaStageManifest& outStage)
		{
			DIA_ASSERT(diastagePath != nullptr, "Stage file path cannot be null");
			return DiaGameManifestLoader::LoadStageFile(diastagePath, outStage);
		}
	}
}
