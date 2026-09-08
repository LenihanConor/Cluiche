#include "DiaGameManifestLoader.h"
#include "JsonDiaGameSerializer.h"
#include "JsonDiaStageSerializer.h"

namespace Dia
{
	namespace Game
	{
		bool DiaGameManifestLoader::LoadGameFile(const char* path, DiaGameManifest& outManifest)
		{
			JsonDiaGameSerializer serializer;
			auto result = serializer.LoadFromFile(path, outManifest);
			return static_cast<bool>(result);
		}

		bool DiaGameManifestLoader::LoadStageFile(const char* path, DiaStageManifest& outManifest)
		{
			JsonDiaStageSerializer serializer;
			auto result = serializer.LoadFromFile(path, outManifest);
			return static_cast<bool>(result);
		}
	}
}
