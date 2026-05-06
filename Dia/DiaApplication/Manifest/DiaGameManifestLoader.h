#pragma once

#include "DiaGameManifest.h"
#include "ManifestValidator.h"

namespace Dia
{
	namespace Application
	{
		class DiaGameManifestLoader
		{
		public:
			static ManifestValidationResult LoadGameFile(const char* path, DiaGameManifest& outManifest);
			static ManifestValidationResult LoadStageFile(const char* path, DiaStageManifest& outManifest);
		};
	}
}
