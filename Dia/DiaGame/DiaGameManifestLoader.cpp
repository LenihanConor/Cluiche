#include "DiaGameManifestLoader.h"
#include "JsonDiaGameSerializer.h"
#include "JsonDiaStageSerializer.h"

#include <cstring>

namespace Dia
{
	namespace Game
	{
		Dia::Application::ManifestValidationResult DiaGameManifestLoader::LoadGameFile(const char* path, DiaGameManifest& outManifest)
		{
			JsonDiaGameSerializer serializer;
			auto result = serializer.LoadFromFile(path, outManifest);

			if (!result)
			{
				if (result.error && strcmp(result.error, "file read error") == 0)
					return Dia::Application::ManifestValidationResult::kImportNotFound;
				return Dia::Application::ManifestValidationResult::kInvalidJSON;
			}

			return Dia::Application::ManifestValidationResult::kSuccess;
		}

		Dia::Application::ManifestValidationResult DiaGameManifestLoader::LoadStageFile(const char* path, DiaStageManifest& outManifest)
		{
			JsonDiaStageSerializer serializer;
			auto result = serializer.LoadFromFile(path, outManifest);

			if (!result)
			{
				if (result.error && strcmp(result.error, "file read error") == 0)
					return Dia::Application::ManifestValidationResult::kImportNotFound;
				if (result.error && strstr(result.error, "missing required field") != nullptr)
					return Dia::Application::ManifestValidationResult::kMissingRequiredField;
				return Dia::Application::ManifestValidationResult::kInvalidJSON;
			}

			return Dia::Application::ManifestValidationResult::kSuccess;
		}
	}
}
