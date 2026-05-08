#pragma once

#include <DiaGame/DiaGameManifest.h>
#include <DiaApplication/Manifest/ApplicationManifest.h>
#include <DiaApplication/Manifest/ManifestValidator.h>

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
