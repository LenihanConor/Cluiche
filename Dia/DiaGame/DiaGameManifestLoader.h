#pragma once

#include "DiaGameManifest.h"
#include <DiaApplicationFlow/Manifest/ManifestValidator.h>

namespace Dia
{
	namespace Game
	{
		class DiaGameManifestLoader
		{
		public:
			static Dia::Application::ManifestValidationResult LoadGameFile(const char* path, DiaGameManifest& outManifest);
			static Dia::Application::ManifestValidationResult LoadStageFile(const char* path, DiaStageManifest& outManifest);
		};
	}
}
