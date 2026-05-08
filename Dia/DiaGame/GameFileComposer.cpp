#include "GameFileComposer.h"
#include "DiaGameManifestLoader.h"

#include <DiaApplication/Manifest/ManifestComposer.h>

namespace Dia
{
	namespace Game
	{
		Dia::Application::ManifestValidationResult GameFileComposer::ComposeFromGameFile(
			const char* diagamePath,
			Dia::Application::ApplicationManifest& outComposedManifest,
			DiaGameManifest& outGameManifest)
		{
			Dia::Application::ManifestValidationResult result =
				DiaGameManifestLoader::LoadGameFile(diagamePath, outGameManifest);

			if (result != Dia::Application::ManifestValidationResult::kSuccess)
				return result;

			Dia::Application::ManifestComposer composer;
			return composer.ComposeFromTypedImports(diagamePath, outGameManifest.imports, outComposedManifest);
		}
	}
}
