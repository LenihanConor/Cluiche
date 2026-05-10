#pragma once

#include <DiaGame/DiaGameManifest.h>
#include <DiaApplicationFlow/Manifest/ManifestValidator.h>

namespace Dia
{
	namespace Application
	{
		class ProcessingUnit;
		class ApplicationTypeRegistry;
	}

	namespace Game
	{
		class GameLoader
		{
		public:
			static Dia::Application::ProcessingUnit* LoadFromGameFile(
				Dia::Application::ApplicationTypeRegistry& registry,
				const char* diagamePath,
				Dia::Application::ManifestValidationResult& outResult);

			static Dia::Application::ManifestValidationResult LoadGameManifest(
				const char* diagamePath,
				DiaGameManifest& outManifest);

			static Dia::Application::ManifestValidationResult LoadStageManifest(
				const char* diastagePath,
				DiaStageManifest& outStage);

		private:
			GameLoader() = delete;
		};
	}
}
