#pragma once

#include <DiaGame/DiaGameManifest.h>
#include <DiaApplicationFlow/Manifest/ApplicationManifest.h>
#include <DiaApplicationFlow/Manifest/ManifestValidator.h>

namespace Dia
{
	namespace Game
	{
		class GameFileComposer
		{
		public:
			Dia::Application::ManifestValidationResult ComposeFromGameFile(
				const char* diagamePath,
				Dia::Application::ApplicationManifest& outComposedManifest,
				DiaGameManifest& outGameManifest);
		};
	}
}
